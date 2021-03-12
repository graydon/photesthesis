// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <fstream>
#include <iostream>
#include <photesthesis/3rdparty/xxhash64.h>
#include <photesthesis/corpus.h>
#include <photesthesis/util.h>
#include <stdexcept>
#include <string>
#include <tuple>

namespace photesthesis
{

#pragma region // Plan

Plan::Plan(TestName tname) : mTestName(tname)
{
}

Plan::Plan(TestName tname, Params const& params)
    : mTestName(tname), mParams(params)
{
}

void
Plan::addToHash(XXHash64& h) const
{
    addSymbolToHash(h, mTestName);
    addStringToHash(h, ":");
    for (auto const& pair : mParams)
    {
        addKeyValueToHash(h, pair.first, pair.second);
    }
}

uint64_t
Plan::getHashCode() const
{
    XXHash64 h{0};
    addToHash(h);
    return h.hash();
}

TestName
Plan::getTestName() const
{
    return mTestName;
}

ParamSpecs
Plan::getParamSpecs() const
{
    ParamSpecs specs;
    for (auto const& pair : mParams)
    {
        specs.emplace(pair.first, headSymbol(pair.second));
    }
    return specs;
}

void
Plan::addParam(ParamName p, Value v)
{
    mParams.emplace(p, v);
}

Value
Plan::getParam(ParamName p) const
{
    auto i = mParams.find(p);
    if (i == mParams.end())
    {
        throw std::runtime_error(std::string("unknown param: ") +
                                 p.getString());
    }
    return i->second;
}

bool
Plan::operator<(Plan const& other) const
{
    return std::tie(mTestName, mParams) <
           std::tie(other.mTestName, other.mParams);
}

bool
Plan::operator==(Plan const& other) const
{
    return std::tie(mTestName, mParams) ==
           std::tie(other.mTestName, other.mParams);
}

bool
Transcript::operator<(Transcript const& other) const
{
    return std::tie(mPlan, mVars) < std::tie(other.mPlan, other.mVars);
}

bool
Transcript::operator==(Transcript const& other) const
{
    return std::tie(mPlan, mVars) == std::tie(other.mPlan, other.mVars);
}

std::ostream&
operator<<(std::ostream& os, const Plan& plan)
{
    for (auto const& pair : plan.mParams)
    {
        os << "param: " << pair.first << " = " << pair.second << std::endl;
    }
    return os;
}

std::istream&
operator>>(std::istream& is, Plan& plan)
{
    plan.mParams.clear();
    scanWhitespace(is);
    while (is.good() && is.peek() == 'p')
    {
        ParamName pname("");
        Value val;
        std::string PARAM, EQ;
        is >> PARAM >> pname >> EQ >> val;
        expectStr(is, "param:", PARAM);
        expectStr(is, "=", EQ);
        expectNonemptyStr(is, pname.getString());
        plan.addParam(pname, val);
        scanWhitespace(is);
    }
    return is;
}

#pragma endregion // Plan

#pragma region // Transcript

Transcript::Transcript() : mPlan(TestName{""})
{
}

Transcript::Transcript(TestName tn) : mPlan(tn)
{
}

Transcript::Transcript(Plan const& plan) : mPlan(plan)
{
}

TestName
Transcript::getTestName() const
{
    return mPlan.getTestName();
}

Plan const&
Transcript::getPlan() const
{
    return mPlan;
}

void
Transcript::addTrackedVar(VarName var, Value val)
{
    mVars.emplace_back(var, val, true);
}

void
Transcript::addCheckedVar(VarName var, Value val)
{
    mVars.emplace_back(var, val, false);
}

std::vector<std::tuple<VarName, Value, bool>> const&
Transcript::getVars() const
{
    return mVars;
}

void
Transcript::clearVars()
{
    mVars.clear();
}
std::ostream&
operator<<(std::ostream& os, const Transcript& transcript)
{
    os << "#### transcript: " << transcript.getTestName() << " 0x" << std::hex
       << transcript.getPlan().getHashCode() << std::dec << std::endl;
    os << transcript.mPlan;
    for (auto const& triple : transcript.mVars)
    {
        if (std::get<2>(triple))
        {
            os << "track: " << std::get<0>(triple) << " = "
               << std::get<1>(triple) << std::endl;
        }
        else
        {
            os << "check: " << std::get<0>(triple) << " = "
               << std::get<1>(triple) << std::endl;
        }
    }
    os << std::endl;
    return os;
}

std::istream&
operator>>(std::istream& is, Transcript& transcript)
{
    transcript = Transcript();
    std::string HASHES, TRANSCRIPT;
    TestName tname("");
    uint64_t plan_hash{0};
    is >> HASHES >> TRANSCRIPT >> tname >> std::hex >> plan_hash >> std::dec;
    expectNonemptyStr(is, tname.getString());
    Plan plan(tname);
    is >> plan;
    expectVal<int64_t>(is, plan_hash, plan.getHashCode());
    transcript = Transcript(plan);

    scanWhitespace(is);
    while (is.good() && (is.peek() == 'c' || is.peek() == 't'))
    {
        std::string KW, EQ;
        VarName vname("");
        Value val;
        is >> KW >> vname >> EQ >> val;
        expectStr(is, "=", EQ);
        expectNonemptyStr(is, vname.getString());
        if (KW != "check:" && KW != "track:")
        {
            throw std::runtime_error(
                std::string("expecting either 'check:' or 'track:', got '") +
                KW + "' at offset " + std::to_string(is.tellg()));
        }
        bool tracked = (KW == "track:");
        transcript.mVars.emplace_back(vname, val, tracked);
        scanWhitespace(is);
    }
    return is;
}

#pragma endregion // Transcript

#pragma region // Corpus

Corpus::Corpus(std::string const& path, bool saveOnDestroy)
    : mPath(path), mSaveOnDestroy(saveOnDestroy), mDirty(false)
{
    if (!mPath.empty())
    {
        std::ifstream in(mPath);
        if (in.good())
        {
            in.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        }
        try {
            scanWhitespace(in);
            while (in.good())
            {
                Transcript trans;
                in >> trans;
                addTranscript(trans);
                scanWhitespace(in);
            }
        } catch (std::exception &e) {
            std::string msg("error parsing file '");
            msg += mPath;
            msg += "' at offset ";
            msg += std::to_string(in.tellg());
            msg += ": ";
            msg += std::string(e.what());
            throw std::runtime_error(msg);
        }
    }
    mDirty = false;
}

Corpus::~Corpus()
{
    if (mSaveOnDestroy)
    {
        save();
    }
}

void
Corpus::markDirty()
{
    mDirty = true;
}

void
Corpus::save()
{
    if (mDirty)
    {
        std::ofstream out(mPath, std::ios::out | std::ios::trunc);
        for (auto const& pair : mTranscripts)
        {
            for (auto const& t : pair.second)
            {
                out << t;
            }
        }
    }
}

std::set<Transcript>&
Corpus::getTranscripts(TestName tname)
{
    return mTranscripts[tname];
}

void
Corpus::addTranscript(Transcript const& ts)
{
    bool inserted = getTranscripts(ts.getTestName()).emplace(ts).second;
    assert(inserted);
    markDirty();
}

void
Corpus::updateTranscript(Transcript const& ts)
{
    auto& tss = getTranscripts(ts.getTestName());
    bool erased = false;
    for (auto i = tss.begin(); i != tss.end(); ++i)
    {
        if (i->getPlan() == ts.getPlan())
        {
            i = tss.erase(i);
            erased = true;
            break;
        }
    }
    assert(erased);
    bool inserted = tss.emplace(ts).second;
    assert(inserted);
    markDirty();
}

#pragma endregion // Corpus

} // namespace photesthesis

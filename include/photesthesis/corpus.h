#pragma once

// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "photesthesis/3rdparty/xxhash64.h"
#include <photesthesis/symbol.h>
#include <photesthesis/util.h>
#include <photesthesis/value.h>

#include <cstdint>
#include <map>
#include <random>
#include <tuple>

namespace photesthesis
{

using ParamName = Symbol;
using RuleName = Symbol;
using ParamSpecs = std::map<ParamName, RuleName>;
using Params = std::map<ParamName, Value>;
using Comment = std::string;
using Comments = std::vector<Comment>;
using TestName = Symbol;
using VarName = Symbol;
using PlanHash = uint64_t;
using Trajectory = uint64_t;

class Plan
{
    TestName mTestName;
    Comments mComments;
    Params mParams;

  public:
    Plan(TestName tname);
    Plan(TestName tname, Params const& params);
    Plan(TestName tname, Comments const& comments,
         Params const& params);

    void addToHash(XXHash64& h) const;
    PlanHash getHashCode() const;
    TestName getTestName() const;
    void addComment(Comment const& comment);
    Comments const& getComments() const;
    ParamSpecs getParamSpecs() const;
    void addParam(ParamName p, Value v);
    Value getParam(ParamName p) const;
    bool hasParam(ParamName p) const;
    bool operator==(Plan const& other) const;
    bool operator<(Plan const& other) const;
    friend std::ostream& operator<<(std::ostream& os, const Plan& plan);
    friend std::istream& operator>>(std::istream& is, Plan& plan);
};

std::ostream& operator<<(std::ostream& os, const Plan& plan);
std::istream& operator>>(std::istream& is, Plan& plan);

class Transcript
{
    Plan mPlan;
    std::vector<std::tuple<VarName, Value, bool>> mVars;
    friend std::ostream& operator<<(std::ostream& os,
                                    const Transcript& transcript);
    friend std::istream& operator>>(std::istream& is, Transcript& transcript);

  public:
    Transcript();
    Transcript(TestName tn);
    Transcript(Plan const& plan);
    TestName getTestName() const;
    Plan const& getPlan() const;
    void addTrackedVar(VarName var, Value val);
    void addCheckedVar(VarName var, Value val);
    std::vector<std::tuple<VarName, Value, bool>> const& getVars() const;
    void clearVars();
    bool operator<(Transcript const& other) const;
    bool operator==(Transcript const& other) const;
};

std::ostream& operator<<(std::ostream& os, const Transcript& transcript);
std::istream& operator>>(std::istream& is, Transcript& transcript);
class Corpus
{
    std::string mPath;
    bool mSaveOnDestroy;
    bool mDirty;
    std::map<TestName, std::set<Transcript>> mTranscripts;

  public:
    Corpus(std::string const& path = "", bool saveOnDestroy = true);
    ~Corpus();
    void markDirty();
    void save();
    std::set<Transcript>& getTranscripts(TestName tname);
    void addTranscript(Transcript const& ts);
    void updateTranscript(Transcript const& ts);
};
} // namespace photesthesis

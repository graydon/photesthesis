// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <cassert>
#include <cwctype>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <photesthesis/corpus.h>
#include <photesthesis/grammar.h>
#include <photesthesis/util.h>
#include <stdexcept>

namespace photesthesis
{

#pragma region // Atom

Atom::~Atom()
{
}

#pragma endregion // Atom

#pragma region // Lit

Lit::Lit(Value v) : mValue(v)
{
}

Lit::~Lit()
{
}

Value const&
Lit::getValue() const
{
    return mValue;
}

#pragma endregion // Lit

#pragma region // Ref

Ref::Ref(RuleName const& r, std::initializer_list<ParamName> ctxExt)
    : mRuleName(r), mCtxExt(ctxExt)
{
    static uint64_t sTag{0};
    mTag = sTag++;
}
Ref::~Ref()
{
}

RuleName const&
Ref::getRuleName() const
{
    return mRuleName;
};

uint64_t
Ref::getTag() const
{
    return mTag;
}

std::set<ParamName>
Ref::getCtxExt() const
{
    return mCtxExt;
}

#pragma endregion // Ref

#pragma region // Production

Production::Production(std::initializer_list<AtomPtr> atoms,
                       std::initializer_list<ParamName> req)
    : mAtoms(atoms), mCtxReq(req), mHasRefs(false)
{
    for (auto const& atom : mAtoms)
    {
        if (auto r = std::dynamic_pointer_cast<const Ref>(atom))
        {
            mHasRefs = true;
            break;
        }
    }
}
#pragma endregion // Production

#pragma region // Rule
Rule::Rule(std::initializer_list<Production> productions)
    : mProductions(productions)
{
}
#pragma endregion // Rule

#pragma region // Context

Context::Context(ParamSpecs const& params) : mGlobalParamSpecs(params)
{
}
void
Context::push(ParamName sym)
{
    mLocalParamNames.emplace_back(sym);
}
void
Context::push(std::set<ParamName> const& ps)
{
    for (auto const& p : ps)
    {
        push(ParamName(p));
    }
}
void
Context::pop(size_t n)
{
    while (n != 0)
    {
        mLocalParamNames.pop_back();
        n -= 1;
    }
}
bool
Context::has(ParamName const& s) const
{
    if (mGlobalParamSpecs.find(s) != mGlobalParamSpecs.end())
    {
        return true;
    }
    for (auto i = mLocalParamNames.rbegin(); i != mLocalParamNames.rend(); ++i)
    {
        if (*i == s)
        {
            return true;
        }
    }
    return false;
}
bool
Context::has(std::set<ParamName> const& ss) const
{
    for (auto const& s : ss)
    {
        if (!has(s))
        {
            return false;
        }
    }
    return true;
}

#pragma endregion // Context

#pragma region // Grammar

LitPtr
Grammar::Sym(Symbol const& s)
{
    return std::make_shared<const Lit>(Value(s));
}
LitPtr
Grammar::Bool(bool s)
{
    return std::make_shared<const Lit>(Value::Bool(s));
}
LitPtr
Grammar::Int64(int64_t i)
{
    return std::make_shared<const Lit>(Value::Int64(i));
}
LitPtr
Grammar::Blob(std::vector<uint8_t> const& s)
{
    return std::make_shared<const Lit>(Value(std::vector<uint8_t>(s)));
}
LitPtr
Grammar::Str(std::string const& s)
{
    return std::make_shared<const Lit>(Value(std::string(s)));
}

RefPtr
Grammar::Ref(RuleName const& s, std::initializer_list<ParamName> ctxExt)
{
    return std::make_shared<const class Ref>(s, ctxExt);
}

void
Grammar::addRule(RuleName const& name, std::initializer_list<Production> prods)
{
    if (mRules.find(name) != mRules.end())
    {
        throw std::runtime_error(std::string("duplicate rule addition: ") +
                                 name.getString());
    }
    mRules.emplace(name, Rule(prods));
    mRootRefs.emplace(name, Ref(name));
}

std::set<KPath>
Grammar::expandKPathPrefix(size_t k, KPath const& prefix, Context& context,
                           std::set<RefPtr>& pathRoots) const
{
    assert(k > 0);
    assert(!prefix.empty());
    if (prefix.size() == k)
    {
        return {prefix};
    }
    std::shared_ptr<const Atom> anchor = prefix.back();
    auto anchor_ref = std::dynamic_pointer_cast<const class Ref>(anchor);
    assert(anchor_ref);
    auto const& prods =
        getActiveProductions(anchor_ref->getRuleName(), k, context);
    std::set<KPath> res;

    for (auto const& prod : prods)
    {
        for (auto const& ext : prod.get().mAtoms)
        {
            auto ref = std::dynamic_pointer_cast<const class Ref>(ext);
            if (ref)
            {
                context.push(ref->getCtxExt());
            }
            // We only accept non-ref (i.e. literal) extensions at the last step
            // of a k-path. At earlier points in a k-path we require refs.
            if (ref || prefix.size() == k - 1)
            {
                KPath extended = prefix;
                extended.emplace_back(ext);
                auto expanded =
                    expandKPathPrefix(k, extended, context, pathRoots);
                res.insert(expanded.begin(), expanded.end());
            }
            // If we're at a ref we've not yet started-from, we also start
            // exploring a _new_ k-path starting from this ref.
            if (ref && pathRoots.find(ref) == pathRoots.end())
            {
                KPath restarted{ext};
                pathRoots.insert(ref);
                auto expanded =
                    expandKPathPrefix(k, restarted, context, pathRoots);
                res.insert(expanded.begin(), expanded.end());
            }
            if (ref)
            {
                context.pop(ref->getCtxExt().size());
            }
        }
    }
    return res;
}

std::vector<Production> const&
Grammar::getProductions(RuleName rule) const
{
    auto r = mRules.find(rule);
    if (r == mRules.end())
    {
        throw std::runtime_error(std::string("rule not found: ") +
                                 rule.getString());
    }
    auto const& prods = r->second.mProductions;
    if (prods.empty())
    {
        throw std::runtime_error(std::string("rule has no productions: ") +
                                 rule.getString());
    }
    return prods;
}

std::vector<std::reference_wrapper<Production const>>
Grammar::getActiveProductions(RuleName rule, size_t depthLimit,
                              Context const& context) const
{
    auto const& prods = getProductions(rule);
    std::vector<std::reference_wrapper<Production const>> activeProds;
    bool skippedDueToRefs = false;
    for (auto const& prod : prods)
    {
        if (depthLimit == 1 && prod.mHasRefs)
        {
            // We avoid descend into a production with a ref if we're at the
            // depth limit.
            skippedDueToRefs = true;
            continue;
        }
        if (context.has(prod.mCtxReq))
        {
            activeProds.emplace_back(prod);
        }
    }
    if (activeProds.empty())
    {
        if (skippedDueToRefs)
        {
            throw std::runtime_error(
                std::string("rule for ") + rule.getString() +
                std::string(" needs at least one nonterminal production"));
        }
        else
        {
            throw std::runtime_error(
                std::string("no active productions found for ") +
                rule.getString());
        }
    }
    return activeProds;
}
RefPtr
Grammar::getRootRef(RuleName rulename) const
{
    auto rp = mRootRefs.find(rulename);
    if (rp == mRootRefs.end())
    {
        throw std::runtime_error(std::string("unknown rule name: ") +
                                 rulename.getString());
    }
    return rp->second;
}

// A k-path is a sequence of exactly k symbolic nodes (terminals or
// nonterminals) connected in the direction of the edges in a
// graph-representation of the grammar.
std::set<KPath>
Grammar::generateKPathSet(size_t k, RuleName root,
                          ParamSpecs const& specs) const
{
    auto rootRef = getRootRef(root);
    std::set<RefPtr> pathRoots{rootRef};
    Context ctx(specs);
    return expandKPathPrefix(k, {rootRef}, ctx, pathRoots);
}

static std::set<std::vector<Value>>
extendByCycling(std::set<std::vector<Value>> const& vecs,
                std::set<Value> const& ext)
{
    assert(!vecs.empty());
    assert(!ext.empty());
    // std::cout << "building cycling extension of " << vecs.size() << " and "
    //          << ext.size() << " prefixes" << std::endl;
    std::set<std::vector<Value>> res;
    std::set<std::vector<Value>>::const_iterator i = vecs.begin();
    std::set<Value>::const_iterator j = ext.begin();
    bool cycledI = false, cycledJ = false;
    while (!(cycledI && cycledJ))
    {
        std::vector<Value> tmp = *i;
        tmp.emplace_back(*j);
        res.emplace(tmp);
        ++i;
        ++j;
        if (i == vecs.end())
        {
            cycledI = true;
            i = vecs.begin();
        }
        if (j == ext.end())
        {
            cycledJ = true;
            j = ext.begin();
        }
    }
    // std::cout << "cycling extension grew set to " << res.size() << "
    // prefixes"
    //           << std::endl;
    return res;
}

static std::set<Params>
extendByCycling(std::set<Params> const& params, ParamName param,
                std::set<Value> const& ext)
{
    assert(!params.empty());
    assert(!ext.empty());
    std::set<Params> res;
    std::set<Params>::const_iterator i = params.begin();
    std::set<Value>::const_iterator j = ext.begin();
    bool cycledI = false, cycledJ = false;
    while (!(cycledI && cycledJ))
    {
        Params tmp = *i;
        tmp.emplace(param, *j);
        res.emplace(tmp);
        ++i;
        ++j;
        if (i == params.end())
        {
            cycledI = true;
            i = params.begin();
        }
        if (j == ext.end())
        {
            cycledJ = true;
            j = ext.begin();
        }
    }
    return res;
}

/*
 * This function returns a pair of sets -- at least one of which is nonempty --
 * which, given a current `path = [... a, b, c]`, are expansions of the rule
 * named `c`. The first returned set of values are those that _are_
 * k-path-covering, and the second set (which will have either zero or one
 * values) can contain an element that _isn't_ k-path-covering, but if so it is
 * the smallest possible expansion of `c`.
 *
 * This works by accumulating 1 or more trees for each production `p` of `c`:
 *
 *   - Each atom `t_i` in `p` is checked against the `path` suffix, to see if
 *     [..., a, b, c, t_i] covers a k-path. If so, `p` is marked as
 *     k-path-covering.
 *
 *   - Each rule-ref atom `r_i` in `p` is separately expanded to a pair of
 *     `xc_i` and `xn_i` covering and non-covering expansions. If `xc_i` is
 *     nonempty then `p` is marked as k-path-covering and `xc_i` is used as the
 *     expansion for `r_i`, otherwise `xn_i` is used as the expansion for `r_i`.
 *
 *   - The expansions of all atoms are combined "cyclically" such that each
 *     atom-expansion occurs in at least one production-expansion, but without
 *     forming the full cartesian product of all atom-expansions.
 *
 *   - If the production was marked as k-path-covering by either of the two
 *     criteria above, its cyclical expansion is added to the first
 *     returned set, otherwise it's added to the second returned set.
 *
 *   - Finally once all productions are expanded, if the first set is nonempty
 *     the second set is emptied, and if not then the second set is reduced to
 *     only its smallest element. Both sets are returned.
 *
 * In other words: a call to this will always return at least 1 expansion, but
 * if no expansion is k-path-covering, it will return the smallest possible
 * non-covering expansion.
 */
std::pair<std::set<Value>, std::set<Value>>
Grammar::kPathCoveringOrMinimalExpansion(std::vector<RefPtr> const& path,
                                         size_t depthLimit, Context& context,
                                         size_t k, std::set<KPath>& paths) const
{
    std::set<Value> kPathCovering, nonKPathCovering;

    if (depthLimit == 0)
    {
        throw std::runtime_error("depth limit reached zero");
    }

    assert(!path.empty());
    assert(k > 0);
    KPath kpath;
    if (path.size() >= k - 1)
    {
        auto i = path.begin();
        std::advance(i, path.size() - (k - 1));
        kpath.insert(kpath.end(), i, path.end());
        assert(kpath.size() == k - 1);
    }

    RuleName rule = path.back()->getRuleName();
    auto prods = getActiveProductions(rule, depthLimit, context);

    for (auto prodref : prods)
    {
        auto const& prod = prodref.get();
        std::set<std::vector<Value>> prefixes{{Value(rule)}};
        bool productionCoversSomeKPath = false;

        for (auto const& atom : prod.mAtoms)
        {
            kpath.emplace_back(atom);
            if (paths.find(kpath) != paths.end())
            {
                // this production covers a k-path -- we need to keep at least
                // one expansion of it.
                paths.erase(kpath);
                kpath.pop_back();
                productionCoversSomeKPath = true;
                break;
            }
            kpath.pop_back();
        }

        for (auto const& atom : prod.mAtoms)
        {
            std::set<Value> atomExpansion;
            if (auto lit = std::dynamic_pointer_cast<const Lit>(atom))
            {
                atomExpansion.emplace(lit->getValue());
            }
            else if (auto ref =
                         std::dynamic_pointer_cast<const class Ref>(atom))
            {
                context.push(ref->getCtxExt());
                std::vector<RefPtr> subPath = path;
                subPath.emplace_back(ref);
                std::set<Value> subKPathCovering, subNonKPathCovering;
                std::tie(subKPathCovering, subNonKPathCovering) =
                    kPathCoveringOrMinimalExpansion(subPath, depthLimit - 1,
                                                    context, k, paths);
                context.pop(ref->getCtxExt().size());

                if (!subKPathCovering.empty())
                {
                    atomExpansion = subKPathCovering;
                    productionCoversSomeKPath = true;
                }
                else
                {
                    assert(subNonKPathCovering.size() == 1);
                    atomExpansion = subNonKPathCovering;
                }
            }
            else
            {
                throw std::logic_error("unknown subclass of Atom");
            }
            assert(!atomExpansion.empty());
            prefixes = extendByCycling(prefixes, atomExpansion);
        }
        if (productionCoversSomeKPath)
        {
            for (auto prefix : prefixes)
            {
                kPathCovering.emplace(Value(prefix));
            }
        }
        else
        {
            for (auto prefix : prefixes)
            {
                nonKPathCovering.emplace(Value(prefix));
            }
        }
    }
    if (!kPathCovering.empty())
    {
        nonKPathCovering.clear();
    }
    else if (nonKPathCovering.size() > 1)
    {
        auto i = nonKPathCovering.begin();
        std::advance(i, 1);
        nonKPathCovering.erase(i, nonKPathCovering.end());
    }
    assert(!(kPathCovering.empty() && nonKPathCovering.empty()));
    return std::make_pair(kPathCovering, nonKPathCovering);
}

std::set<Value>
Grammar::kPathCovering(RuleName rule, size_t k, ParamSpecs const& specs) const
{
    Context ctx(specs);
    std::set<KPath> paths = generateKPathSet(k, rule, specs);
    std::set<Value> res;
    size_t depthLimit = k;
    while (!paths.empty())
    {
        auto pair = kPathCoveringOrMinimalExpansion({getRootRef(rule)},
                                                    depthLimit, ctx, k, paths);
        if (pair.first.empty())
        {
            /*
            std::cout << "expanding depth limit from " << depthLimit << " to "
                      << (depthLimit + 1) << " with " << paths.size()
                      << " k-paths still uncovered: " << std::endl;
            for (auto const &kp : paths) {
              for (auto const &atom : kp) {
                if (auto r = std::dynamic_pointer_cast<const class Ref>(atom)) {
                  std::cout << ' ' << r->getRuleName() << '_' << r->getTag();
                } else if (auto l = std::dynamic_pointer_cast<const Lit>(atom))
            { std::cout << ' ' << l->getValue();
                }
              }
              std::cout << std::endl;
            }
            */
            depthLimit += 1;
        }
        else
        {
            res.insert(pair.first.begin(), pair.first.end());
        }
    }
    return res;
}

std::set<Params>
Grammar::kPathCoverings(size_t k, ParamSpecs const& specs) const
{
    std::set<Params> res;
    for (auto const& spec : specs)
    {
        std::set<Value> vals = kPathCovering(spec.second, k, specs);
        if (res.empty())
        {
            for (auto const& v : vals)
            {
                Params p;
                p.emplace(spec.first, v);
                res.emplace(p);
            }
        }
        else
        {
            // FIXME: we might want to do a cartesian product here instead of
            // cycling? Or a different N-tuples coverage? It could get
            // expensive. Tradeoffs...
            res = extendByCycling(res, spec.first, vals);
        }
    }
    return res;
}

// Returns a Value of type Pair (list) containing a fully-expanded production of
// `rule`. this might be an empty list (i.e. `(rule)` alone) if the rule has no
// active produtions.
Value
Grammar::randomValueFromRule(RuleName rule, std::default_random_engine& gen,
                             size_t depth_lim, Context& context) const
{
    if (depth_lim == 0)
    {
        throw std::runtime_error("depth limit reached zero");
    }

    auto prods = getActiveProductions(rule, depth_lim, context);
    std::vector<Value> vals{Value(rule)};
    if (!prods.empty())
    {
        auto& prod = pickUniform(gen, prods).get();
        for (auto atom : prod.mAtoms)
        {
            if (auto lit = std::dynamic_pointer_cast<const Lit>(atom))
            {
                vals.emplace_back(lit->getValue());
            }
            else if (auto ref =
                         std::dynamic_pointer_cast<const class Ref>(atom))
            {
                context.push(ref->getCtxExt());
                Value val = randomValueFromRule(ref->getRuleName(), gen,
                                                depth_lim - 1, context);
                assert(val.isPair());
                Value rest;
                assert(val.match(ref->getRuleName(), rest));
                vals.emplace_back(val);
                context.pop(ref->getCtxExt().size());
            }
            else
            {
                throw std::logic_error("unknown subclass of Atom");
            }
        }
    }
    return Value(vals);
}

Plan
Grammar::randomlyPopulatePlan(TestName tname, ParamSpecs const& params,
                              std::default_random_engine& gen,
                              size_t depth_lim) const
{
    Plan p(tname);
    for (auto const& pair : params)
    {
        Context ctx(params);
        Value v = randomValueFromRule(pair.second, gen, depth_lim, ctx);
        p.addParam(pair.first, v);
    }
    return p;
}

std::set<Plan>
Grammar::populatePlansFromKPathCoverings(TestName tname,
                                         ParamSpecs const& specs,
                                         size_t k) const
{
    std::set<Plan> res;
    std::set<Params> pset = kPathCoverings(k, specs);
    for (auto const& p : pset)
    {
        res.emplace(tname, p);
    }
    return res;
}

#pragma endregion // Grammar

} // namespace photesthesis

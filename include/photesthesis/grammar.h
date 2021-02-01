#pragma once

// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <photesthesis/corpus.h>
#include <photesthesis/value.h>
#include <random>
#include <string>
#include <vector>

namespace photesthesis {

// An Atom is a component of a Production in a Grammar: either a Lit (a.k.a.
// terminal) or a Ref (a.k.a. nonterminal).
class Atom {
public:
  virtual ~Atom();
};

// A Lit is one of two kinds of Atom. It contains a single "literal" Value.
class Lit : public Atom {
  const Value mValue;

public:
  Lit(Value v);
  virtual ~Lit();
  Value const &getValue() const;
};

// A Ref is the other kind of Atom, besides Lit. It contains a reference to a
// named Rule in the Grammar, along with a tag number that differentiates it
// from other Refs to the same rule in the same Grammar. In other words, every
// occurrence of a Ref in a Grammar has an "identity" that counts as a different
// node when reasoning about k-paths.
//
// (In practice we actually use _pointer_ identity of Refs, and only keep the
// tag around for diagnostic formatting purposes.)
class Ref : public Atom {
  uint64_t mTag;
  const RuleName mRuleName;
  const std::set<ParamName> mCtxExt;

public:
  Ref(RuleName const &r, std::initializer_list<ParamName> ctxExt = {});
  virtual ~Ref();
  RuleName const &getRuleName() const;
  uint64_t getTag() const;
  std::set<ParamName> getCtxExt() const;
};

using AtomPtr = std::shared_ptr<const Atom>;
using LitPtr = std::shared_ptr<const Lit>;
using RefPtr = std::shared_ptr<const Ref>;

// A Production is one alternative in a given Rule. It holds a list of Atoms
// that make up the content of the alternative, as well as a set of parameter
// names that it can depend on the presence-of in its production context.
struct Production {
  const std::vector<AtomPtr> mAtoms;
  const std::set<ParamName> mCtxReq;
  bool mHasRefs{false};
  Production(std::initializer_list<AtomPtr> atoms,
             std::initializer_list<ParamName> req = {});
};

// A Rule (a.k.a. nonterminal) is a named set of productions that can be
// referenced elsewhere in a Grammar.
struct Rule {
  std::vector<Production> mProductions;
  Rule(std::initializer_list<Production> productions);
};

// A Context enables writing context-sensitive Productions in Grammars. The
// semantic content of a context is essentially a "set of named flags" and you
// can guard any given Production on the presence of one of those flags in the
// context it's being expanded in.
//
// Internally, a Context is a pair of ParamName-sets, one global and one local.
// The global part is just the key-set of the ParamSpecs map used to populate a
// Plan, and the local part is dynamically modified during grammar-node
// expansion with any local-context extension rules. Both can be used as guards
// for enabling context-sensitive rules in a Grammar.
class Context {
  ParamSpecs const &mGlobalParamSpecs;
  std::vector<ParamName> mLocalParamNames;

public:
  Context(ParamSpecs const &params);
  void push(ParamName sym);
  void push(std::set<ParamName> const &ps);
  void pop(size_t n);
  bool has(ParamName const &s) const;
  bool has(std::set<ParamName> const &ss) const;
};

// A k-path is a path of symbols through the grammar with exactly k elements.
// We use this to generate a certain form of grammar coverage.
using KPath = std::vector<AtomPtr>;

// A Grammar is a set of named Rules as well as a factory for handing out
// various types of Atom that populate Productions (and thus Rules). A Grammar
// also has methods to populate a Plan using one of two strategies: randomly,
// and using "k-paths coverage" (as defined in
// https://doi.org/10.1109/ASE.2019.00027).
class Grammar {
  std::map<RuleName, Rule> mRules;
  std::map<RuleName, RefPtr> mRootRefs;

  // Return the root Ref for a given rule, throwing if it does not exist.
  RefPtr getRootRef(RuleName rulename) const;

  // Return all the productions for a given rule.
  std::vector<Production> const &getProductions(RuleName rule) const;

  // Return all the productions _active_ for a given rule subject to a
  // depth limit and current context.
  std::vector<std::reference_wrapper<Production const>>
  getActiveProductions(RuleName rule, size_t depth_lim,
                       Context const &ctx) const;

  // Return a random Vaule produced by a given rule with a given depth limit
  // and Context.
  Value randomValueFromRule(RuleName rule, std::default_random_engine &gen,
                            size_t depthLimit, Context &context) const;

  // Return the set of k-paths starting from `prefix`.
  std::set<KPath> expandKPathPrefix(size_t k, KPath const &prefix,
                                    Context &context,
                                    std::set<RefPtr> &pathRoots) const;

  // Generate a k-path set from the given rule, in a given ParamSpecs
  // environment.
  std::set<KPath> generateKPathSet(size_t k, RuleName root,
                                   ParamSpecs const &specs) const;

  // Helper function in calculating k-path covering, see implementation for
  // details.
  std::pair<std::set<Value>, std::set<Value>>
  kPathCoveringOrMinimalExpansion(std::vector<RefPtr> const &path,
                                  size_t depthLimit, Context &context, size_t k,
                                  std::set<KPath> &paths) const;

  // Generate a k-path set _covering_ from a given rule, in a given ParamSpecs
  // environment.
  std::set<Value> kPathCovering(RuleName rule, size_t k,
                                ParamSpecs const &params) const;

  // Generate a set of k-path coverings for all the params in a given
  // ParamSpecs.
  std::set<Params> kPathCoverings(size_t k, ParamSpecs const &specs) const;

public:
  RefPtr Ref(RuleName const &, std::initializer_list<ParamName> ctxExt = {});
  LitPtr Sym(Symbol const &);
  LitPtr Bool(bool);
  LitPtr Int64(int64_t);
  LitPtr Blob(std::vector<uint8_t> const &);
  LitPtr Str(std::string const &);
  void addRule(RuleName const &name, std::initializer_list<Production> prods);

  Plan randomlyPopulatePlan(TestName tname, ParamSpecs const &params,
                            std::default_random_engine &gen,
                            size_t depthLimit) const;

  std::set<Plan> populatePlansFromKPathCoverings(TestName tname,
                                                 ParamSpecs const &specs,
                                                 size_t k) const;
};

} // namespace photesthesis

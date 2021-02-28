#pragma once

// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "photesthesis/3rdparty/xxhash64.h"
#include <photesthesis/corpus.h>
#include <photesthesis/grammar.h>
#include <photesthesis/value.h>

namespace photesthesis {
class Test {

  Grammar const &mGram;
  Corpus &mCorp;
  std::default_random_engine mGen;
  bool mFailed{false};
  uint64_t mVerboseLevel{0};

  XXHash64 mTrajHasher{0};
  Trajectory mTrajectory{0};
  Transcript mTranscript;

  using Trajectories = std::map<Trajectory, Transcript>;
  using Failures = std::vector<PlanHash>;

  Failures initializeCorpusFromKPaths(uint64_t kPathLength);
  Failures randomlyExpandCorpus(Trajectories &, uint64_t steps, uint64_t depth);
  Failures checkCorpus(Trajectories &);
  void checkTranscript(Transcript const &);
  void runPlan(Plan const &);
  bool runPlanAndMaybeExpandCorpus(Plan const &, Trajectories &);
  void reportFailures(Failures const &) const;

protected:
  void initTrajectory();
  void finiTrajectory();

  Value getParam(ParamName p) { return mTranscript.getPlan().getParam(p); }

  // Calling `invariant()` indicates a value you expect to be invariant across
  // all executions (so is neither relevant to trajectories nor transcripts).
  // If a value does not meet expectations, it will trigger a call to
  // handle_invariant_failure which by default prints the plan and mismatched
  // values.
  void invariant(VarName, Value expected, Value got);

  // Calling `trace()` on a given value adds it to the hashed state of the
  // test's trajectory (which also includes any path-coverage information we can
  // observe, if compiled with instrumentation). Test runs are grouped by
  // trajectory, and mutation happens between trajectories to attempt to find
  // new ones.
  //
  // Mnemonic: TRAced values contribute to TRAjectories.
  void trace(VarName, Value seen);

  // Calling `check()` on a given value records the value to the transcript
  // and/or checks that the value is the same as the corresponding value
  // transcribed in a previous run, but does not `trace()` the value.
  //
  // Mnemonic: checks can fail, and failures are reported.
  void check(VarName, Value seen);

  // Calling `track()` on a given value records it to the transcript for
  // checking _and_ traces it. It is equivalent to calling `trace` and `check`
  // except the word 'track' is put in the transcript instead of 'check', which
  // emphasizes the trajectory-sensitivity to anyone reading the transcript.
  //
  // Mnemonic: TRACK = TRAce + cheCK
  void track(VarName, Value seen);

public:
  const std::vector<ParamSpecs> mSeedSpecs;

  Test(Grammar const &gram, Corpus &corp, TestName testName,
       std::vector<ParamSpecs> const &seedSpecs);

  // Seed the PRNG used in random decisions using the system random number
  // device. If not seeded with this function or seed_specific, it will be
  // seeded with zero.
  void seedFromRandomDevice();

  // Seed the PRNG used in random decisions using a specific value. If not
  // seeded with this function or seed_urandom, it will be seeded with zero.
  void seedWithValue(uint64_t seed);

  // Entrypoint for clients. Checks and/or grows a corpus.
  //
  // If `expansionSteps` or the env var `PHOTESTHESIS_EXPANSION_STEPS` is
  // nonzero, `administer` will expand the corpus (initially using K-paths
  // coverage, later using random generation).
  //
  // If the environment variable `PHOTESTHESIS_TEST_HASH` is set to some number,
  // only the transcript labeled with that number will be checked.
  //
  // The corpus will be re-written if any checks fail or the corpus is expanded.
  //
  // `administer` returns a vector of PlanHashes that identify any transcripts
  // that failed.
  //
  // If you want to get a useful and informative test failure signal at an outer
  // level test harness, assert that the return value of this is equal to the
  // empty vector.
  std::vector<PlanHash> administer(uint64_t expansionSteps = 0,
                                   uint64_t kPathLength = 3,
                                   uint64_t randomDepth = 3);

  // You must override `run` in your own subclasses -- this runs your test!
  virtual void run() = 0;

  // You may (but need not) override these handler callbacks to treat errors
  // specially. By default they print information about failures to stdout
  // if run with PHOTESTHESIS_VERBOSE, otherwise they do nothing.
  virtual void handleTranscriptMismatch(Transcript const &expected,
                                        Transcript const &got);
  virtual void handleInvariantFailure(Plan const &plan, VarName varname,
                                      Value expected, Value got);
};
} // namespace photesthesis

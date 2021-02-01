// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <cstdint>
#include <cstring>
#include <iostream>
#include <photesthesis/3rdparty/xxhash64.h>
#include <photesthesis/corpus.h>
#include <photesthesis/grammar.h>
#include <photesthesis/test.h>
#include <photesthesis/util.h>
#include <random>
#include <sstream>

namespace {
uint8_t *gCov8BitStart{nullptr};
size_t gCov8BitLen{0};

bool getEnvNum(char const *evar, uint64_t &num) {
  if (auto *p = std::getenv(evar)) {
    num = std::strtoull(p, nullptr, 0);
    return true;
  }
  return false;
}

bool getEnvExpansionSteps(uint64_t &expansionSteps) {
  return getEnvNum("PHOTESTHESIS_EXPANSION_STEPS", expansionSteps);
}

bool getEnvKPathLength(uint64_t &kPathLen) {
  return getEnvNum("PHOTESTHESIS_KPATH_LENGTH", kPathLen);
}

bool getEnvRandomDepth(uint64_t &randomDepth) {
  return getEnvNum("PHOTESTHESIS_RANDOM_DEPTH", randomDepth);
}

bool getEnvVerbose(uint64_t &verbose) {
  return getEnvNum("PHOTESTHESIS_VERBOSE", verbose);
}

bool getEnvTestHash(uint64_t &testHash) {
  return getEnvNum("PHOTESTHESIS_TEST_HASH", testHash);
}

bool getEnvRandomSeed(uint64_t &seed) {
  return getEnvNum("PHOTESTHESIS_RANDOM_SEED", seed);
}

} // namespace

extern "C" {
__attribute__((visibility("default"))) void
__sanitizer_cov_8bit_counters_init(uint8_t *Start, uint8_t *Stop) {
  gCov8BitStart = Start;
  gCov8BitLen = Stop - Start;
}

__attribute__((visibility("default"))) void
__sanitizer_cov_pcs_init(const uintptr_t *pcs_beg, const uintptr_t *pcs_end) {
  //...
}
}

namespace photesthesis {

void Test::finiTrajectory() {
  if (gCov8BitLen != 0) {
    mTrajHasher.add(gCov8BitStart, gCov8BitLen);
  }
  mTrajectory = mTrajHasher.hash();
}

void Test::initTrajectory() {
  mTrajectory = 0;
  mTrajHasher = XXHash64(0);
  if (gCov8BitLen != 0) {
    std::memset(gCov8BitStart, 0, gCov8BitLen);
  }
}

void Test::seedFromRandomDevice() {
  std::random_device dev;
  mGen.seed(dev());
}

void Test::seedWithValue(uint64_t seed) { mGen.seed(seed); }

void Test::runPlan(Plan const &plan) {
  mFailed = false;
  mTranscript = Transcript(plan);
  initTrajectory();
  run();
  finiTrajectory();
}

bool Test::runPlanAndMaybeExpandCorpus(Plan const &plan,
                                       Trajectories &trajectories) {
  auto tname = plan.getTestName();
  runPlan(plan);
  if (trajectories.find(mTrajectory) == trajectories.end() &&
      mCorp.getTranscripts(tname).find(mTranscript) ==
          mCorp.getTranscripts(tname).end()) {
    if (mVerboseLevel > 1) {
      std::cout << "novel trajectory found: " << std::endl;
      std::cout << mTranscript;
    }
    trajectories.emplace(mTrajectory, mTranscript);
    mCorp.addTranscript(mTranscript);
    return true;
  }
  return false;
}

void Test::reportFailures(Failures const &failures) const {
  if (mVerboseLevel > 0)
    if (!failures.empty()) {
      std::cout << "failing test hashes: ";
      bool first = true;
      for (auto f : failures) {
        if (first) {
          first = false;
        } else {
          std::cout << ", ";
        }
        std::cout << std::hex << f << std::dec;
      }
      std::cout << std::endl;
    }
}

Test::Failures Test::initializeCorpusFromKPaths(uint64_t kPathLength) {
  TestName tname = mTranscript.getTestName();
  Trajectories trajectories;
  Failures failures;
  if (mVerboseLevel > 0) {
    std::cout << "generating initial " << kPathLength
              << "-paths for test: " << tname;
  }
  size_t nPlans = 0;
  for (auto const &spec : mSeedSpecs) {
    for (auto const &plan : mGram.populatePlansFromKPathCoverings(
             tname, spec, static_cast<size_t>(kPathLength))) {
      ++nPlans;
      runPlanAndMaybeExpandCorpus(plan, trajectories);
      if (mFailed) {
        failures.emplace_back(plan.getHashCode());
      }
    }
  }
  if (mVerboseLevel > 0) {
    std::cout << "generated " << nPlans << " initial plans with "
              << trajectories.size() << " trajectories for test: " << tname;
    reportFailures(failures);
  }
  return failures;
}

Test::Failures
Test::checkCorpus(std::map<Trajectory, Transcript> &trajectories) {
  TestName tname = mTranscript.getTestName();
  auto &transcripts = mCorp.getTranscripts(tname);
  if (transcripts.empty()) {
    return {};
  }
  Failures failures;
  if (mVerboseLevel > 0) {
    std::cout << "checking " << transcripts.size() << " transcripts for test "
              << tname << std::endl;
  }
  uint64_t specificHash = 0;
  bool limitToHash = getEnvTestHash(specificHash);
  for (auto const &ts : transcripts) {
    if (limitToHash && ts.getPlan().getHashCode() != specificHash) {
      continue;
    }
    checkTranscript(ts);
    if (mFailed) {
      failures.emplace_back(ts.getPlan().getHashCode());
    }
    trajectories.emplace(mTrajectory, mTranscript);
  }
  if (mVerboseLevel > 0) {
    std::cout << "found " << trajectories.size() << " trajectories from "
              << transcripts.size() << " transcripts for test " << tname
              << std::endl;
    reportFailures(failures);
  }
  return failures;
}

Test::Failures Test::randomlyExpandCorpus(Trajectories &trajectories,
                                          uint64_t steps, uint64_t depth) {

  if (steps == 0) {
    return {};
  }
  size_t newTrajs = 0;
  auto tname = mTranscript.getTestName();
  Failures failures;
  if (mVerboseLevel > 0) {
    std::cout << "expanding corpus for test: " << tname << std::endl;
  }
  for (uint64_t i = 0; i < steps; ++i) {
    ParamSpecs spec;
    if (trajectories.empty()) {
      spec = pickUniform(mGen, mSeedSpecs);
    } else {
      spec = pickUniform(mGen, trajectories).second.getPlan().getParamSpecs();
    }
    Plan plan = mGram.randomlyPopulatePlan(tname, spec, mGen,
                                           static_cast<size_t>(depth));
    if (runPlanAndMaybeExpandCorpus(plan, trajectories)) {
      newTrajs++;
    };
    if (mFailed) {
      failures.emplace_back(plan.getHashCode());
    }
  }
  if (mVerboseLevel > 0) {
    std::cout << "explored " << steps << " random inputs at depth " << depth
              << ", expanded corpus by " << newTrajs << " to "
              << mCorp.getTranscripts(tname).size() << " distinct trajectories "
              << std::endl;
    reportFailures(failures);
  }
  return failures;
}

void Test::invariant(VarName vn, Value expected, Value got) {
  if (!(expected == got)) {
    mFailed = true;
    handleInvariantFailure(mTranscript.getPlan(), vn, expected, got);
  }
}

void Test::trace(VarName vn, Value seen) {
  addKeyValueToHash(mTrajHasher, vn, seen);
}

void Test::check(VarName vn, Value seen) {
  mTranscript.addCheckedVar(vn, seen);
}

void Test::track(VarName vn, Value seen) {
  trace(vn, seen);
  mTranscript.addTrackedVar(vn, seen);
}

Test::Test(Grammar const &gram, Corpus &corp, TestName testName,
           std::vector<ParamSpecs> const &seedSpecs)
    : mGram(gram), mCorp(corp), mTranscript(testName), mSeedSpecs(seedSpecs) {
  getEnvVerbose(mVerboseLevel);
}

Test::Failures Test::administer(uint64_t expansionSteps, uint64_t kPathLength,
                                uint64_t randomDepth) {

  getEnvExpansionSteps(expansionSteps);
  getEnvKPathLength(kPathLength);
  getEnvRandomDepth(randomDepth);
  uint64_t randomSeed = 0;
  if (getEnvRandomSeed(randomSeed)) {
    seedWithValue(randomSeed);
  }

  TestName tname = mTranscript.getTestName();

  if (mCorp.getTranscripts(tname).empty()) {
    return initializeCorpusFromKPaths(kPathLength);
  } else {
    Trajectories trajectories;
    auto failures = checkCorpus(trajectories);
    if (!failures.empty()) {
      return failures;
    }
    return randomlyExpandCorpus(trajectories, expansionSteps, randomDepth);
  }
}

void Test::handleInvariantFailure(Plan const &plan, VarName varname,
                                  Value expected, Value got) {
  if (mVerboseLevel > 0) {
    std::cout << "invariant failed in test " << plan.getTestName() << " "
              << std::hex << plan.getHashCode() << std::dec << std::endl;
    std::cout << "  parameters:" << std::endl << plan << std::endl;
    std::cout << "  invariant: " << varname << std::endl;
    std::cout << "  expected:" << expected << std::endl;
    std::cout << "  got:" << got << std::endl;
  }
}

void Test::handleTranscriptMismatch(Transcript const &expected,
                                    Transcript const &got) {
  if (mVerboseLevel > 0) {
    std::cout << "transcript mismatch!" << std::endl;
    std::cout << "  expected:" << std::endl << expected << std::endl;
    std::cout << "  got:" << std::endl << got << std::endl;
  }
}

void Test::checkTranscript(Transcript const &ts) {
  runPlan(ts.getPlan());
  if (!(ts == mTranscript)) {
    handleTranscriptMismatch(ts, mTranscript);
    mCorp.updateTranscript(mTranscript);
  };
}

} // namespace photesthesis

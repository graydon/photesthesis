# Photesthesis

This is a small, experimental parameterized-testing tool.

It is intended to be used in concert with another unit-testing framework (eg.
[Catch2](https://en.wikipedia.org/wiki/QuickCheck)) for providing a `main()`
test harness and for reporting assertion failures.

It combines a mixture of techniques from [grammar-based
fuzzing](https://www.fuzzingbook.org/html/Grammars.html), [coverage-guided
fuzzing](https://llvm.org/docs/LibFuzzer.html), [property-based
testing](https://en.wikipedia.org/wiki/QuickCheck), and [approval
testing](https://approvaltests.com/).

## Quick start

1. Define a `Grammar` and call `Grammar::addRule` to model possible inputs to
   your test. These are _abstract_ rules that will construct an _abstract input_
   to your test. See section below on _abstract grammar_.
2. Define a subclass of `photesthesis::Test` that contains your parameterized
   test. This should call `Test::getParam()` to extract abstract parameters and
   then `Value::match()` those parameters to extract them into concrete
   C++-typed values, and should call `Test::check()`, `Test::trace()` or
   `Test::track()` to observe variables during the test, and `Test::invariant()`
   for any properties expected to be invariant over all executions. See section
   below on _observed values and trajectories_.
3. Construct a `Corpus` which is an object that manages a file that will hold
   transcripts (parameters and observed variables) of your test. This file will
   be checked in to your revision control. A single corpus file can hold
   transcripts for multiple named tests.
4. Make a test in your _outer_ test harness (eg. Catch2), and in it construct an
   instance of your `Test` subclass given the provided corpus and grammar, then
   call `Test::administer()`.
5. Optionally: compile your SUT and test code (but not photesthesis itself)
   using `-fsanitize-coverage=inline-8bit-counters`: this will cause
   photesthesis to record approximate _path coverage_ of the SUT as part of the
   value of each run's trajectory.

## Example

See [test/test_photesthesis.cpp](test/test_photesthesis.cpp).

## Theory of operation

Photesthesis has a main entrypoint `Test::administer` that will perform some
mixture of three tasks:

  - **Initializing** a corpus. If you don't have a corpus yet (or rather: any
    entries in the corpus for the named test you're running) it will build one
    using a K-path-covering set (https://doi.org/10.1109/ASE.2019.00027)
    generated from the grammar, run the test on each element of the set, and
    save the corpus for reuse in the future.

  - **Checking** an existing corpus. This re-runs only the (small) set of
    transcripts stored in the corpus and compares the run's checked (or tracked)
    variables to those stored in the transcript. Re-run transcripts are written
    back to the corpus file on disk if anything changed, in a deterministic
    position and in human-readable, textual form, so that any changed
    observations can be inspected and approved.

  - **Expanding** a corpus by randomly generating new entries from the grammar
    (up to some depth limit) to attempt to find and record parameter values that
    trigger new trajectories in the SUT, recording the associated transcripts to
    the corpus.

The `Test::administer` function takes 3 arguments, all optional:

  - An expansion-step count argument, which is `0` by default, and can also be
    set through the environment variable `PHOTESTHESIS_EXPANSION_STEPS`. So by
    default photesthesis will initialize and re-check a corpus but not expand
    (fuzz) it.
  - A value `K` for the K-paths covering set, which is `3` by default, and can
    also be set through the environment variable `PHOTESTHESIS_KPATH_LENGTH`.
  - A depth-limit for randomly generated trees, which is `3` by default, and can
    also be set through the environment variable `PHOTESTHESIS_RANDOM_DEPTH`.

The expected usage is to run with the initial K-paths corpus while designing a
unit test, and then run it once with a fairly large expansion-step count to
establish a good extended corpus, that you save. Then _mostly_ re-run that saved
corpus (quickly) with a zero expansion-step count as part of your typical
unit-test runs. Periodically (as a test maintenance task, or in response to
major edits) you can re-run with a nonzero expansion-step count to see if there
are new uncovered trajectories.

## Observed values and trajectories

When a parameterized test runs, photesthesis makes observations and records two
separate sets of values about the test:

  - The **transcript** of the run, which includes the test name, the input
    parameter values chosen, and the canonical reference-values of any `checked`
    or `tracked` variables (see below). In other words, the values in the
    transcript indicate _expected outcomes of a given run_.

  - The **trajectory** of the run, which is a value used to group test runs
    together into equivalence classes in the corpus. Only one transcript is
    maintained per trajectory when expanding the corpus. In other words, the
    values in the trajectory indicate _which transcripts are meaningfully
    different_.

Trajectories are hashed into a single uint64_t, which incorporates any
variable marked as trajectory-relevant by the test using the `Test::trace` (or
`Test::track`) methods, as well as (optionally, using LLVM instrumentation) an
approximate measure of variation in path-coverage counters from the SUT.

Note: if you're using path-coverage, you may not need to record any values with
`track` or `trace`; but they can still be useful to subdivide trajectory
classes.

Observations made by the test of its own state thus fall into 4 natural categories:

  - **Invariants** are those values (like properties in property testing) that
    you expect to be invariant over _all_ executions. They are not recorded in
    transcripts, not considered part of trajectories, but if expected invariants
    are violated photesthesis will call `Test::handleInvariantFailure` with
    the parameters that caused the violation, and report the triggering inputs
    as failures.

  - **Traced** values are those that contribute to the trajectory, but that have
    specific values that aren't of interest to the transcript. For example, the
    path-coverage approximation is neither informative to a reader nor does it
    define correctness of a run, but it does differentiate runs. Similarly
    various hit-rates or other performance metrics might be worth tracing to
    differentiate runs, but don't define "correctness" per se.

  - **Checked** values are those that contribute to the transcript, but not the
    trajectory. They are values which represent a correctness check but which
    should not cause equivalence-class splitting for each possible value. For
    example, if an arithmetic identity holds, it might be worth checking but
    *not* tracing, to avoid retaining separate transcripts for every possible
    value of the identity.

  - **Tracked** values are those that are both checked and traced.

## Abstract grammar

Photesthesis is based on _abstract_ grammars. Meaning: it generates parameters
and observes variables of a single C++ type `Value`, which is an
[S-expression](https://en.wikipedia.org/wiki/S-expression)-like type with a few
subtypes that represent booleans, numbers, symbols, and lists.

Convenience `match` functions exist for pattern-matching various C++ types
against `Value` in order to extract concrete information used to parameterize
your tests, and reusable composite matching rules can be written by extending
the `Matcher<T>` type and overriding one of its `match` member functions.

Similarly, any concrete value you wish to observe as a variable (see below) you
will need to inject into the `Value` abstract domain. Again, there are
convenience methods provided but you might need to write few of your own for
structured types you observe in multiple tests.

There are two reasons for working in the abstract `Value` domain:
  - We wish to be able to serialize and deserialzie _human readable_ parameters
    and observations into test-run transcripts, to support the approval-style
    workflow.
  - We wish to be able to generate values from grammars which may not be
    unambiguously parseable in the concrete domain. We can guarantee trivial
    parse-ability of the abstract domain's Value type regardless of grammar (and
    even allow context-sensitive grammars in the abstract domain).

Additionally, this approach produces a smaller and simpler-to-debug library than
one that rests on a lot of complex C++ type-level trickery, and it also produces
a natural pattern-matching-conditional idiom for alternating between different
variants of a test's behaviour -- this would have to be accomplished with
separate booleans or test-feature flags otherwise.

## What is new here

Photesthesis offers no single new technology, but rather a new (and hopefully
useful) combination of existing techniques that have not (to this author's
knowledge) previously been combined:

  - Compared to many structured and random input-generation libraries (eg.
    property-based testers) it incorporates fuzzer-style path-coverage
    measurement of the SUT.

  - Compared to many fuzzers and property-based testers, it is grammar driven
    and therefore can more likely find inputs with syntactic and even deep
    semantic validity conditions.

  - Compared to concrete grammar testers, it performs generation in an abstract
    grammar (for reasons discussed above).

  - Compared to most fuzzers, it is designed to be embedded in unit test
    suites and to support a test-writer making fine-grained assertions and
    observations, not just opaque whole-program crash-fail semantics.

The closest system I am aware of in design is
[Nautilus](https://github.com/nautilus-fuzz/nautilus) but it differs in a few
key ways:
  - Doesn't support serializing and reloading corpus, approval-style workflow.
  - Lacks grammar-coverage-guided generation phase (k-paths generator).
  - Written in Rust and designed for use with subprocesses, hard to use as
    an embedded library in C++ unit tests.
  - Designed around AFL's instrumentation (Photesthesis uses LLVM's coverage
    sanitizer instrumentation, the same mode libfuzzer uses)

## Dependencies

This should build on any newish C++17-speaking C++ compiler, but it is only
currently tested on clang 11. It's depends on nothing outside its own source
tree and the C++ standard library.

## License

Photesthesis is Copyright 2021 Stellar Development Foundation, licensed
under the Apahce 2.0 license (ASL-2).

It includes a bundled copy of an xxhash implementation by Stephan Brumme,
licensed under ASL-2-compatible, zlib-like terms. The original can be found
here: https://github.com/stbrumme/xxhash/blob/master/xxhash64.h

## Name

"Photesthesis" means "light sensitivity", which is somewhat thematically related
to the [streetlight effect](https://en.wikipedia.org/wiki/Streetlight_effect) in
test coverage: "people only search for something where it is easiest to look".

It also contains the substring "test", and is a similar-sounding word to
"hypothesis", which is the name of [one of the better property-based test
tools](https://hypothesis.works/).

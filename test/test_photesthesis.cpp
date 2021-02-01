// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <iostream>
#include <photesthesis/corpus.h>
#include <photesthesis/grammar.h>
#include <photesthesis/test.h>
#include <photesthesis/value.h>
#include <sstream>

namespace ph = photesthesis;

const ph::RuleName EXPR{"expr"};
const ph::RuleName ADD{"add"};
const ph::RuleName SUB{"sub"};
const ph::RuleName MUL{"mul"};
const ph::RuleName LET{"let"};
const ph::RuleName VAR{"var"};
const ph::ParamName X{"x"};
const ph::ParamName N{"n"};
const ph::VarName RES{"res"};

class MyTest : public ph::Test {
public:
  MyTest(ph::Grammar const &gram, ph::Corpus &corp)
      : Test(gram, corp, ph::Symbol("MyTest"), {{{N, EXPR}}}) {}

  void checkRoundtrip(ph::Value const &val) {
    std::ostringstream oss;
    oss << val;
    ph::Value nval;
    std::istringstream iss(oss.str());
    iss >> nval;
    if (!(nval == val)) {
      std::cout << "deserialized doesn't match:\n  " << val << "\nvs.\n  "
                << nval << std::endl;
    }
  }

  ph::Value eval(std::map<ph::Symbol, ph::Value> env, ph::Value val) {
    ph::Value a, b, c;

    if (val.match(EXPR, a)) {
      if (a.match(ADD, b, c)) {
        ph::Value vb = eval(env, b);
        ph::Value vc = eval(env, c);
        int64_t ia{0}, ic{0};
        if (vb.match(ia) && vc.match(ic)) {
          return ph::Value::Int64(ia + ic);
        }
      }
      if (a.match(SUB, b, c)) {
        ph::Value vb = eval(env, b);
        ph::Value vc = eval(env, c);
        int64_t ia{0}, ic{0};
        if (vb.match(ia) && vc.match(ic)) {
          return ph::Value::Int64(ia - ic);
        }
      }

      if (a.match(MUL, b, c)) {
        ph::Value vb = eval(env, b);
        ph::Value vc = eval(env, c);
        int64_t ia{0}, ic{0};
        if (vb.match(ia) && vc.match(ic)) {
          return ph::Value::Int64(ia * ic);
        }
      }

      if (a.match(LET, X, b, c)) {
        std::map<ph::Symbol, ph::Value> newEnv = env;
        newEnv[X] = eval(env, b);
        return eval(newEnv, c);
      }

      if (a.match(VAR, X)) {
        return env[X];
      }

      if (a.isInt64()) {
        return a;
      }
    }
    return ph::Value::Int64(0);
  }

  void run() override {
    ph::Value val = getParam(N);
    checkRoundtrip(val);
    check(RES, eval({}, val));
  }
};

int main() {

  ph::Grammar gram;

  gram.addRule(ADD, {{gram.Int64(0)}, {gram.Ref(EXPR), gram.Ref(EXPR)}});
  gram.addRule(SUB, {{gram.Int64(0)}, {gram.Ref(EXPR), gram.Ref(EXPR)}});
  gram.addRule(MUL, {{gram.Int64(0)}, {gram.Ref(EXPR), gram.Ref(EXPR)}});

  // LET introduces X as a context symbol.
  gram.addRule(LET, {{gram.Int64(0)},
                     {gram.Sym(X), gram.Ref(EXPR), gram.Ref(EXPR, {X})}});
  gram.addRule(VAR, {{gram.Sym(X)}});
  gram.addRule(EXPR, {{gram.Int64(1)},
                      {gram.Int64(2)},
                      {gram.Int64(3)},
                      {gram.Ref(ADD)},
                      {gram.Ref(SUB)},
                      {gram.Ref(MUL)},
                      {gram.Ref(LET)},
                      // References to VAR need X as a context symbol.
                      {{gram.Ref(VAR)}, {X}}});

  ph::Corpus corp("test.corpus");
  MyTest test(gram, corp);
  test.seedFromRandomDevice();
  test.administer();
}

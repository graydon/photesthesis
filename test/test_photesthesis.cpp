// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <iostream>
#include <photesthesis/corpus.h>
#include <photesthesis/grammar.h>
#include <photesthesis/test.h>
#include <photesthesis/value.h>
#include <sstream>

// This is the SUT: a miniature calculator with a
// stack of local symbolic variables.
class Calculator
{
    std::vector<std::pair<std::string, int64_t>> mVarStack;

  public:
    int64_t
    getVar(std::string const& var)
    {
        for (auto pair = mVarStack.rbegin(); pair != mVarStack.rend(); ++pair)
        {
            if (pair->first == var)
            {
                return pair->second;
            }
        }
        throw std::runtime_error(std::string("variable ") + var + " not found");
    }
    void
    pushVar(std::string const& var, int64_t val)
    {
        mVarStack.emplace_back(var, val);
    }
    void
    popVar()
    {
        mVarStack.pop_back();
    }

    int64_t
    add(int64_t a, int64_t b)
    {
        return a + b;
    }
    int64_t
    sub(int64_t a, int64_t b)
    {
        return a - b;
    }
    int64_t
    mul(int64_t a, int64_t b)
    {
        return a * b;
    }
};

namespace ph = photesthesis;

// These are symbols used in the grammar that describes abstract test
// scenarios in terms of arithmetic expressions.
const ph::RuleName EXPR{"expr"};
const ph::RuleName ADD{"add"};
const ph::RuleName SUB{"sub"};
const ph::RuleName MUL{"mul"};
const ph::RuleName LET{"let"};
const ph::RuleName VAR{"var"};
const ph::ParamName X{"x"};
const ph::ParamName N{"n"};
const ph::VarName RES{"res"};

// This function returns a grammar for abstract tests using the above
// symbols for terminals and nonterminals.
ph::Grammar
exprGrammar()
{
    ph::Grammar gram;

    gram.addRule(ADD, {{gram.Int64(0)}, {gram.Ref(EXPR), gram.Ref(EXPR)}});
    gram.addRule(SUB, {{gram.Int64(0)}, {gram.Ref(EXPR), gram.Ref(EXPR)}});
    gram.addRule(MUL, {{gram.Int64(0)}, {gram.Ref(EXPR), gram.Ref(EXPR)}});

    // LET introduces X as a context symbol.
    gram.addRule(
        LET, {{gram.Int64(0)},
              {gram.Sym(X), gram.Ref(EXPR), addContext(X, gram.Ref(EXPR))}});
    gram.addRule(VAR, {{gram.Sym(X)}});
    gram.addRule(EXPR, {{gram.Int64(1)},
                        {gram.Int64(2)},
                        {gram.Int64(3)},
                        {gram.Ref(ADD)},
                        {gram.Ref(SUB)},
                        {gram.Ref(MUL)},
                        {gram.Ref(LET)},
                        // References to VAR need X as a context symbol.
                        inContext(X, {gram.Ref(VAR)})});
    return gram;
}

// This class represents the _interface_ between the the photesthesis testing
// engine, the abstract test grammar, and the SUT.
class CalcTest : public ph::Test
{

    Calculator mCalc;

  public:
    CalcTest(ph::Grammar const& gram, ph::Corpus& corp)
        : Test(gram, corp, ph::Symbol("CalcTest"), {{{N, EXPR}}})
    {
    }

    int64_t
    eval(ph::Value val)
    {
        ph::Value a, b, c;
        int64_t i;

        if (val.match(EXPR, a))
        {
            if (a.match(ADD, b, c))
            {
                return mCalc.add(eval(b), eval(c));
            }
            if (a.match(SUB, b, c))
            {
                return mCalc.sub(eval(b), eval(c));
            }

            if (a.match(MUL, b, c))
            {
                return mCalc.mul(eval(b), eval(c));
            }

            if (a.match(LET, X, b, c))
            {
                mCalc.pushVar(X.getString(), eval(b));
                i = eval(c);
                mCalc.popVar();
                return i;
            }

            if (a.match(VAR, X))
            {
                return mCalc.getVar(X.getString());
            }

            if (a.match(i))
            {
                return i;
            }
        }
        return 0;
    }

    void
    run() override
    {
        ph::Value val = getParam(N);
        check(RES, ph::Value::Int64(eval(val)));
    }
};

int
main()
{
    ph::Corpus corp("test.corpus");
    ph::Grammar gram = exprGrammar();
    CalcTest test(gram, corp);
    test.seedFromRandomDevice();
    test.administer();
}

#pragma once

// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <map>
#include <photesthesis/3rdparty/xxhash64.h>
#include <photesthesis/symbol.h>
#include <photesthesis/value.h>
#include <random>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace photesthesis
{

inline void
addStringToHash(XXHash64& h, std::string const& s)
{
    h.add(s.c_str(), s.size());
}

inline void
addValueToHash(XXHash64& h, Value v)
{
    std::ostringstream oss;
    oss << v;
    addStringToHash(h, oss.str());
}

inline void
addSymbolToHash(XXHash64& h, Symbol s)
{
    addStringToHash(h, s.getString());
}

inline void
addKeyValueToHash(XXHash64& h, Symbol k, Value v)
{
    addSymbolToHash(h, k);
    addStringToHash(h, "=");
    addValueToHash(h, v);
}

inline Symbol
headSymbol(Value v)
{
    Symbol sym("");
    Value rest;
    if (v.match(sym, rest))
    {
        return sym;
    }
    throw std::runtime_error("expected head symbol in list");
}

inline Symbol
headSymbol(std::shared_ptr<const PairValue> pv)
{
    Symbol sym("");
    Value rest;
    if (pv->match(sym, rest))
    {
        return sym;
    }
    throw std::runtime_error("expected head symbol in list");
}

template <typename T>
T const&
pickUniform(std::default_random_engine& gen, std::vector<T> const& elts)
{
    if (elts.empty())
    {
        throw std::runtime_error("pick_uniform on empty vector");
    }
    std::uniform_int_distribution<size_t> dist(0, elts.size() - 1);
    return elts.at(dist(gen));
}

template <typename K, typename V>
std::pair<const K, V> const&
pickUniform(std::default_random_engine& gen, std::map<K, V> const& elts)
{
    if (elts.empty())
    {
        throw std::runtime_error("pick_uniform on empty map");
    }
    std::uniform_int_distribution<size_t> dist(0, elts.size() - 1);
    auto i = elts.cbegin();
    std::advance(i, dist(gen));
    return *i;
}

template <typename T>
void
expectVal(std::istream& is, T const& expected, T const& got)
{
    if (expected != got)
    {
        throw std::runtime_error(
            std::string("input failure at offset ") +
            std::to_string(is.tellg()) + std::string(": expected '") +
            std::to_string(expected) + std::string("' but got '") +
            std::to_string(got) + std::string("'"));
    }
}

inline void
expectStr(std::istream& is, std::string const& expected, std::string const& got)
{
    if (expected != got)
    {
        throw std::runtime_error(
            std::string("input failure at offset ") +
            std::to_string(is.tellg()) + std::string(": expected '") +
            expected + std::string("' but got '") + got + std::string("'"));
    }
}

inline void
expectNonemptyStr(std::istream& is, std::string const& s)
{
    if (s.empty())
    {
        throw std::runtime_error(
            std::string("input read unexpected empty string at offset ") +
            std::to_string(is.tellg()));
    }
}

inline void
scanWhitespace(std::istream& is)
{
    while (is.good() && std::isspace(is.peek()))
    {
        char c;
        is.get(c);
    }
}

} // namespace photesthesis
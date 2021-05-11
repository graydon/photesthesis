#pragma once

// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <cassert>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace photesthesis
{

/// A Symbol is a globally-unique isalnum()-and-hyphens identifier for use
/// in a Grammar, either as a terminal or nonterminal.
class Symbol
{
    std::shared_ptr<std::string> mInterned;
    static std::shared_ptr<std::string> intern(std::string const&);

  public:
    Symbol(std::string const& s);
    Symbol(Symbol const&) = default;
    Symbol() : Symbol("") {}
    Symbol& operator=(Symbol const&) = default;

    bool operator==(Symbol const& other) const;
    bool operator<(Symbol const& other) const;

    std::string const&
    getString() const
    {
        return *mInterned;
    };
    friend std::ostream& operator<<(std::ostream& os, const Symbol& sym);
    friend std::istream& operator>>(std::istream& is, Symbol& sym);
};

std::ostream& operator<<(std::ostream& os, const Symbol& sym);
std::istream& operator>>(std::istream& is, Symbol& sym);
} // namespace photesthesis
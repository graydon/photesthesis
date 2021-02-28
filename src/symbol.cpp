// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <iostream>
#include <photesthesis/symbol.h>

namespace
{
struct DerefLess
{
    bool
    operator()(std::shared_ptr<std::string> const& x,
               std::shared_ptr<std::string> const& y) const
    {
        assert(x && y);
        return *x < *y;
    }
};
} // namespace

namespace photesthesis
{

std::shared_ptr<std::string>
Symbol::intern(std::string const& s)
{

    static std::mutex sLock;
    static std::set<std::shared_ptr<std::string>, DerefLess> sInternTable;

    for (auto const& c : s)
    {
        if (!(isalnum(c) || c == '_'))
        {
            throw std::runtime_error(
                "Symbol must be alphanumeric-or-underscores");
        }
    }
    auto p = std::make_shared<std::string>(s);
    std::lock_guard<std::mutex> guard(sLock);
    auto pair = sInternTable.emplace(p);
    return *pair.first;
}

Symbol::Symbol(std::string const& s) : mInterned(intern(s))
{
}

bool
Symbol::operator==(Symbol const& other) const
{
    return mInterned == other.mInterned;
};

bool
Symbol::operator<(Symbol const& other) const
{
    assert(mInterned && other.mInterned);
    return *mInterned < *other.mInterned;
};

std::ostream&
operator<<(std::ostream& os, const Symbol& sym)
{
    return os << *sym.mInterned;
}

std::istream&
operator>>(std::istream& is, Symbol& sym)
{
    std::string tmp;
    is >> tmp;
    sym = Symbol(tmp);
    return is;
}

} // namespace photesthesis
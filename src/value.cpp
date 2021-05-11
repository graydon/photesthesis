// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include "photesthesis/util.h"
#include <cassert>
#include <cctype>
#include <iostream>
#include <memory>
#include <photesthesis/value.h>
#include <stdexcept>

namespace photesthesis
{

std::ostream&
operator<<(std::ostream& os, const Type& ty)
{
    switch (ty)
    {
    case Type::Nil:
        os << "Nil";
        break;
    case Type::Pair:
        os << "Pair";
        break;
    case Type::Sym:
        os << "Sym";
        break;
    case Type::Bool:
        os << "Bool";
        break;
    case Type::Int64:
        os << "Int64";
        break;
    case Type::Blob:
        os << "Blob";
        break;
    case Type::String:
        os << "String";
        break;
    }
    return os;
}

bool
ValueImpl::match() const
{
    return true;
}

bool
ValueImpl::match(std::pair<Value, std::shared_ptr<const PairValue>>& out) const
{
    return false;
}
bool
ValueImpl::match(Symbol& out) const
{
    return false;
}
bool
ValueImpl::match(bool& out) const
{
    return false;
}
bool
ValueImpl::match(int64_t& out) const
{
    return false;
}
bool
ValueImpl::match(std::vector<uint8_t>& out) const
{
    return false;
}
bool
ValueImpl::match(std::string& out) const
{
    return false;
}

Type
Value::getType() const
{
    return mImpl ? mImpl->getType() : Type::Nil;
}
Value::Value(std::shared_ptr<const ValueImpl> vip) : mImpl(vip)
{
}
Value::Value() : Value(std::shared_ptr<const ValueImpl>())
{
}
Value::Value(Value head, std::shared_ptr<const PairValue> tail)
    : Value(std::make_shared<const PairValue>(head, tail))
{
}
Value::Value(Symbol val) : Value(std::make_shared<const SymValue>(val))
{
}
Value::Value(std::vector<uint8_t> const& val)
    : Value(std::make_shared<const BlobValue>(val))
{
}
Value::Value(std::string const& val)
    : Value(std::make_shared<const StringValue>(val))
{
}
Value::Value(std::vector<Value> const& vals)
{
    auto tvals(vals);
    std::shared_ptr<const PairValue> tmp;
    while (!tvals.empty())
    {
        tmp = std::make_shared<const PairValue>(Value(tvals.back()), tmp);
        tvals.pop_back();
    }
    mImpl = tmp;
}
Value::Value(std::set<Value> const& vals)
{
    std::vector<Value> tmps;
    for (auto const& v : vals)
    {
        tmps.emplace_back(v);
    }
    Value vtmp(tmps);
    mImpl = vtmp.mImpl;
}
Value::Value(std::map<Value, Value> const& vals)
{
    std::vector<Value> tmps;
    for (auto const& pair : vals)
    {
        std::vector<Value> vpair;
        vpair.emplace_back(pair.first);
        vpair.emplace_back(pair.second);
        tmps.emplace_back(vpair);
    }
    Value vtmp(tmps);
    mImpl = vtmp.mImpl;
}

Value
Value::Bool(bool val)
{
    return Value(std::make_shared<const BoolValue>(val));
}

Value
Value::Int64(int64_t val)
{
    return Value(std::make_shared<const Int64Value>(val));
}

bool
Value::operator!=(Value const& other) const
{
    return !(*this == other);
}

bool
Value::operator==(Value const& other) const
{
    if (getType() != other.getType())
    {
        return false;
    }
    switch (getType())
    {
    case Type::Nil:
        return true;
    case Type::Pair:
    {
        auto a = std::dynamic_pointer_cast<const PairValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const PairValue>(other.mImpl);
        assert(a && b);
        while (a && b)
        {
            auto const& pair_a = a->getValue();
            auto const& pair_b = b->getValue();
            if (pair_a.first != pair_b.first)
            {
                return false;
            }
            a = pair_a.second;
            b = pair_b.second;
        }
        return (!a && !b);
    }
    case Type::Sym:
    {
        auto a = std::dynamic_pointer_cast<const SymValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const SymValue>(other.mImpl);
        assert(a && b);
        return a->getValue() == b->getValue();
    }
    case Type::Bool:
    {
        auto a = std::dynamic_pointer_cast<const BoolValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const BoolValue>(other.mImpl);
        assert(a && b);
        return a->getValue() == b->getValue();
    }
    case Type::Int64:
    {
        auto a = std::dynamic_pointer_cast<const Int64Value>(mImpl);
        auto b = std::dynamic_pointer_cast<const Int64Value>(other.mImpl);
        assert(a && b);
        return a->getValue() == b->getValue();
    }
    case Type::Blob:
    {
        auto a = std::dynamic_pointer_cast<const BlobValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const BlobValue>(other.mImpl);
        assert(a && b);
        return a->getValue() == b->getValue();
    }
    case Type::String:
    {
        auto a = std::dynamic_pointer_cast<const StringValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const StringValue>(other.mImpl);
        assert(a && b);
        return a->getValue() == b->getValue();
    }
    }
}

bool
Value::operator<(Value const& other) const
{
    if (getType() != other.getType())
    {
        return static_cast<size_t>(getType()) <
               static_cast<size_t>(other.getType());
    }
    switch (getType())
    {
    case Type::Nil:
        return false;
    case Type::Pair:
    {
        auto a = std::dynamic_pointer_cast<const PairValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const PairValue>(other.mImpl);
        assert(a && b);
        if (a->mLength < b->mLength)
        {
            return true;
        }
        if (b->mLength < a->mLength)
        {
            return false;
        }
        while (a && b)
        {
            auto const& pair_a = a->getValue();
            auto const& pair_b = b->getValue();
            if (pair_a.first < pair_b.first)
            {
                return true;
            }
            else if (pair_b.first < pair_a.first)
            {
                return false;
            }
            a = pair_a.second;
            b = pair_b.second;
        }
        return (!a && b);
    }
    case Type::Sym:
    {
        auto a = std::dynamic_pointer_cast<const SymValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const SymValue>(other.mImpl);
        assert(a && b);
        return a->getValue() < b->getValue();
    }
    case Type::Bool:
    {
        auto a = std::dynamic_pointer_cast<const BoolValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const BoolValue>(other.mImpl);
        assert(a && b);
        return a->getValue() < b->getValue();
    }
    case Type::Int64:
    {
        auto a = std::dynamic_pointer_cast<const Int64Value>(mImpl);
        auto b = std::dynamic_pointer_cast<const Int64Value>(other.mImpl);
        assert(a && b);
        return a->getValue() < b->getValue();
    }
    case Type::Blob:
    {
        auto a = std::dynamic_pointer_cast<const BlobValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const BlobValue>(other.mImpl);
        assert(a && b);
        return a->getValue() < b->getValue();
    }
    case Type::String:
    {
        auto a = std::dynamic_pointer_cast<const StringValue>(mImpl);
        auto b = std::dynamic_pointer_cast<const StringValue>(other.mImpl);
        assert(a && b);
        return a->getValue() < b->getValue();
    }
    }
}

bool
Value::match() const
{
    return true;
}
bool
Value::match(Value& out) const
{
    out = *this;
    return true;
};

bool
Value::isNil() const
{
    return getType() == Type::Nil;
}
bool
Value::isPair() const
{
    return getType() == Type::Pair;
}
bool
Value::isSym() const
{
    return getType() == Type::Sym;
}
bool
Value::isBool() const
{
    return getType() == Type::Bool;
}
bool
Value::isInt64() const
{
    return getType() == Type::Int64;
}
bool
Value::isBlob() const
{
    return getType() == Type::Blob;
}
bool
Value::isString() const
{
    return getType() == Type::String;
}

std::ostream&
operator<<(std::ostream& os, const Value& val)
{
    if (auto vi = std::dynamic_pointer_cast<const StringValue>(val.mImpl))
    {
        os << '"';
        for (auto c : vi->getValue())
        {
            if (c == '"' || c == '\\')
            {
                os << '\\';
            }
            os << c;
        }
        os << '"';
    }
    else if (auto vi = std::dynamic_pointer_cast<const BlobValue>(val.mImpl))
    {
        os << '[';
        bool first = true;
        for (auto byte : vi->getValue())
        {
            os << (first ? "" : " ") << "0x" << std::hex << byte;
            first = false;
        }
        os << ']';
    }
    else if (auto vi = std::dynamic_pointer_cast<const BoolValue>(val.mImpl))
    {
        os << (vi->getValue() ? "#t" : "#f");
    }
    else if (auto vi = std::dynamic_pointer_cast<const Int64Value>(val.mImpl))
    {
        os << vi->getValue();
    }
    else if (auto vi = std::dynamic_pointer_cast<const SymValue>(val.mImpl))
    {
        os << vi->getValue();
    }
    else if (auto p = std::dynamic_pointer_cast<const PairValue>(val.mImpl))
    {
        os << '(';
        std::pair<Value, std::shared_ptr<const PairValue>> pair = p->getValue();
        os << pair.first;
        while (pair.second)
        {
            pair = pair.second->getValue();
            os << ' ' << pair.first;
        }
        os << ')';
    }
    else
    {
        assert(val.getType() == Type::Nil);
        os << "#nil";
    }
    return os;
}

std::istream&
operator>>(std::istream& is, Value& val)
{
    scanWhitespace(is);
    if (!is.good())
    {
        return is;
    }
    switch (is.peek())
    {
    case EOF:
        break;
    case '(':
    {
        char c;
        is.get(c);
        std::vector<Value> vals;
        while (is.good() && is.peek() != ')')
        {
            Value tmp;
            is >> tmp;
            vals.emplace_back(tmp);
        }
        if (is.good() && is.peek() == ')')
        {
            is.get(c);
            val = Value(vals);
        }
        else
        {
            throw std::runtime_error("incomplete pair-list");
        }
        break;
    }
    case '[':
    {
        char c;
        is.get(c);
        std::vector<uint8_t> bytes;
        while (is.good() && is.peek() != ']')
        {
            bytes.emplace_back();
            is >> std::hex >> bytes.back();
        }
        if (is.good() && is.peek() == ']')
        {
            is.get(c);
            val = Value(bytes);
        }
        else
        {
            throw std::runtime_error("incomplete blob");
        }
        break;
    }
    case '"':
    {
        std::string tmp;
        char c;
        is.get(c);
        while (is.good() && is.peek() != '"')
        {
            is.get(c);
            if (c == '\\')
            {
                if (!is.good())
                {
                    throw std::runtime_error("incomplete string escape");
                }
                is.get(c);
            }
            tmp += c;
        }
        if (is.good() && is.peek() == '"')
        {
            is.get(c);
            val = Value(tmp);
        }
        else
        {
            throw std::runtime_error("incomplete string");
        }
        break;
    }
    case '#':
    {
        std::string tmp;
        char c;
        do
        {
            is.get(c);
            tmp += c;
        } while (is.good() && std::isalnum(is.peek()));
        if (tmp == "#t")
        {
            val = Value::Bool(true);
        }
        else if (tmp == "#f")
        {
            val = Value::Bool(false);
        }
        else if (tmp == "#nil")
        {
            val = Value();
        }
        else
        {
            throw std::runtime_error(std::string("unknown special symbol: ") +
                                     tmp);
        }
    }
    default:
    {
        if (is.peek() == '-' || std::isdigit(is.peek()))
        {
            int64_t tmp{0};
            is >> tmp;
            val = Value::Int64(tmp);
        }
        else if (is.peek() == '_' || std::isalnum(is.peek()))
        {
            std::string tmp;
            char c;
            do
            {
                is.get(c);
                tmp += c;
            } while (is.good() &&
                     (is.peek() == '_' || std::isalnum(is.peek())));
            val = Value(Symbol(tmp));
        }
    }
    break;
    }
    return is;
}

Type
PairValue::getType() const
{
    return Type::Pair;
}
PairValue::PairValue(Value head, std::shared_ptr<const PairValue> tail)
    : TypedValue(std::make_pair(head, tail))
    , mLength(1 + (getValue().second ? getValue().second->mLength : 0))
{
}

Type
SymValue::getType() const
{
    return Type::Sym;
}
SymValue::SymValue(Symbol val) : TypedValue(val)
{
}

Type
BoolValue::getType() const
{
    return Type::Bool;
}
BoolValue::BoolValue(bool val) : TypedValue(val)
{
}

Type
Int64Value::getType() const
{
    return Type::Int64;
}
Int64Value::Int64Value(int64_t val) : TypedValue(val)
{
}

Type
BlobValue::getType() const
{
    return Type::Blob;
}
BlobValue::BlobValue(std::vector<uint8_t> const& val) : TypedValue(val)
{
}

Type
StringValue::getType() const
{
    return Type::String;
}
StringValue::StringValue(std::string const& val) : TypedValue(val)
{
}

} // namespace photesthesis
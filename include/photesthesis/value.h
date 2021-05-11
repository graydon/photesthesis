#pragma once

// Copyright 2021 Stellar Development Foundation and contributors. Licensed
// under the Apache License, Version 2.0. See the COPYING file at the root
// of this distribution or at http://www.apache.org/licenses/LICENSE-2.0

#include <cstdint>
#include <iosfwd>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <optional>

#include <photesthesis/symbol.h>

namespace photesthesis
{

enum class Type
{
    Nil = 0,
    Pair = 1,
    Sym = 2,
    Bool = 3,
    Int64 = 4,
    Blob = 5,
    String = 6,
};

std::ostream& operator<<(std::ostream& os, const Type& ty);
class Value;
class PairValue;

// Un-templated MatcherBase exists just to make the enable_if
// overload guards selecting less ugly below.
struct MatcherBase {};

// Matcher is defined later on, it exists to let client matching
// code interleave with the list-decomposing match functions here.
template<typename T>
struct Matcher;

// A Value wraps a shared_ptr<ValueImpl> of a particular ValueImpl subtype.
class ValueImpl
{
  public:
    virtual Type getType() const = 0;
    virtual bool match() const;
    virtual bool
    match(std::pair<Value, std::shared_ptr<const PairValue>>& out) const;
    virtual bool match(Symbol& out) const;
    virtual bool match(bool& out) const;
    virtual bool match(int64_t& out) const;
    virtual bool match(std::vector<uint8_t>& out) const;
    virtual bool match(std::string& out) const;
};

class PairValue;

/// Value is a serializable, comparable, dynamic-typed s-expression-like
/// recursive building block for the Productions in Grammars. The `match()`
/// methods can be used to pattern-match a Value into a variadic sequence of C++
/// types.
class Value
{
  public:
    Type getType() const;
    Value(std::shared_ptr<const ValueImpl> vip);
    Value(Value const&) = default;
    Value& operator=(Value const&) = default;

    // Primary constructors.
    Value();                                                  // Nil
    Value(Value head, std::shared_ptr<const PairValue> tail); // Pair
    Value(Symbol sym);
    Value(std::vector<uint8_t> const& blob);
    Value(std::string const& str);

    // We can't have these as ctor overloads because of sad
    // C++ number <-> bool coercions. So we go with static methods.
    static Value Bool(bool b);
    static Value Int64(int64_t i);

    // Convenience constructors that build Pair-based structures.
    Value(std::vector<Value> const&);
    Value(std::set<Value> const&);
    Value(std::map<Value, Value> const&);

    bool operator==(Value const& other) const;
    bool operator!=(Value const& other) const;
    bool operator<(Value const& other) const;

    bool isNil() const;
    bool isPair() const;
    bool isSym() const;
    bool isBool() const;
    bool isInt64() const;
    bool isBlob() const;
    bool isString() const;

    // Matching the empty argpack always succeeds.
    bool match() const;

    // Matching a Value always succeeds and assigns *this to `out`.
    bool match(Value& out) const;


    template <typename T, typename = typename std::enable_if<!std::is_base_of<MatcherBase,T>::value>::type>
    bool matchOne(T& out) const
    {
        return mImpl && mImpl->match(out);
    }

    template <typename T>
    bool matchOne(Matcher<T>& out) const;

    // Matching a single other type succeeds if the wrapped ValueImpl matches,
    // assigning the typed value to `out` on successful match.
    template <typename T>
    bool
    match(T& out) const
    {
        return matchOne(out);
    }

    // Matching to a const-ref causes a match to a temporary and an equality
    // comparison on the const-ref. This is a bit subtle but it makes for fairly
    // pleasant idiomatic client code.
    template <typename T, typename = typename std::enable_if<!std::is_base_of<MatcherBase,T>::value>::type>
    bool
    matchOne(T const& v) const
    {
        T tmp(v);
        return match(tmp) && tmp == v;
    }

    template <typename T>
    bool matchOne(Matcher<T> const& out) const;

    template <typename T>
    bool
    match(T const& v) const
    {
        return matchOne(v);
    }

    // Matching any 2-or-more element argpack succeeds if the wrapped ValueImpl
    // is a PairValue, and its head and tail elements both match the respective
    // portions of the argpack.
    template <typename T, typename... Args>
    bool match(T& head, Args&... tail) const;
    template <typename T, typename... Args>
    bool match(T const& head, Args&... tail) const;
    template <typename T, typename... Args>
    bool match(T& head, Args const&... tail) const;
    template <typename T, typename... Args>
    bool match(T const& head, Args const&... tail) const;

    friend std::ostream& operator<<(std::ostream& os, const Value& val);
    friend std::istream& operator>>(std::istream& is, Value& val);

  protected:
    std::shared_ptr<const ValueImpl> mImpl;
};

template<typename T>
struct Matcher : public MatcherBase, public std::optional<T> {
    virtual void match(Value val) {};
    virtual bool match(Value val) const { return false; }
};

std::ostream& operator<<(std::ostream& os, const Value& val);
std::istream& operator>>(std::istream& is, Value& val);

template <typename T> class TypedValue : public ValueImpl
{

  public:
    virtual Type getType() const override = 0;
    virtual ~TypedValue()
    {
    }
    T const&
    getValue() const
    {
        return mValue;
    }
    bool
    match(T& out) const override
    {
        out = mValue;
        return true;
    }

  protected:
    T mValue;
    TypedValue(T const& val) : mValue(val)
    {
    }
};

class PairValue
    : public TypedValue<std::pair<Value, std::shared_ptr<const PairValue>>>
{

  public:
    Type getType() const override;
    const size_t mLength;
    PairValue(Value head, std::shared_ptr<const PairValue> tail);

    // The variadic template implementations of PairValue::match have to bottom
    // out in singleton argpacks rather than the empty argpack because we treat
    // & and const& differently (capture vs. equality-test) and the 2nd-last
    // recursive template call in any chain would be ambiguous if we bottomed
    // out in the empty argpack.

    template <typename T> bool match(T& v) const;
    template <typename T> bool match(T const& v) const;
    template <typename T, typename... Args>
    bool match(T& head, Args&... tail) const;
    template <typename T, typename... Args>
    bool match(T const& head, Args&... tail) const;
    template <typename T, typename... Args>
    bool match(T& head, Args const&... tail) const;
    template <typename T, typename... Args>
    bool match(T const& head, Args const&... tail) const;
};
class SymValue : public TypedValue<Symbol>
{
  public:
    Type getType() const override;
    SymValue(Symbol val);
};

class BoolValue : public TypedValue<bool>
{
  public:
    Type getType() const;
    BoolValue(bool val);
};

class Int64Value : public TypedValue<int64_t>
{
  public:
    Type getType() const;
    Int64Value(int64_t val);
};

class BlobValue : public TypedValue<std::vector<uint8_t>>
{
  public:
    Type getType() const;
    BlobValue(std::vector<uint8_t> const& val);
};
class StringValue : public TypedValue<std::string>
{
  public:
    Type getType() const;
    StringValue(std::string const& val);
};

template <typename T>
inline bool
Value::matchOne(Matcher<T>& m) const
{
    m.reset();
    m.match(*this);
    return m.has_value();
}

template <typename T>
inline bool
Value::matchOne(Matcher<T> const& m) const
{
    return m.match(*this);
}

template <typename T, typename... Args>
inline bool
Value::match(T& head, Args&... tail) const
{
    if (auto a = std::dynamic_pointer_cast<const PairValue>(mImpl))
    {
        return a->match(head, tail...);
    }
    return false;
}

template <typename T, typename... Args>
inline bool
Value::match(T& head, Args const&... tail) const
{
    if (auto a = std::dynamic_pointer_cast<const PairValue>(mImpl))
    {
        return a->match(head, tail...);
    }
    return false;
}

template <typename T, typename... Args>
inline bool
Value::match(T const& head, Args&... tail) const
{
    if (auto a = std::dynamic_pointer_cast<const PairValue>(mImpl))
    {
        return a->match(head, tail...);
    }
    return false;
}

template <typename T, typename... Args>
inline bool
Value::match(T const& head, Args const&... tail) const
{
    if (auto a = std::dynamic_pointer_cast<const PairValue>(mImpl))
    {
        return a->match(head, tail...);
    }
    return false;
}

// PairValue also has a variadic-template matcher that recurses
// down the list.

template <typename T>
inline bool
PairValue::match(T& v) const
{
    return getValue().first.match(v);
}
template <typename T>
inline bool
PairValue::match(T const& v) const
{
    return getValue().first.match(v);
}

template <typename T, typename... Args>
inline bool
PairValue::match(T& head, Args&... tail) const
{
    auto& pair = getValue();
    return pair.first.match(head) &&
           (!pair.second || pair.second->match(tail...));
}

template <typename T, typename... Args>
inline bool
PairValue::match(T const& head, Args&... tail) const
{
    auto& pair = getValue();
    return pair.first.match(head) &&
           (!pair.second || pair.second->match(tail...));
}

template <typename T, typename... Args>
inline bool
PairValue::match(T& head, Args const&... tail) const
{
    auto& pair = getValue();
    return pair.first.match(head) &&
           (!pair.second || pair.second->match(tail...));
}

template <typename T, typename... Args>
inline bool
PairValue::match(T const& head, Args const&... tail) const
{
    auto& pair = getValue();
    return pair.first.match(head) &&
           (!pair.second || pair.second->match(tail...));
}

} // namespace photesthesis

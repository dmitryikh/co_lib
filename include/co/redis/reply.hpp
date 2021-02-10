#pragma once

#include <iostream>
#include <string>
#include <variant>

namespace co::redis
{

class reply_string : public std::string
{
public:
    // using std::string::string;

    reply_string(std::string&& str)
        : std::string(std::move(str))
    {}
};

class reply_error : public std::string
{
public:
    using std::string::string;

    reply_error(std::string&& str)
        : std::string(std::move(str))
    {}
};

class reply_bulk_string : public std::string
{
public:
    // using std::string::string;
    reply_bulk_string(reply_bulk_string&&) = default;
    reply_bulk_string(const reply_bulk_string&) = delete;
    reply_bulk_string& operator=(const reply_bulk_string&) = delete;
    reply_bulk_string& operator=(reply_bulk_string&&) = default;

    reply_bulk_string(std::string&& str)
        : std::string(std::move(str))
    {}
};

class reply_null
{};

class reply;
using reply_array = std::vector<reply>;

class reply
{
    using Variant =
        std::variant<std::monostate, reply_string, reply_error, int64_t, reply_bulk_string, reply_null, reply_array>;

public:
    reply() = default;

    reply(const reply&) = delete;
    reply(reply&&) = default;
    reply& operator=(reply&&) = default;
    reply& operator=(const reply&) = delete;

    reply(reply_string&& r)
        : _variant(std::move(r))
    {}

    reply(reply_error&& r)
        : _variant(std::move(r))
    {}

    reply(int64_t r)
        : _variant(r)
    {}

    reply(reply_bulk_string&& r)
        : _variant(std::move(r))
    {}

    reply(reply_null&& r)
        : _variant(std::move(r))
    {}

    reply(reply_array&& r)
        : _variant(std::move(r))
    {}

    bool has_string() const noexcept
    {
        return std::holds_alternative<reply_string>(_variant);
    }

    bool has_error() const noexcept
    {
        return std::holds_alternative<reply_error>(_variant);
    }

    bool has_int64() const noexcept
    {
        return std::holds_alternative<int64_t>(_variant);
    }

    bool has_bulk_string() const noexcept
    {
        return std::holds_alternative<reply_bulk_string>(_variant);
    }

    bool has_null() const noexcept
    {
        return std::holds_alternative<reply_null>(_variant);
    }

    bool has_array() const noexcept
    {
        return std::holds_alternative<reply_array>(_variant);
    }

    bool empty() const noexcept
    {
        return std::holds_alternative<std::monostate>(_variant);
    }

    const std::string& as_string() const
    {
        if (!has_string())
            throw std::runtime_error("reply holds no string");

        return std::get<reply_string>(_variant);
    }

    std::string& as_string()
    {
        if (!has_string())
            throw std::runtime_error("reply holds no string");

        return std::get<reply_string>(_variant);
    }

    const std::string& as_error() const
    {
        if (!has_error())
            throw std::runtime_error("reply holds no error");

        return std::get<reply_error>(_variant);
    }

    std::string& as_error()
    {
        if (!has_error())
            throw std::runtime_error("reply holds no error");

        return std::get<reply_error>(_variant);
    }

    int64_t as_int64() const
    {
        if (!has_int64())
            throw std::runtime_error("reply holds no int64");

        return std::get<int64_t>(_variant);
    }

    const std::string& as_bulk_string() const
    {
        if (!has_bulk_string())
            throw std::runtime_error("reply holds no bulk_string");

        return std::get<reply_bulk_string>(_variant);
    }

    std::string& as_bulk_string()
    {
        if (!has_bulk_string())
            throw std::runtime_error("reply holds no bulk_string");

        return std::get<reply_bulk_string>(_variant);
    }

    const std::vector<reply>& as_array() const
    {
        if (!has_array())
            throw std::runtime_error("reply holds no array");

        return std::get<reply_array>(_variant);
    }

    std::vector<reply>& as_array()
    {
        if (!has_array())
            throw std::runtime_error("reply holds no array");

        return std::get<reply_array>(_variant);
    }

    friend std::ostream& operator<<(std::ostream& o, const reply& r)
    {
        if (r.has_null())
            o << "null";
        else if (r.has_int64())
            o << r.as_int64();
        else if (r.has_string())
            o << r.as_string();
        else if (r.has_error())
            o << r.as_error();
        else if (r.has_bulk_string())
            o << r.as_bulk_string();
        else
            o << "unknown";
        return o;
    }

private:
    Variant _variant;
};

}  // namespace co::redis
#pragma once

#include <charconv>
#include <string>

namespace co::redis
{

namespace impl
{

inline void serialize_int64(std::vector<char>& buffer, int64_t v)
{
    std::array<char, std::numeric_limits<int64_t>::digits + 1> local_buffer;

    const auto res = std::to_chars(local_buffer.begin(), local_buffer.end(), v);
    if (res.ec != std::errc{})
        throw std::runtime_error("int64 serialization failed");

    std::copy(local_buffer.begin(), res.ptr, std::back_inserter(buffer));
}

}  // namespace impl

class command
{
public:
    size_t bytes_estimation() const
    {
        size_t size = 0;
        size += 1;  // *
        size += std::numeric_limits<int64_t>::digits;
        size += 2;  // \r\n

        for (const auto& token : tokens)
        {
            size += 1;  // $
            size += std::numeric_limits<int64_t>::digits;
            size += 2;  // \r\n
            size += token.size();
            size += 2;  // \r\n
        }

        return size;
    }

    void serialize(std::vector<char>& buffer) const
    {
        buffer.push_back('*');
        impl::serialize_int64(buffer, tokens.size());
        buffer.push_back('\r');
        buffer.push_back('\n');
        for (const auto& token : tokens)
        {
            buffer.push_back('$');
            impl::serialize_int64(buffer, token.size());
            buffer.push_back('\r');
            buffer.push_back('\n');
            std::copy(token.begin(), token.end(), std::back_inserter(buffer));
            buffer.push_back('\r');
            buffer.push_back('\n');
        }
    }

    std::vector<std::string> tokens;
};

}  // namespace co::redis
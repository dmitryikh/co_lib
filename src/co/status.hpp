#pragma once

namespace co
{

enum status_code
{
    undefined = 0,
    ok = 1,
    cancel = 2,
    timeout = 3
};

class status
{
public:
    status() = default;

    status(status_code code)
        : _code(code)
    {}

    operator status_code() const
    {
        return _code;
    };
private:
    status_code _code = undefined;
};

}  // namespace co
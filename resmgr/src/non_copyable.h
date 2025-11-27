#pragma once

class NonCopyable
{
protected:
	NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(NonCopyable const &) = delete;
    void operator=(NonCopyable const &x) = delete;
};

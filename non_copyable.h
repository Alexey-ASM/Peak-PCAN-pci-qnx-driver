/*
 * non_copyable.h
 *
 *  Created on: 15 Nov 2025
 *      Author: Alexey.Malyshev@outlook.ie
 */

#pragma once

class NonCopyable
{
protected:
	NonCopyable() = default;
    ~NonCopyable() = default;
    NonCopyable(NonCopyable const &) = delete;
    void operator=(NonCopyable const &x) = delete;
};


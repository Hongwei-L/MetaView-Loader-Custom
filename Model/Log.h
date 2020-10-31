// Copyright (c) 2020, Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <sstream>

#define DEBUGLOG(...)                         \
    do {                                      \
        std::ostringstream os;                \
        os << __FUNCTION__ << ": ";           \
        os << __VA_ARGS__;                    \
        os << "\n";                           \
        OutputDebugStringA(os.str().c_str()); \
    } while (0)

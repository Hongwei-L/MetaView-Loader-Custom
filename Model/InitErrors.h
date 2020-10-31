// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
#pragma once

#include <stdint.h>

namespace metaview {
enum class InitErrors : uint32_t {
    None = 0,
    NotInitialized = 1,
    NoDevice = 100,
    TooManyDevices = 101,
};
static inline const char* to_string(InitErrors err) {
    switch (err) {
        case InitErrors::None:
            return "None";
        case InitErrors::NotInitialized:
            return "NotInitialized";
        case InitErrors::NoDevice:
            return "NoDevice";
        case InitErrors::TooManyDevices:
            return "TooManyDevices";
        default:
            return "INVALID";
    }
}
}  // namespace metaview

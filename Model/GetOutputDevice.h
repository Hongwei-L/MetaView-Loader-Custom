// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
#pragma once
#include <stdint.h>

namespace metaview {
/**
 * @brief Get the LUID for the default adapter
 *
 * @return uint64_t corresponding to a LUID
 */
uint64_t getDefaultLuid();
}  // namespace metaview

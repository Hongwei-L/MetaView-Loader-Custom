// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#include "ModeComparison.h"

#include <stdexcept>

namespace metaview {

double ModeInfo::getRate() const {
    if (modeInfo == nullptr) {
        throw std::logic_error("Can't get rate of a null mode info.");
    }
    double numerator = modeInfo.PresentationRate().VerticalSyncRate.Numerator;
    double denominator =
        modeInfo.PresentationRate().VerticalSyncRate.Denominator;
    return numerator / denominator;
}

/**
 * Max refresh rate we'll select.
 *
 * Slightly higher than the nominal 90 because the display timing math can
 * result in not-quite-whole refresh rates.
 */

constexpr double MaxRefresh = 91.0;

bool ModeInfo::betterThan(ModeInfo const& other) const noexcept {
    if (modeInfo == nullptr) {
        // empty is never better
        return false;
    }
    // something is always better than nothing
    if (modeInfo != nullptr && other.modeInfo == nullptr) {
        return true;
    }

    // Prefer something no larger than MaxRefresh
    if (getRate() > MaxRefresh && other.getRate() <= MaxRefresh) {
        return false;
    }
    if (getRate() <= MaxRefresh && other.getRate() > MaxRefresh) {
        return true;
    }

    // If both are below MaxRefresh, pick the higher one.
    return getRate() > other.getRate();
}

}  // namespace metaview
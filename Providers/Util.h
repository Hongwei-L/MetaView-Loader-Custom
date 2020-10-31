// Copyright (c) 2020, Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#pragma once

#include <directxmath.h>
#include <cmath>
#include "ProviderInterface/XRMath.h"
#include "SimpleMath.h"

/**
 * @brief Convert a Unity 4x4 matrix to a DirectX::SimpleMath matrix.
 *
 * @param unityMat Matrix to convert.
 * @return DirectX::SimpleMath::Matrix
 */
static inline DirectX::SimpleMath::Matrix toSimpleMath(
    UnityXRMatrix4x4 const& unityMat) {
    return DirectX::SimpleMath::Matrix(
        // clang-format off
        unityMat.columns[0].x, unityMat.columns[1].x, unityMat.columns[2].x, unityMat.columns[3].x,
        unityMat.columns[0].y, unityMat.columns[1].y, unityMat.columns[2].y, unityMat.columns[3].y,
        unityMat.columns[0].z, unityMat.columns[1].z, unityMat.columns[2].z, unityMat.columns[3].z,
        unityMat.columns[0].w, unityMat.columns[1].w, unityMat.columns[2].w, unityMat.columns[3].w);
    // clang-format on
}

static inline bool isMatrixValid(UnityXRMatrix4x4 const& unityMat) {
    auto mat = toSimpleMath(unityMat);
    return std::abs(mat.Determinant()) > FLT_MIN;
}

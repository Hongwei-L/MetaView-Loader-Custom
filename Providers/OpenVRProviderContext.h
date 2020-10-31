// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/OpenVRProviderContext.h
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <cassert>
#include "ProviderInterface/IUnityXRDisplay.h"

struct IUnityXRTrace;
struct IUnityXRDisplayInterface;
struct IUnityXRInputInterface;

class OpenVRDisplayProvider;
class MetaViewInputProvider;

struct OpenVRProviderContext {
    IUnityXRTrace *trace;

    IUnityXRDisplayInterface *display;
    OpenVRDisplayProvider *displayProvider;

    IUnityXRInputInterface *input;
    MetaViewInputProvider *inputProvider;
};

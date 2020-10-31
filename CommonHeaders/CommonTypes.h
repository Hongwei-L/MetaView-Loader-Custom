// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include "ProviderInterface/IUnityXRTrace.h"
#include "UnityInterfaces.h"

//#define XR_TRACE_PTR (UnityInterfaces::Get().GetInterface<IUnityXRTrace>())

extern IUnityXRTrace *s_pXRTrace;
#define XR_TRACE_PTR s_pXRTrace

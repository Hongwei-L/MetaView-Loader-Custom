// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#pragma once

#ifdef MV_STANDALONE
#include "ProviderInterface/IUnityXRTrace.h"

const IUnityXRTrace* GetTrace();
#define XR_TRACE_PTR (GetTrace())

#else
#include <CommonHeaders/CommonTypes.h>
#endif

#define MV_ERROR(...) XR_TRACE_ERROR(XR_TRACE_PTR, __VA_ARGS__)
#define MV_LOG(...) XR_TRACE_ERROR(XR_TRACE_PTR, __VA_ARGS__)

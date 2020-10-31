// Copyright 2020 Meta View, Inc.
//
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED

#define _CRT_SECURE_NO_WARNINGS

#include "Logging.h"

#include <stdarg.h>
#include <stdio.h>
#include <iostream>

namespace {
void UNITY_INTERFACE_API DoTrace(XRLogType logType, const char *message, ...) {
    switch (logType) {
        case kXRLogTypeError:
            std::cerr << "ERROR: ";
            break;
        case kXRLogTypeAssert:
            std::cerr << "ASSERT: ";
            break;
        case kXRLogTypeWarning:
            std::cerr << "kXRLogTypeWarning: ";
            break;
        case kXRLogTypeLog:
            std::cerr << "kXRLogTypeLog: ";
            break;
        case kXRLogTypeException:
            std::cerr << "kXRLogTypeException: ";
            break;
        case kXRLogTypeDebug:
            std::cerr << "kXRLogTypeDebug: ";
            break;

        default:
            break;
    }
    char buffer[1024];
    va_list args;
    va_start(args, message);
    vsprintf(buffer, message, args);
    std::cerr << buffer << std::endl;
    va_end(args);
}
struct UnityTraceImpl : public IUnityXRTrace {};
IUnityXRTrace setup() {
    IUnityXRTrace ret;
    ret.Trace = DoTrace;
    return ret;
}
}  // namespace

#ifdef MV_STANDALONE
const IUnityXRTrace *GetTrace() {
    static const IUnityXRTrace inst = setup();
    return &inst;
}
#endif

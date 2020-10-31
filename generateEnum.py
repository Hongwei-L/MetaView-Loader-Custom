#!/usr/bin/env python3
# Copyright (c) 2020, Meta View, Inc.
# All rights reserved.
#
# SPDX-License-Identifier: UNLICENSED

VALUES = {
    "None": 0,
    "NotInitialized": 1,
    "NoDevice": 100,
    "TooManyDevices": 101,
}

CS_PATH = "com.metavision.unity/Runtime/InitErrors.cs"
CS_NAMESPACE = "Unity.XR.MetaView"
H_PATH = "Model/InitErrors.h"
H_NAMESPACE = "metaview"

HEADER = [
    '// Copyright (c) 2020, Meta View, Inc.',
    '// All rights reserved.',
    '// SPDX-License-' + 'Identifier: UNLICENSED',
]


def writeCSharp(fp):
    fp.write('\n'.join(HEADER))
    fp.write('\n\n')
    fp.write(f'namespace {CS_NAMESPACE} {{\n')
    fp.write(f'public enum InitErrors : System.UInt32 {{\n')
    for name, val in VALUES.items():
        fp.write(f'    {name} = {val},\n')
    fp.write('}\n')
    fp.write('}\n')


def writeCPP(fp):
    fp.write('\n'.join(HEADER))
    fp.write('\n#pragma once\n\n')
    fp.write('#include <stdint.h>\n\n')
    fp.write(f'namespace {H_NAMESPACE} {{\n')
    fp.write(f'enum class InitErrors : uint32_t {{\n')
    for name, val in VALUES.items():
        fp.write(f'    {name} = {val},\n')
    fp.write('};\n')
    fp.write('static inline const char * to_string(InitErrors err) {\n')
    fp.write('switch (err) {\n')
    for name in VALUES:
        fp.write(f'case InitErrors::{name}: return "{name}";\n')
    fp.write('default: return "INVALID";\n')
    # end of switch
    fp.write('}\n')
    # end of function
    fp.write('}\n')

    # end of namespace
    fp.write('}\n')


if __name__ == "__main__":
    with open(CS_PATH, 'w', encoding='utf-8') as fp:
        writeCSharp(fp)
    with open(H_PATH, 'w', encoding='utf-8') as fp:
        writeCPP(fp)

# Copyright 2020 Meta View, Inc.
#
# All rights reserved.
# SPDX-License-Identifier: UNLICENSED

function configAndBuild {
    param (
        $arch
    )
    Write-Host "**** Building for platform ${arch}"
    # mkdir -Force "build-${arch}"
    cmake -G "Visual Studio 16 2019" -a -T host=x64 -A ${arch} -S . -B "build-${arch}" -DBUILD_EXTRA_SAMPLES=OFF
    cmake --build "build-${arch}" --config RelWithDebInfo --target ALL_BUILD -j4
    cmake --build "build-${arch}" --config RelWithDebInfo --target PlaceInPackage
}

# Clean up old files first or it won't install right.
Remove-Item .\com.metavision.unity\Runtime\x64\XRSDKMetaView.dll -ErrorAction SilentlyContinue
Remove-Item .\com.metavision.unity\Runtime\x86\XRSDKMetaView.dll -ErrorAction SilentlyContinue

# Now build and install.
configAndBuild x64
configAndBuild Win32

// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/UserProjectSettings.h
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

#include <string>

enum EVRMirrorViewMode {
    Eye_None = 0,
    Eye_Left = 1,
    Eye_Right = 2,
    Distort_Eye_Both = 3,
};

enum class EVRStereoRenderingModes {
    MultiPass = 0,
    SinglePassInstanced = 1,
    SingleCamera = 2,
};

class UserProjectSettings {
  public:
    static bool InEditor();
    static bool RotateEyes();
    static EVRStereoRenderingModes GetStereoRenderingMode();
    static void Initialize();
    static EVRMirrorViewMode GetMirrorViewMode();
    static int GetUnityMirrorViewMode();
    static std::string GetProjectDirectoryPath(bool bAddDataDirectory);
    static std::string GetCurrentWorkingPath();
    static bool FileExists(const std::string &fileName);

  private:
    static bool FindSettingAndGetValue(const std::string &line,
                                       const std::string &lineKey,
                                       std::string &lineValue);
    static std::string GetExecutablePath();
    static void Trim(std::string &s);
    static bool DirectoryExists(const char *const path);
    static std::string RemoveFileExtension(const std::string &filename);
};

// Copyright (c) 2020, Meta View, Inc.
// All rights reserved.
// SPDX-License-Identifier: UNLICENSED
//
// Portions based on unity-xr-plugin/Providers/UserProjectSettings.cpp
// Copyright (c) 2020, Valve Software
//
// SPDX-License-Identifier: BSD-3-Clause

#include "UserProjectSettings.h"

#include "CommonTypes.h"
#include "Display/Display.h"
#include "Metadata.h"
#include "UnityInterfaces.h"

#ifdef _WIN32
#include <direct.h>
#include <windows.h>  //GetModuleFileNameW
#else
#include <libgen.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>  //readlink
#endif
#include <sys/stat.h>
#include <algorithm>

#include <cassert>
#include <fstream>
#include <iomanip>
#include <iostream>

#ifdef __linux__
#define MAX_PATH PATH_MAX
#define _getcwd getcwd
char *strcpy_s(char *dest, const char *src) { return strcpy(dest, src); }
char *strcpy_s(char *dest, unsigned long maxCharacters, const char *src) {
    return strncpy(dest, src, maxCharacters);
}
#endif

typedef struct _UserDefinedSettings {
    unsigned short stereoRenderingMode = 0;
    unsigned short mirrorViewMode = 0;
    unsigned short rotateEyes = 1;
} UserDefinedSettings;

static UserDefinedSettings s_UserDefinedSettings;
static bool bInitialized = false;
static bool inEditor = true;

const std::string kStereoRenderingMode = "StereoRenderingMode:";
const std::string kMirrorViewMode = "MirrorView:";
const std::string kRotateEyes = "RotateEyes:";

#ifdef __linux__
const std::string kStreamingAssetsFilePath =
    "StreamingAssets/" + std::string{StreamingAssetsSubdir} + "/" +
    std::string{StreamingAssetsSettingsFilename};
#else
const std::string kStreamingAssetsFilePath =
    "StreamingAssets\\" + std::string{StreamingAssetsSubdir} + "\\" +
    std::string{StreamingAssetsSettingsFilename};
#endif

const std::string kVSDebugPath = "..\\..\\";

std::string UserProjectSettings::GetProjectDirectoryPath(
    bool bAddDataDirectory) {
    std::string exePath = GetExecutablePath();

    char fullExePath[MAX_PATH];
    strcpy_s(fullExePath, exePath.c_str());

    char exePathDrive[MAX_PATH];
    char exePathDirectory[MAX_PATH];
    char exePathFilename[MAX_PATH];
    char exePathExtension[MAX_PATH];

#ifndef __linux__
    _splitpath_s(fullExePath, exePathDrive, exePathDirectory, exePathFilename,
                 exePathExtension);

    std::string basePath =
        std::string(exePathDrive) + std::string(exePathDirectory);
#else
    std::string basePath = std::string(dirname(fullExePath));
#endif

    if (!bAddDataDirectory) {
        return basePath;
    } else {
        std::string projectDirectoryName;

#ifndef __linux__
        projectDirectoryName = std::string(exePathFilename) + "_Data\\";
        std::string projectDirectoryPath = basePath + projectDirectoryName;
        if (!DirectoryExists(projectDirectoryPath.c_str())) {
            // check for the path it might be in if we've made a vs debug build
            projectDirectoryPath =
                basePath + kVSDebugPath + projectDirectoryName;
        }
        return projectDirectoryPath;
#else
        projectDirectoryName =
            RemoveFileExtension(std::string(basename(fullExePath)) + "_Data/");
        return basePath + projectDirectoryName;
#endif
    }
}

bool UserProjectSettings::FindSettingAndGetValue(const std::string &line,
                                                 const std::string &lineKey,
                                                 std::string &lineValue) {
    size_t index = line.find(lineKey);
    if (index != std::string::npos) {
        if (line.at(index + lineKey.length()) == ' ') {
            index++;
        }

        lineValue =
            line.substr(index + lineKey.length(), line.length() - index);

        if (lineValue.length() > 0) {
            return true;
        }
    }
    return false;
}

std::string UserProjectSettings::GetCurrentWorkingPath() {
    char temp[MAX_PATH];
    return (_getcwd(temp, sizeof(temp)) ? std::string(temp) : std::string(""));
}

std::string UserProjectSettings::GetExecutablePath() {
#ifdef _WIN32
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, path, MAX_PATH);
    return std::string(path);
#else
    char result[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
    return std::string(result, (count > 0) ? count : 0);
#endif
}

void UserProjectSettings::Trim(std::string &s) {
    std::string::const_iterator it = s.begin();
    while (it != s.end() && isspace((unsigned char)*it)) it++;

    std::string::const_reverse_iterator rit = s.rbegin();
    while (rit.base() != it && isspace((unsigned char)*rit)) rit++;

    s = std::string(it, rit.base());
}

std::string UserProjectSettings::RemoveFileExtension(
    const std::string &filename) {
    size_t lastdot = filename.find_last_of(".");
    if (lastdot == std::string::npos) return filename;
    return filename.substr(0, lastdot);
}

bool UserProjectSettings::RotateEyes() {
    return (bool)s_UserDefinedSettings.rotateEyes;
}

EVRStereoRenderingModes UserProjectSettings::GetStereoRenderingMode() {
    return (EVRStereoRenderingModes)s_UserDefinedSettings.stereoRenderingMode;
}

EVRMirrorViewMode UserProjectSettings::GetMirrorViewMode() {
    return (EVRMirrorViewMode)s_UserDefinedSettings.mirrorViewMode;
}

int UserProjectSettings::GetUnityMirrorViewMode() {
    int unityMode = kUnityXRMirrorBlitNone;

    switch ((EVRMirrorViewMode)GetMirrorViewMode()) {
        case EVRMirrorViewMode::Eye_None:
            unityMode = kUnityXRMirrorBlitNone;
            break;
        case EVRMirrorViewMode::Eye_Left:
            unityMode = kUnityXRMirrorBlitLeftEye;
            break;
        case EVRMirrorViewMode::Eye_Right:
            unityMode = kUnityXRMirrorBlitRightEye;
            break;
        case EVRMirrorViewMode::Distort_Eye_Both:
        default:
            unityMode = kUnityXRMirrorBlitDistort;
            break;
    }

    return unityMode;
}

const char *GetStereoRenderingModeString(unsigned short nStereoRenderingMode) {
    switch (nStereoRenderingMode) {
        case 0:
            return "Multi Pass";
        case 1:
            return "Single Pass Instanced";
        case 2:
            return "Single Camera";
        default:
            return "Unknown";
    }
}

const char *GetMirrorViewModeString(unsigned short nMirrorViewMode) {
    switch (nMirrorViewMode) {
        case 0:
            return "None";
        case 1:
            return "Left Eye";
        case 2:
            return "Right Eye";
        case 3:
            return "Distort";
        default:
            return "Unknown";
    }
}

bool UserProjectSettings::FileExists(const std::string &fileName) {
    std::ifstream infile(fileName);
    return infile.good();
}

bool UserProjectSettings::DirectoryExists(const char *const path) {
    struct stat info;

    int statRC = stat(path, &info);
    if (statRC != 0) {
        if (errno == ENOENT) {
            return false;
        }  // something along the path does not exist
        if (errno == ENOTDIR) {
            return false;
        }  // something in path prefix is not a dir
        return false;
    }

    return (info.st_mode & S_IFDIR) ? true : false;
}
extern "C" uint16_t UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
GetMirrorViewMode() {
    return s_UserDefinedSettings.mirrorViewMode;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetMirrorViewMode(uint16_t mirrorViewMode) {
    XR_TRACE(PLUGIN_LOG_PREFIX "Extern SetMirrorViewMode (%s)\n",
             GetMirrorViewModeString(mirrorViewMode));

    s_UserDefinedSettings.mirrorViewMode = mirrorViewMode;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
TellPluginRotateEyes(uint16_t rotateEyes) {
    XR_TRACE(PLUGIN_LOG_PREFIX "Extern TellPluginRotateEyes (%d)\n",
             (int)rotateEyes);

    s_UserDefinedSettings.rotateEyes = rotateEyes;
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
SetUserDefinedSettings(UserDefinedSettings settings) {
    try {
        XR_TRACE(PLUGIN_LOG_PREFIX "Loaded settings: \n");
        XR_TRACE("\tStereo Rendering Mode : %s\n",
                 GetStereoRenderingModeString(settings.stereoRenderingMode));
        XR_TRACE("\tMirror View Mode : %s\n",
                 GetMirrorViewModeString(settings.mirrorViewMode));
        XR_TRACE("\tRotate Eyes : %s\n",
                 settings.rotateEyes == 0 ? "false" : "true");

        // Not sure why just s_UserDefinedSettings = settings; doesn't work, but
        // it doesn't.
        s_UserDefinedSettings.stereoRenderingMode =
            settings.stereoRenderingMode;
        s_UserDefinedSettings.mirrorViewMode = settings.mirrorViewMode;
        s_UserDefinedSettings.rotateEyes = settings.rotateEyes;
        bInitialized = true;

    }

    catch (std::exception const &e) {
        OutputDebugStringA(__FUNCTION__ ": Exception ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
    }
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API NotifyInBuild(void) {
    inEditor = false;
}

bool UserProjectSettings::InEditor() { return inEditor; }

void UserProjectSettings::Initialize() {
    if (!bInitialized) {
        std::string projectDirectoryPath = GetProjectDirectoryPath(true);
        std::string settingsPath =
            projectDirectoryPath + kStreamingAssetsFilePath;

        if (FileExists(settingsPath)) {
            UserDefinedSettings settings;

            std::string line;
            std::ifstream infile;
            infile.open(settingsPath);
            if (infile.is_open()) {
                while (getline(infile, line)) {
                    std::string lineValue;

                    if (FindSettingAndGetValue(line, kStereoRenderingMode,
                                               lineValue)) {
                        settings.stereoRenderingMode =
                            (unsigned short)std::stoi(lineValue);
                    } else if (FindSettingAndGetValue(line, kMirrorViewMode,
                                                      lineValue)) {
                        settings.mirrorViewMode =
                            (unsigned short)std::stoi(lineValue);
                    } else if (FindSettingAndGetValue(line, kRotateEyes,
                                                      lineValue)) {
                        settings.rotateEyes =
                            (unsigned short)std::stoi(lineValue);
                    }
                }
                infile.close();
            }

            SetUserDefinedSettings(settings);
        } else {
            XR_TRACE(PLUGIN_LOG_PREFIX
                     "[ERROR] Not initialized by Unity and could not find "
                     "settings file. Searched paths: \n\t'%s'\n\t'%s'\n",
                     (projectDirectoryPath + kStreamingAssetsFilePath).c_str(),
                     settingsPath.c_str());
        }
    }
}

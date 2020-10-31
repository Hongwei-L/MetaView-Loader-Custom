# Copyright (c) 2020, Meta View, Inc.
# Copyright (c) 2020, Valve Software
#
# SPDX-License-Identifier: BSD-3-Clause

cmake_minimum_required(VERSION 3.10)
project(unity_xr_metaview_plugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_EXTRA_SAMPLES "Should we build extra samples?" ON)
option(ENABLE_CODE_ANALYSIS "Should we turn on built-in code analysis?" ON)

set(PACKAGE "com.metavision.unity")

# Auto determine platform if none is provided
if(NOT PLATFORM)
	if(CMAKE_SIZEOF_VOID_P MATCHES 8)
		set(PLATFORM 64)
		set(PLATFORMX "x64")
	else()
		set(PLATFORM 32)
		set(PLATFORMX "x86")
	endif()
endif()

if(MSVC)
	# Parallel within a target
	string(APPEND CMAKE_CXX_FLAGS " /MP")
endif()

set(DEST "${CMAKE_CURRENT_SOURCE_DIR}/${PACKAGE}/Runtime/${PLATFORMX}")

set(SHARED_SOURCES
	Model/DirectDisplayManager.h
	Model/DirectDisplayManager.cpp
	Model/DisplayDetection.h
	Model/DisplayDetection.cpp
	Model/GetOutputDevice.cpp
	Model/GetOutputDevice.h
	Model/ModeComparison.h
	Model/ModeComparison.cpp
	Model/ModeSelection.h
	Model/ModeSelection.cpp
	Model/RenderParam.h
	Model/RenderParam.cpp
	Model/Renderer.cpp
	Model/Renderer.h
	Model/Log.h
	Model/Logging.h
	Model/Logging.cpp)

add_library(standalone STATIC ${SHARED_SOURCES})
target_compile_definitions(standalone PUBLIC MV_STANDALONE)

target_include_directories(standalone PUBLIC CommonHeaders
											 ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(standalone PUBLIC dxgi d3d11 WindowsApp)

add_executable(sample samples/Sample.cpp)
target_link_libraries(sample standalone)

add_executable(GetTopology samples/GetTopology.cpp)
target_link_libraries(GetTopology standalone)

add_subdirectory(ThirdParty)

if(BUILD_EXTRA_SAMPLES)
	add_executable(samplegui WIN32 samples/SampleGUI.cpp samples/SampleGUI.rc)
	target_link_libraries(samplegui standalone)
	target_compile_definitions(samplegui PRIVATE _UNICODE)

	add_executable(rendersample samples/RenderSample.cpp
								samples/GameRenderer.cpp)
	target_link_libraries(rendersample standalone DirectXTK)
endif()

add_library(
	XRSDKMetaView MODULE
	${SHARED_SOURCES}
	Providers/dllmain.cpp
	Providers/Metadata.h
	Providers/OpenVRProviderContext.h
	Providers/OpenVRSystem.h
	Providers/OpenVRSystem.cpp
	Providers/UserProjectSettings.h
	Providers/UserProjectSettings.cpp
	Providers/Display/Display.h
	Providers/Display/Display.cpp
	Providers/Input/Input.h
	Providers/Input/Input.cpp
	CommonHeaders/UnityInterfaces.h
	CommonHeaders/UnityInterfaces.cpp
	CommonHeaders/Singleton.h
	CommonHeaders/CommonTypes.h
	CommonHeaders/ProviderInterface/IUnityEventQueue.h
	CommonHeaders/ProviderInterface/IUnityGraphics.h
	CommonHeaders/ProviderInterface/IUnityGraphicsD3D11.h
	CommonHeaders/ProviderInterface/IUnityGraphicsD3D12.h
	CommonHeaders/ProviderInterface/IUnityGraphicsMetal.h
	CommonHeaders/ProviderInterface/IUnityGraphicsVulkan.h
	CommonHeaders/ProviderInterface/IUnityInterface.h
	CommonHeaders/ProviderInterface/IUnityProfilerCallbacks.h
	CommonHeaders/ProviderInterface/IUnityRenderingExtensions.h
	CommonHeaders/ProviderInterface/IUnityShaderCompilerAccess.h
	CommonHeaders/ProviderInterface/IUnityXRDisplay.h
	CommonHeaders/ProviderInterface/IUnityXRInput.h
	CommonHeaders/ProviderInterface/IUnityXRMeshing.h
	CommonHeaders/ProviderInterface/IUnityXRPreInit.h
	CommonHeaders/ProviderInterface/IUnityXRStats.h
	CommonHeaders/ProviderInterface/IUnityXRTrace.h
	CommonHeaders/ProviderInterface/UnitySubsystemTypes.h
	CommonHeaders/ProviderInterface/UnityXRDisplayStats.h
	CommonHeaders/ProviderInterface/UnityXRTypes.h
	CommonHeaders/ProviderInterface/XRMath.h
	CommonHeaders/ProviderInterface/XRMath.cpp)

target_include_directories(
	XRSDKMetaView PUBLIC Providers CommonHeaders ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(XRSDKMetaView PRIVATE DirectXTK)
target_link_libraries(XRSDKMetaView PRIVATE dxgi d3d11 WindowsApp)

set(DESTFILE "${DEST}/XRSDKMetaView.dll")
add_custom_command(
	OUTPUT "${DESTFILE}" "${DEST}/XRSDKMetaView.pdb"
	COMMAND ${CMAKE_COMMAND} -E remove -f
			"${DEST}/$<TARGET_FILE_NAME:XRSDKMetaView>"
	COMMAND ${CMAKE_COMMAND} -E remove -f
			"${DEST}/$<TARGET_PDB_FILE_NAME:XRSDKMetaView>"
	COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:XRSDKMetaView>"
			"${DESTFILE}"
	COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_PDB_FILE:XRSDKMetaView>" ${DEST}
	COMMENT "Copying plugin binary to package runtime directory"
	VERBATIM)

add_custom_target(PlaceInPackage DEPENDS "${DESTFILE}")
add_dependencies(PlaceInPackage XRSDKMetaView)

if(MSVC)
	# Statically link the runtime libraries
	target_compile_options(
		XRSDKMetaView
		PRIVATE /Zi $<$<CONFIG:>:/MT> $<$<CONFIG:Debug>:/MTd>
				$<$<CONFIG:Release>:/MT> $<$<CONFIG:RelWithDebInfo>:/MT>
				$<$<CONFIG:MinSizeRel>:/MT>)
	if(ENABLE_CODE_ANALYSIS)
		target_compile_options(XRSDKMetaView PRIVATE /analyze)
	endif()
endif()

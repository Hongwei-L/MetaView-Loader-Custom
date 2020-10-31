# Based on CMakeLists.txt from:
# DirectX Tool Kit for DirectX 11
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT License.
# SPDX-License-Identifier: MIT
#
# http://go.microsoft.com/fwlink/?LinkId=248929

cmake_minimum_required(VERSION 3.11)

project(DirectXTK LANGUAGES CXX)

set(DirectXTK_SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/DirectXTK-aug2020)

set(LIBRARY_SOURCES
	${DirectXTK_SOURCE}/Inc/BufferHelpers.h
	${DirectXTK_SOURCE}/Inc/CommonStates.h
	${DirectXTK_SOURCE}/Inc/DDSTextureLoader.h
	${DirectXTK_SOURCE}/Inc/DirectXHelpers.h
	${DirectXTK_SOURCE}/Inc/Effects.h
	${DirectXTK_SOURCE}/Inc/GamePad.h
	${DirectXTK_SOURCE}/Inc/GeometricPrimitive.h
	${DirectXTK_SOURCE}/Inc/GraphicsMemory.h
	${DirectXTK_SOURCE}/Inc/Keyboard.h
	${DirectXTK_SOURCE}/Inc/Model.h
	${DirectXTK_SOURCE}/Inc/Mouse.h
	${DirectXTK_SOURCE}/Inc/PostProcess.h
	${DirectXTK_SOURCE}/Inc/PrimitiveBatch.h
	${DirectXTK_SOURCE}/Inc/ScreenGrab.h
	${DirectXTK_SOURCE}/Inc/SimpleMath.h
	${DirectXTK_SOURCE}/Inc/SimpleMath.inl
	${DirectXTK_SOURCE}/Inc/SpriteBatch.h
	${DirectXTK_SOURCE}/Inc/SpriteFont.h
	${DirectXTK_SOURCE}/Inc/VertexTypes.h
	${DirectXTK_SOURCE}/Inc/WICTextureLoader.h
	${DirectXTK_SOURCE}/Src/AlignedNew.h
	${DirectXTK_SOURCE}/Src/AlphaTestEffect.cpp
	${DirectXTK_SOURCE}/Src/BasicEffect.cpp
	${DirectXTK_SOURCE}/Src/BasicPostProcess.cpp
	${DirectXTK_SOURCE}/Src/Bezier.h
	${DirectXTK_SOURCE}/Src/BinaryReader.cpp
	${DirectXTK_SOURCE}/Src/BinaryReader.h
	${DirectXTK_SOURCE}/Src/BufferHelpers.cpp
	${DirectXTK_SOURCE}/Src/CommonStates.cpp
	${DirectXTK_SOURCE}/Src/dds.h
	${DirectXTK_SOURCE}/Src/DDSTextureLoader.cpp
	${DirectXTK_SOURCE}/Src/DebugEffect.cpp
	${DirectXTK_SOURCE}/Src/DemandCreate.h
	${DirectXTK_SOURCE}/Src/DGSLEffect.cpp
	${DirectXTK_SOURCE}/Src/DGSLEffectFactory.cpp
	${DirectXTK_SOURCE}/Src/DirectXHelpers.cpp
	${DirectXTK_SOURCE}/Src/DualPostProcess.cpp
	${DirectXTK_SOURCE}/Src/DualTextureEffect.cpp
	${DirectXTK_SOURCE}/Src/EffectCommon.cpp
	${DirectXTK_SOURCE}/Src/EffectCommon.h
	${DirectXTK_SOURCE}/Src/EffectFactory.cpp
	${DirectXTK_SOURCE}/Src/EnvironmentMapEffect.cpp
	${DirectXTK_SOURCE}/Src/GamePad.cpp
	${DirectXTK_SOURCE}/Src/GeometricPrimitive.cpp
	${DirectXTK_SOURCE}/Src/Geometry.h
	${DirectXTK_SOURCE}/Src/Geometry.cpp
	${DirectXTK_SOURCE}/Src/GraphicsMemory.cpp
	${DirectXTK_SOURCE}/Src/Keyboard.cpp
	${DirectXTK_SOURCE}/Src/LoaderHelpers.h
	${DirectXTK_SOURCE}/Src/Model.cpp
	${DirectXTK_SOURCE}/Src/ModelLoadCMO.cpp
	${DirectXTK_SOURCE}/Src/ModelLoadSDKMESH.cpp
	${DirectXTK_SOURCE}/Src/ModelLoadVBO.cpp
	${DirectXTK_SOURCE}/Src/Mouse.cpp
	${DirectXTK_SOURCE}/Src/NormalMapEffect.cpp
	${DirectXTK_SOURCE}/Src/PBREffect.cpp
	${DirectXTK_SOURCE}/Src/PBREffectFactory.cpp
	${DirectXTK_SOURCE}/Src/pch.h
	${DirectXTK_SOURCE}/Src/PlatformHelpers.h
	${DirectXTK_SOURCE}/Src/PrimitiveBatch.cpp
	${DirectXTK_SOURCE}/Src/ScreenGrab.cpp
	${DirectXTK_SOURCE}/Src/SDKMesh.h
	${DirectXTK_SOURCE}/Src/SharedResourcePool.h
	${DirectXTK_SOURCE}/Src/SimpleMath.cpp
	${DirectXTK_SOURCE}/Src/SkinnedEffect.cpp
	${DirectXTK_SOURCE}/Src/SpriteBatch.cpp
	${DirectXTK_SOURCE}/Src/SpriteFont.cpp
	${DirectXTK_SOURCE}/Src/TeapotData.inc
	${DirectXTK_SOURCE}/Src/ToneMapPostProcess.cpp
	${DirectXTK_SOURCE}/Src/vbo.h
	${DirectXTK_SOURCE}/Src/VertexTypes.cpp
	${DirectXTK_SOURCE}/Src/WICTextureLoader.cpp)

set(SHADER_SOURCES
	${DirectXTK_SOURCE}/Src/Shaders/AlphaTestEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/BasicEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/Common.fxh
	${DirectXTK_SOURCE}/Src/Shaders/DebugEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/DGSLEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/DGSLLambert.hlsl
	${DirectXTK_SOURCE}/Src/Shaders/DGSLPhong.hlsl
	${DirectXTK_SOURCE}/Src/Shaders/DGSLUnlit.hlsl
	${DirectXTK_SOURCE}/Src/Shaders/DualTextureEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/EnvironmentMapEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/Lighting.fxh
	${DirectXTK_SOURCE}/Src/Shaders/NormalMapEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/PBRCommon.fxh
	${DirectXTK_SOURCE}/Src/Shaders/PBREffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/PixelPacking_Velocity.hlsli
	${DirectXTK_SOURCE}/Src/Shaders/PostProcess.fx
	${DirectXTK_SOURCE}/Src/Shaders/SkinnedEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/SpriteEffect.fx
	${DirectXTK_SOURCE}/Src/Shaders/Structures.fxh
	${DirectXTK_SOURCE}/Src/Shaders/ToneMap.fx
	${DirectXTK_SOURCE}/Src/Shaders/Utilities.fxh)

add_library(
	${PROJECT_NAME} STATIC
	${LIBRARY_SOURCES}
	${DirectXTK_SOURCE}/Src/Shaders/Compiled/SpriteEffect_SpriteVertexShader.inc
)

add_custom_command(
	OUTPUT
		"${DirectXTK_SOURCE}/Src/Shaders/Compiled/SpriteEffect_SpriteVertexShader.inc"
	MAIN_DEPENDENCY "${DirectXTK_SOURCE}/Src/Shaders/CompileShaders.cmd"
	DEPENDS ${SHADER_SOURCES}
	COMMENT "Generating HLSL shaders..."
	COMMAND "CompileShaders.cmd"
	WORKING_DIRECTORY "${DirectXTK_SOURCE}/Src/Shaders"
	USES_TERMINAL)

source_group(Inc REGULAR_EXPRESSION ${DirectXTK_SOURCE}/Inc/.*)
source_group(Src REGULAR_EXPRESSION ${DirectXTK_SOURCE}/Src/.*)

target_include_directories(${PROJECT_NAME} PUBLIC ${DirectXTK_SOURCE}/Inc)
target_include_directories(${PROJECT_NAME} PRIVATE ${DirectXTK_SOURCE}/Src)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /fp:fast)

   if(ENABLE_CODE_ANALYSIS)
      target_compile_options(${PROJECT_NAME} PRIVATE /analyze)
   endif()

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.26)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zc:preprocessor /wd5104 /wd5105)
    endif()

    if (${CMAKE_SIZEOF_VOID_P} EQUAL "4")
        target_compile_options(${PROJECT_NAME} PRIVATE /arch:SSE2)
    endif()
endif()

if ( CMAKE_CXX_COMPILER_ID MATCHES "Clang" )
    set(WarningsLib "-Wpedantic" "-Wextra")
    target_compile_options(${PROJECT_NAME} PRIVATE ${WarningsLib})
endif()
if ( CMAKE_CXX_COMPILER_ID MATCHES "MSVC" )
    target_compile_options(${PROJECT_NAME} PRIVATE /permissive- /JMC- /Zc:__cplusplus)
endif()

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE UNICODE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
endif()

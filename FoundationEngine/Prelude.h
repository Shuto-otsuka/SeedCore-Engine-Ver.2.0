#pragma once
#ifndef SC_VERTION
#define SC_VERTION "2.00.0"
#endif

#ifndef SC_FOUNDATION_VERTION
#define SC_FOUNDATION_VERTION "2.00.0"
#endif

#ifndef SC_GRAPHICS_VERTION
#define SC_GRAPHICS_VERTION "2.00.0"
#endif

#ifndef SC_PHYSICS_VERTION
#define SC_PHYSICS_VERTION "2.00.0"
#endif

#ifndef SC_AI_VERTION
#define SC_AI_VERTION "2.00.0"
#endif

#ifndef SC_AUDIO_VERTION
#define SC_AUDIO_VERTION "2.00.0"
#endif

#ifndef SC_ENCRYPTION_KEY_SEED
#define SC_ENCRYPTION_KEY_SEED "8e5d9cac-4c56-4513-813c-09a0f1168a83"
#endif

#define SC_RENDER_DOC_USAGE 0

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifdef __clang__
#define __builtin_FUNCSIG() __FUNCSIG__
#endif

#ifndef JPH_DEBUG_RENDERER
#define JPH_DEBUG_RENDERER
#endif

#ifndef JPH_PROFILE_ENABLED
#define JPH_PROFILE_ENABLED
#endif

#ifndef JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
#define JPH_FLOATING_POINT_EXCEPTIONS_ENABLED
#endif

#ifndef JPH_OBJECT_STREAM
#define JPH_OBJECT_STREAM
#endif

#define SC_REFLECTION_FIELD()
#define SC_REFLECTION_FIELD_EX(displayName)

#define SC_REFLECTION_FIELD_CONDITION(...)

#define SC_REFLECTION_CLAMPED(min, max)
#define SC_REFLECTION_CLAMPED_EX(displayName, min, max)

#define SC_PAYLOAD_FIELD(assetType)
#define SC_PAYLOAD_FIELD_EX(displayName, assetType)

#define SC_SERIALIZE_FIELD()

#ifdef FOUNDATIONENGINE_EXPORTS
#define SEEDCORE_API __declspec(dllexport)
#else
#define SEEDCORE_API __declspec(dllimport)
#endif

#pragma warning(disable: 4251)
#pragma warning(disable: 4273)
#pragma warning(disable: 4275)

#include <algorithm>
#include <atomic>
#include <array>
#include <bit>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <new>
#include <optional>
#include <ostream>
#include <queue>
#include <random>
#include <ranges>
#include <source_location>
#include <span>
#include <sstream>
#include <stack>
#include <string>
#include <thread>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <variant>

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmicrosoft-include"
#pragma clang diagnostic ignored "-Wunused-command-line-argument"
#pragma clang diagnostic ignored "-Winvalid-offsetof"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wint-in-bool-context"
#endif

#ifdef _DEBUG
#if !SC_RENDER_DOC_USAGE
#pragma comment(lib, "../External/DLSS/Lib/sl.interposer.lib" )
#endif
#pragma comment(lib, "../External/SDL3/Lib/SDL3.lib")
#pragma comment(lib, "../External/DirectXTK/Lib/Debug/DirectXTK12.lib")
#pragma comment(lib, "../External/DirectXTex/Lib/Debug/DirectXTex.lib")
#pragma comment(lib, "../External/JoltPhysics/Lib/Debug/Jolt.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Debug/Effekseer.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Debug/EffekseerRendererCommon.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Debug/EffekseerRendererLLGI.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Debug/EffekseerRendererDX12.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Debug/LLGI.lib")
#pragma comment(lib, "../External/ImGui/Lib/Debug/ImGui.lib")
#pragma comment(lib, "../External/CRI/Lib/cri_ware_pcx64_le_import.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/freetyped.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/harfbuzz.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/msdfgen-core.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/msdfgen-ext.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/msdf-atlas-gen.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/libpng18_staticd.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Debug/libzsd.lib")
#pragma comment(lib, "../External/NsightAftermath/Lib/GFSDK_Aftermath_Lib.x64.lib")
#else
#if !SC_RENDER_DOC_USAGE
#pragma comment(lib, "../External/DLSS/Lib/sl.interposer.lib" )
#endif
#pragma comment(lib, "../External/SDL3/Lib/SDL3.lib")
#pragma comment(lib, "../External/DirectXTK/Lib/Release/DirectXTK12.lib")
#pragma comment(lib, "../External/DirectXTex/Lib/Release/DirectXTex.lib")
#pragma comment(lib, "../External/JoltPhysics/Lib/Release/Jolt.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Release/Effekseer.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Release/EffekseerRendererCommon.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Release/EffekseerRendererLLGI.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Release/EffekseerRendererDX12.lib")
#pragma comment(lib, "../External/Effekseer/Lib/Release/LLGI.lib")
#pragma comment(lib, "../External/ImGui/Lib/Release/ImGui.lib")
#pragma comment(lib, "../External/CRI/Lib/cri_ware_pcx64_le_import.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/freetype.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/harfbuzz.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/msdfgen-core.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/msdfgen-ext.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/msdf-atlas-gen.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/libpng18_static.lib")
#pragma comment(lib, "../External/MTSDF/Lib/Release/libzs.lib")
#pragma comment(lib, "../External/NsightAftermath/Lib/GFSDK_Aftermath_Lib.x64.lib")
#endif

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

#include <objbase.h>
#include <oleauto.h>
#include <crtdbg.h>
#include <psapi.h>

#if !SC_RENDER_DOC_USAGE
#pragma push_macro("free")
#undef free
#include <External/DLSS/Include/sl.h>
#include <External/DLSS/Include/sl_helpers.h>
#include <External/DLSS/Include/sl_dlss.h>
#include <External/DLSS/Include/sl_deepdvc.h>
#pragma pop_macro("free")
#endif

#include <External/ImGui/Include/imgui.h>
#include <External/ImGui/Include/imgui_internal.h>
#include <External/ImGui/Include/imgui_impl_dx12.h>
#include <External/ImGui/Include/imgui_impl_win32.h>
#include <External/ImGui/Include/ImGuizmo/ImGuizmo.h>
#include <External/ImGui/Include/ImNodeEditor/imgui_node_editor.h>

#include <External/SDL3/Include/SDL3/SDL.h>
#include <External/SDL3/Include/SDL3/SDL_keyboard.h>
#include <External/SDL3/Include/SDL3/SDL_mouse.h>
#include <External/SDL3/Include/SDL3/SDL_scancode.h>
#include <External/SDL3/Include/SDL3/SDL_gamepad.h>

#include <External/DirectXTK/Include/WICTextureLoader.h>
#include <External/DirectXTK/Include/DDSTextureLoader.h>
#include <External/DirectXTK/Include/ResourceUploadBatch.h>
#include <External/DirectXTK/Include/BufferHelpers.h>

#include <External/DirectXTex/Include/DirectXTex.h>

#ifdef __clang__
#define STBI_NO_SIMD
#endif
#include <External/TinyglTF/Include/tiny_gltf.h>

#include <External/NsightAftermath/Include/GFSDK_Aftermath.h>
#include <External/NsightAftermath/Include/GFSDK_Aftermath_GpuCrashDump.h>
#include <External/NsightAftermath/Include/GFSDK_Aftermath_GpuCrashDumpDecoding.h>

#include <External/RecastNavigation/Detour/Include/DetourNavMesh.h>
#include <External/RecastNavigation/DetourCrowd/Include/DetourCrowd.h>
#include <External/RecastNavigation/Recast/Include/Recast.h>

#include <External/JoltPhysics/Jolt/Jolt.h>
#include <External/JoltPhysics/Jolt/RegisterTypes.h>
#include <External/JoltPhysics/Jolt/Core/Factory.h>
#include <External/JoltPhysics/Jolt/Core/TempAllocator.h>
#include <External/JoltPhysics/Jolt/Core/JobSystemWithBarrier.h>
#include <External/JoltPhysics/Jolt/Physics/PhysicsSystem.h>
#include <External/JoltPhysics/Jolt/Physics/Body/BodyCreationSettings.h>
#include <External/JoltPhysics/Jolt/Physics/Body/BodyInterface.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/BoxShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/SphereShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/MeshShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/ConvexShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <External/JoltPhysics/Jolt/Physics/Character/CharacterVirtual.h>
#include <External/JoltPhysics/Jolt/Physics/Body/Body.h>
#include <External/JoltPhysics/Jolt/Physics/Body/BodyLock.h>
#include <External/JoltPhysics/Jolt/Physics/Body/BodyLockMulti.h>
#include <External/JoltPhysics/Jolt/Physics/Constraints/TwoBodyConstraint.h>
#include <External/JoltPhysics/Jolt/Physics/Constraints/SpringSettings.h>
#include <External/JoltPhysics/Jolt/Physics/Constraints/HingeConstraint.h>
#include <External/JoltPhysics/Jolt/Physics/Constraints/FixedConstraint.h>
#include <External/JoltPhysics/Jolt/Physics/Constraints/DistanceConstraint.h>
#include <External/JoltPhysics/Jolt/Physics/Constraints/SliderConstraint.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/RayCast.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/CastResult.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/ShapeCast.h>
#include <External/JoltPhysics/Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <External/JoltPhysics/Jolt/Physics/SoftBody/SoftBodySharedSettings.h>
#include <External/JoltPhysics/Jolt/Physics/SoftBody/SoftBodyCreationSettings.h>
#include <External/JoltPhysics/Jolt/Physics/SoftBody/SoftBodyMotionProperties.h>
#include <External/JoltPhysics/Jolt/Renderer/DebugRendererSimple.h>

#include <External/Effekseer/Include/Effekseer.h>
#include <External/Effekseer/Include/EffekseerRendererDX12.h>
#include <External/Effekseer/Include/EffekseerRendererLLGI/Common.h>

#include <External/Json/json.hpp>

#include <External/CRI/Include/cri_adx2le.h>

#ifndef MSDFGEN_PUBLIC
#define MSDFGEN_PUBLIC
#endif

#include <External/MTSDF/Include/freetype/ft2build.h>
#include FT_FREETYPE_H

#include <External/MTSDF/Include/harfbuzz/hb.h>
#include <External/MTSDF/Include/harfbuzz/hb-ft.h>

#include <External/MTSDF/Include/msdfgen/msdfgen.h>
#include <External/MTSDF/Include/msdfgen/msdfgen-ext.h>
#include <External/MTSDF/Include/msdf-atlas-gen/msdf-atlas-gen.h>

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#include <FoundationEngine/Math/Algorithm.h>
#include <FoundationEngine/Math/Vector.h>
#include <FoundationEngine/Math/Quaternion.h>
#include <FoundationEngine/Math/Matrix.h>

#include <FoundationEngine/Utility/Types.h>
#include <FoundationEngine/Utility/String.h>
#include <FoundationEngine/Utility/Array.h>
#include <FoundationEngine/Utility/Bitset.h>
#include <FoundationEngine/Utility/ResourcePtr.h>
#include <FoundationEngine/Utility/ResourceRef.h>
#include <FoundationEngine/Utility/NonCopyable.h>
#include <FoundationEngine/Utility/NonMovable.h>
#include <FoundationEngine/Utility/NonTransferable.h>
#include <FoundationEngine/Utility/DestructiveCopy.h>
#include <FoundationEngine/Utility/Delegate.h>

#include <GraphicsEngine/D3D12/D3D12Common.h>

#define in    _In_
#define out   _Out_
#define inout _Inout_
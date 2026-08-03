// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2026 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <External/JoltPhysics/Jolt/Jolt.h>

#ifdef JPH_USE_CPU_COMPUTE

#include <External/JoltPhysics/Jolt/Shaders/HairWrapper.h>

#define JPH_SHADER_NAME HairTeleport
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairTeleport.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairTeleportBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairApplyDeltaTransform
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairApplyDeltaTransform.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairApplyDeltaTransformBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairSkinVertices
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairSkinVertices.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairSkinVerticesBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairSkinRoots
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairSkinRoots.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairSkinRootsBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairApplyGlobalPose
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairApplyGlobalPose.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairApplyGlobalPoseBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairCalculateCollisionPlanes
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairCalculateCollisionPlanes.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairCalculateCollisionPlanesBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairGridClear
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairGridClear.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairGridClearBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairGridAccumulate
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairGridAccumulate.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairGridAccumulateBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairGridNormalize
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairGridNormalize.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairGridNormalizeBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairIntegrate
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairIntegrate.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairIntegrateBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairUpdateRoots
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairUpdateRoots.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairUpdateRootsBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairUpdateStrands
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairUpdateStrands.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairUpdateStrandsBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairUpdateVelocity
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairUpdateVelocity.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairUpdateVelocityBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairUpdateVelocityIntegrate
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairUpdateVelocityIntegrate.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairUpdateVelocityIntegrateBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

#define JPH_SHADER_NAME HairCalculateRenderPositions
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBegin.h>
#include "HairCalculateRenderPositions.hlsl"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderBindings.h>
#include "HairCalculateRenderPositionsBindings.h"
#include <External/JoltPhysics/Jolt/Compute/CPU/WrapShaderEnd.h>

JPH_NAMESPACE_BEGIN

void JPH_EXPORT HairRegisterShaders(ComputeSystemCPU *inComputeSystem)
{
	JPH_REGISTER_SHADER(inComputeSystem, HairTeleport);
	JPH_REGISTER_SHADER(inComputeSystem, HairApplyDeltaTransform);
	JPH_REGISTER_SHADER(inComputeSystem, HairSkinVertices);
	JPH_REGISTER_SHADER(inComputeSystem, HairSkinRoots);
	JPH_REGISTER_SHADER(inComputeSystem, HairApplyGlobalPose);
	JPH_REGISTER_SHADER(inComputeSystem, HairCalculateCollisionPlanes);
	JPH_REGISTER_SHADER(inComputeSystem, HairGridClear);
	JPH_REGISTER_SHADER(inComputeSystem, HairGridAccumulate);
	JPH_REGISTER_SHADER(inComputeSystem, HairGridNormalize);
	JPH_REGISTER_SHADER(inComputeSystem, HairIntegrate);
	JPH_REGISTER_SHADER(inComputeSystem, HairUpdateRoots);
	JPH_REGISTER_SHADER(inComputeSystem, HairUpdateStrands);
	JPH_REGISTER_SHADER(inComputeSystem, HairUpdateVelocity);
	JPH_REGISTER_SHADER(inComputeSystem, HairUpdateVelocityIntegrate);
	JPH_REGISTER_SHADER(inComputeSystem, HairCalculateRenderPositions);
}

JPH_NAMESPACE_END

#endif // JPH_USE_CPU_COMPUTE

// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <External/JoltPhysics/Jolt/Jolt.h>

#include <External/JoltPhysics/Jolt/Physics/DeterminismLog.h>

#ifdef JPH_ENABLE_DETERMINISM_LOG

JPH_NAMESPACE_BEGIN

DeterminismLog DeterminismLog::sLog;

JPH_NAMESPACE_END

#endif // JPH_ENABLE_DETERMINISM_LOG

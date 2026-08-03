// Jolt Physics Library (https://github.com/jrouwe/JoltPhysics)
// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <External/JoltPhysics/Jolt/Jolt.h>

#include <External/JoltPhysics/Jolt/Physics/Collision/GroupFilterTable.h>
#include <External/JoltPhysics/Jolt/ObjectStream/TypeDeclarations.h>
#include <External/JoltPhysics/Jolt/Core/StreamIn.h>
#include <External/JoltPhysics/Jolt/Core/StreamOut.h>

JPH_NAMESPACE_BEGIN

JPH_IMPLEMENT_SERIALIZABLE_VIRTUAL(GroupFilterTable)
{
	JPH_ADD_BASE_CLASS(GroupFilterTable, GroupFilter)

	JPH_ADD_ATTRIBUTE(GroupFilterTable, mNumSubGroups)
	JPH_ADD_ATTRIBUTE(GroupFilterTable, mTable)
}

void GroupFilterTable::SaveBinaryState(StreamOut &inStream) const
{
	GroupFilter::SaveBinaryState(inStream);

	inStream.Write(mNumSubGroups);
	inStream.Write(mTable);
}

void GroupFilterTable::RestoreBinaryState(StreamIn &inStream)
{
	GroupFilter::RestoreBinaryState(inStream);

	inStream.Read(mNumSubGroups);
	inStream.Read(mTable);
}

JPH_NAMESPACE_END

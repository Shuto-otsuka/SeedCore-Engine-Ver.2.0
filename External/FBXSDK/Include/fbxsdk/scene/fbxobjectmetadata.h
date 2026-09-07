/****************************************************************************************
 
   Copyright (C) 2015 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxobjectmetadata.h
#ifndef _FBXSDK_SCENE_OBJECT_META_DATA_H_
#define _FBXSDK_SCENE_OBJECT_META_DATA_H_

#include <External/FBXSDK/Include/fbxsdk/fbxsdk_def.h>

#include <External/FBXSDK/Include/fbxsdk/core/fbxobject.h>

#include <External/FBXSDK/Include/fbxsdk/fbxsdk_nsbegin.h>

/** This class is used to hold meta-data information on nodes.
  * \nosubgrouping
  *
  * This class does not offer any new functionality over a regular FbxObject;
  * all meta-data information should be stored in properties.
  * 
  */
class FBXSDK_DLL FbxObjectMetaData : public FbxObject
{
	FBXSDK_OBJECT_DECLARE(FbxObjectMetaData, FbxObject);
};

#include <External/FBXSDK/Include/fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_SCENE_OBJECT_META_DATA_H_ */

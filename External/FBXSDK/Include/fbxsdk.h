/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxsdk.h
#ifndef _FBXSDK_H_
#define _FBXSDK_H_

/**
  * \mainpage FBX SDK Reference
  * <p>
  * \section welcome Welcome to the FBX SDK Reference
  * The FBX SDK Reference contains reference information on every header file, 
  * namespace, class, method, enum, typedef, variable, and other C++ elements 
  * that comprise the FBX software development kit (SDK).
  * <p>
  * The FBX SDK Reference is organized into the following sections:
  * <ul><li>Class List: an alphabetical list of FBX SDK classes
  *     <li>Class Hierarchy: a textual representation of the FBX SDK class structure
  *     <li>Graphical Class Hierarchy: a graphical representation of the FBX SDK class structure
  *     <li>File List: an alphabetical list of all documented header files</ul>
  * <p>
  * \section otherdocumentation Other Documentation
  * Apart from this reference guide, an FBX SDK Programming Guide and many FBX 
  * SDK examples are also provided.
  * <p>
  * \section aboutFBXSDK About the FBX SDK
  * The FBX SDK is a C++ software development kit (SDK) that lets you import 
  * and export 3D scenes using the Autodesk FBX file format. The FBX SDK 
  * reads FBX files created with FiLMBOX version 2.5 and later and writes FBX 
  * files compatible with MotionBuilder version 6.0 and up. 
  */

#include <External/FBXSDK/Include/fbxsdk/fbxsdk_def.h>

#ifndef FBXSDK_NAMESPACE_USING
	#define FBXSDK_NAMESPACE_USING 1
#endif

//---------------------------------------------------------------------------------------
//Core Base Includes
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxarray.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxbitset.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxcharptrset.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxcontainerallocators.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxdynamicarray.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxstatus.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxfile.h>
#ifndef FBXSDK_ENV_WINSTORE
	#include <External/FBXSDK/Include/fbxsdk/core/base/fbxfolder.h>
#endif
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxhashmap.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxintrusivelist.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxmap.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxmemorypool.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxpair.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxset.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxstring.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxstringlist.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxtime.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxtimecode.h>
#include <External/FBXSDK/Include/fbxsdk/core/base/fbxutils.h>

//---------------------------------------------------------------------------------------
//Core Math Includes
#include <External/FBXSDK/Include/fbxsdk/core/math/fbxmath.h>
#include <External/FBXSDK/Include/fbxsdk/core/math/fbxdualquaternion.h>
#include <External/FBXSDK/Include/fbxsdk/core/math/fbxmatrix.h>
#include <External/FBXSDK/Include/fbxsdk/core/math/fbxquaternion.h>
#include <External/FBXSDK/Include/fbxsdk/core/math/fbxvector2.h>
#include <External/FBXSDK/Include/fbxsdk/core/math/fbxvector4.h>

//---------------------------------------------------------------------------------------
//Core Sync Includes
#ifndef FBXSDK_ENV_WINSTORE
	#include <External/FBXSDK/Include/fbxsdk/core/sync/fbxatomic.h>
	#include <External/FBXSDK/Include/fbxsdk/core/sync/fbxclock.h>
	#include <External/FBXSDK/Include/fbxsdk/core/sync/fbxsync.h>
	#include <External/FBXSDK/Include/fbxsdk/core/sync/fbxthread.h>
#endif /* !FBXSDK_ENV_WINSTORE */

//---------------------------------------------------------------------------------------
//Core Includes
#include <External/FBXSDK/Include/fbxsdk/core/fbxclassid.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxconnectionpoint.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxdatatypes.h>
#ifndef FBXSDK_ENV_WINSTORE
	#include <External/FBXSDK/Include/fbxsdk/core/fbxmodule.h>
	#include <External/FBXSDK/Include/fbxsdk/core/fbxloadingstrategy.h>
#endif /* !FBXSDK_ENV_WINSTORE */
#include <External/FBXSDK/Include/fbxsdk/core/fbxmanager.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxobject.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxperipheral.h>
#ifndef FBXSDK_ENV_WINSTORE
	#include <External/FBXSDK/Include/fbxsdk/core/fbxplugin.h>
	#include <External/FBXSDK/Include/fbxsdk/core/fbxplugincontainer.h>
#endif /* !FBXSDK_ENV_WINSTORE */
#include <External/FBXSDK/Include/fbxsdk/core/fbxproperty.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxpropertydef.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxpropertyhandle.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxpropertypage.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxpropertytypes.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxquery.h>
#include <External/FBXSDK/Include/fbxsdk/core/fbxqueryevent.h>
#ifndef FBXSDK_ENV_WINSTORE
	#include <External/FBXSDK/Include/fbxsdk/core/fbxscopedloadingdirectory.h>
	#include <External/FBXSDK/Include/fbxsdk/core/fbxscopedloadingfilename.h>
#endif /* !FBXSDK_ENV_WINSTORE */
#include <External/FBXSDK/Include/fbxsdk/core/fbxxref.h>

//---------------------------------------------------------------------------------------
//File I/O Includes
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxexporter.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxexternaldocreflistener.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxfiletokens.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxglobalcamerasettings.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxgloballightsettings.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxgobo.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbximporter.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxiobase.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxiopluginregistry.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxiosettings.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxstatisticsfbx.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxstatistics.h>
#include <External/FBXSDK/Include/fbxsdk/fileio/fbxcallbacks.h>

//---------------------------------------------------------------------------------------
//Scene Includes
#include <External/FBXSDK/Include/fbxsdk/scene/fbxaudio.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxaudiolayer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxcollection.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxcollectionexclusive.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxcontainer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxcontainertemplate.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxdisplaylayer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxdocument.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxdocumentinfo.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxenvironment.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxgroupname.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxlibrary.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxmediaclip.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxobjectmetadata.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxpose.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxreference.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxscene.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxselectionset.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxselectionnode.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxtakeinfo.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxthumbnail.h>
#include <External/FBXSDK/Include/fbxsdk/scene/fbxvideo.h>

//---------------------------------------------------------------------------------------
//Scene Animation Includes
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimcurve.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimcurvebase.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimcurvefilters.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimcurvenode.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimevalclassic.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimevalstate.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimevaluator.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimlayer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimstack.h>
#include <External/FBXSDK/Include/fbxsdk/scene/animation/fbxanimutilities.h>

//---------------------------------------------------------------------------------------
//Scene Constraint Includes
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxcharacternodename.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxcharacter.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxcharacterpose.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraint.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintaim.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintcustom.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintparent.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintposition.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintrotation.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintscale.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintsinglechainik.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxconstraintutils.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxcontrolset.h>
#include <External/FBXSDK/Include/fbxsdk/scene/constraint/fbxhik2fbxcharacter.h>

//---------------------------------------------------------------------------------------
//Scene Geometry Includes
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxblendshape.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxblendshapechannel.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxcache.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxcachedeffect.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxcamera.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxcamerastereo.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxcameraswitcher.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxcluster.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxdeformer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxgenericnode.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxgeometry.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxgeometrybase.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxgeometryweightedmap.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxlight.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxlimitsutilities.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxline.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxlodgroup.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxmarker.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxmesh.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxnode.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxnodeattribute.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxnull.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxnurbs.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxnurbscurve.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxnurbssurface.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxopticalreference.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxpatch.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxproceduralgeometry.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxshape.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxskeleton.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxskin.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxsubdeformer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxsubdiv.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxtrimnurbssurface.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxvertexcachedeformer.h>
#include <External/FBXSDK/Include/fbxsdk/scene/geometry/fbxweightedmapping.h>

//---------------------------------------------------------------------------------------
//Scene Shading Includes
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxshadingconventions.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxbindingsentryview.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxbindingtable.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxbindingtableentry.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxbindingoperator.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxconstantentryview.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxentryview.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxfiletexture.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbximplementation.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbximplementationfilter.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbximplementationutils.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxlayeredtexture.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxoperatorentryview.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxproceduraltexture.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxpropertyentryview.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxsemanticentryview.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxsurfacelambert.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxsurfacematerial.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxsurfacematerialutils.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxsurfacephong.h>
#include <External/FBXSDK/Include/fbxsdk/scene/shading/fbxtexture.h>

//---------------------------------------------------------------------------------------
//Utilities Includes
#include <External/FBXSDK/Include/fbxsdk/utils/fbxdeformationsevaluator.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxprocessor.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxprocessorxref.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxprocessorxrefuserlib.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxprocessorshaderdependency.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxclonemanager.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxgeometryconverter.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxmanipulators.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxmaterialconverter.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxrenamingstrategyfbx5.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxrenamingstrategyfbx6.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxrenamingstrategyutilities.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxrootnodeutility.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxusernotification.h>
#include <External/FBXSDK/Include/fbxsdk/utils/fbxscenecheckutility.h>

//---------------------------------------------------------------------------------------
#if defined(FBXSDK_NAMESPACE) && (FBXSDK_NAMESPACE_USING == 1)
	using namespace FBXSDK_NAMESPACE;
#endif

#endif /* _FBXSDK_H_ */

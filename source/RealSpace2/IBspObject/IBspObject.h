#pragma once

#include "stdafx.h"
#include <stdio.h>
#include <list>
#include "RTypes.h"
#include "RSolidBsp.h"




namespace RealSpace2
{
	enum ROpenFlag;
	struct FogInfo;
	struct AmbSndInfo;
	struct RBSPMATERIAL;
	struct RBSPPICKINFO;
	class RLightList;
	class RSBspNode;

	class RMapObjectList;
	class RMeshMgr;
	class RDummyList;
}
class RNavigationMesh;

using namespace RealSpace2;

/*
 * IBspObject is the interface to RBspObject
 * This interface provides the functions declarations
 * used by Gunz.
 */
class IBspObject
{
public:
	IBspObject(){};
	virtual ~IBspObject(){};

	virtual void Release() = 0;

	virtual bool Open(const char *fileName, const char * ext) = 0;
	virtual bool Open(const char *fileName, const char* descExt, ROpenFlag nOpenFlag, RFPROGRESSCALLBACK pfnProgressCallback=NULL, void *CallbackParam=NULL) = 0;

	virtual void OptimizeBoundingBox() = 0;
	
	virtual bool IsVisible(rboundingbox &bb) = 0;

	virtual bool Draw() = 0;
	virtual void DrawObjects() = 0;


	virtual rvector GetFloor(rvector &origin,float fRadius,float fHeight,rplane *pimpactplane=NULL) = 0;
	virtual bool Pick(const rvector &pos, const rvector &dir, RBSPPICKINFO *pOut, DWORD dwPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_USEOPACITY | RM_FLAG_HIDE) = 0;
	virtual bool PickTo(const rvector &pos, const rvector &to, RBSPPICKINFO *pOut, DWORD dwPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_USEOPACITY | RM_FLAG_HIDE) = 0;
	virtual bool CheckWall(rvector &origin,rvector &targetpos,float fRadius,float fHeight=0.f,RCOLLISIONMETHOD method=RCW_CYLINDER,int nDepth=0,rplane *pimpactplane=NULL) = 0;

	virtual RBSPMATERIAL * GetMaterial(int nIndex) = 0;
	virtual RBSPMATERIAL * GetMaterial(RSBspNode *pNode, int nIndex) = 0;


	virtual void OnInvalidate() = 0;
	virtual void OnRestore() = 0;





	virtual bool CheckSolid(rvector &pos,float fRadius,float fHeight=0.f,RCOLLISIONMETHOD method=RCW_CYLINDER) = 0;
	virtual void DrawLight(D3DLIGHT9 *pLight) = 0;
	virtual RLightList * GetObjectLightList() = 0;
	virtual RMapObjectList * GetMapObjectList() = 0;
	virtual RealSpace2::RMeshMgr * GetMeshManager() = 0;
	virtual FogInfo GetFogInfo() = 0;
	virtual RDummyList * GetDummyList() = 0;
	virtual list<AmbSndInfo *> GetAmbSndList() = 0;
	virtual void SetWireframeMode(bool bWireframe) = 0;
	virtual RNavigationMesh * GetNavigationMesh() = 0;
	virtual bool GetShadowPosition( rvector& pos_, rvector& dir_, rvector* outNormal_, rvector* outPos_ ) = 0;
	virtual RLightList * GetSunLightList() = 0;
	virtual void LightMapOnOff(bool b) = 0;
	virtual void SetMapObjectOcclusion(bool b) = 0;
	virtual RSBspNode * GetRootNode() = 0;

};
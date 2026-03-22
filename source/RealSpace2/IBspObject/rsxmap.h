#pragma once
#include "stdafx.h"
#include "RBspObject.h"
#include "../../rsx/Interface.h"

_NAMESPACE_REALSPACE2_BEGIN

class DumpOnDestruct
{
public:
    ~DumpOnDestruct();
};

class RSXMap : public IBspObject
{
	RDummyList m_DummyList;

    std::vector<IObject*> vobjects;
    std::vector<ILight*> vlights;
    //DumpOnDestruct dump___;

    void updateMatrices();

public:

	void Release();

	bool LoadDummy(const char * name);

	bool Draw();
	void DrawObjects();

	void OnInvalidate();
	void OnRestore();


	bool Open(const char *fileName, const char * ext);
	bool Open(const char *fileName, const char* descExt, ROpenFlag nOpenFlag, RFPROGRESSCALLBACK pfnProgressCallback=NULL, void *CallbackParam=NULL);

	void OptimizeBoundingBox();
	bool IsVisible(rboundingbox &bb);




	rvector GetFloor(rvector &origin,float fRadius,float fHeight,rplane *pimpactplane=NULL);
	bool Pick(const rvector &pos, const rvector &dir, RBSPPICKINFO *pOut, DWORD dwPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_USEOPACITY | RM_FLAG_HIDE);
	bool PickTo(const rvector &pos, const rvector &to, RBSPPICKINFO *pOut, DWORD dwPassFlag=RM_FLAG_ADDITIVE | RM_FLAG_USEOPACITY | RM_FLAG_HIDE);
	bool CheckWall(rvector &origin,rvector &targetpos,float fRadius,float fHeight=0.f,RCOLLISIONMETHOD method=RCW_CYLINDER,int nDepth=0,rplane *pimpactplane=NULL);

	RBSPMATERIAL * GetMaterial(int nIndex);
	RBSPMATERIAL * GetMaterial(RSBspNode *pNode, int nIndex);


	bool CheckSolid(rvector &pos,float fRadius,float fHeight=0.f,RCOLLISIONMETHOD method=RCW_CYLINDER);
	void DrawLight(D3DLIGHT9 *pLight);
	RLightList * GetObjectLightList();
	RMapObjectList * GetMapObjectList();
	RealSpace2::RMeshMgr * GetMeshManager();
	FogInfo GetFogInfo();
	RDummyList * GetDummyList();
	list<AmbSndInfo *> GetAmbSndList();
	void SetWireframeMode(bool bWireframe);
	RNavigationMesh * GetNavigationMesh();
	bool GetShadowPosition( rvector& pos_, rvector& dir_, rvector* outNormal_, rvector* outPos_ );
	RLightList * GetSunLightList();
	void LightMapOnOff(bool b);
	void SetMapObjectOcclusion(bool b);
	RSBspNode * GetRootNode();
};

_NAMESPACE_REALSPACE2_END
#pragma once

//////////////////////////////////////////////////////////////////////////
// working specification( progammer note )
// shadow로 쓰일 사각 폴리곤 하나를 생성한다(global) + texture
// character의 foot position을 얻어올수 있다 ( 2개)
// map의 현재 발 포지션의 맵의 floor 좌표와 map의 폴리곤의 normal을 얻어올수 있다
// floor의 좌표와 charactoer의 foot position의 거리를 체크해서 그려줄 것인지를 결정한다
// shadow폴리곤의 normal과 map의 폴리곤의 normal을 일치하도록 transformation 시켜준다
// 그려준다
//////////////////////////////////////////////////////////////////////////

#define	ENABLE_SHOWDOW
#include "zapplication.h"
#include "RTypes.h"
#include "RMeshUtil.h"
#include "RBaseTexture.h"

class ZCharacter;

_USING_NAMESPACE_REALSPACE2

class ZShadow
{
public:
	ZShadow(void);
	virtual ~ZShadow(void);

private:	

	bool bLFShadow;
	bool bRFShadow;

	rmatrix mWorldMatLF;	//  왼쪽발
	rmatrix mWorldMatRF;	// 오른쪽발

	static rvector mNormal;

	float mfDistance;	// 왼쪽발과 오른쪽 발의 바닥으로부터의 거리의 제곱의 평균

public:
	bool setMatrix( ZCharacterObject& char_, float size_ = 100.0f );	// 매 프레임마다 불려져야 함
	bool setMatrix( RVisualMesh& vmesh, float size_ = 100.f, IBspObject* p_map  = 0 );
	void draw(bool bForced=false);

private:
	rmatrix setSize( float size_ ); // size_ should be bigger than 0
	rmatrix setDirection( rvector& dir_ );
};
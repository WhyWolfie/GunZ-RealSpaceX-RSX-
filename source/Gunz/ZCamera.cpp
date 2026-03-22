#include "stdafx.h"

#include "ZGame.h"
#include "ZCamera.h"
#include "RealSpace2.h"
#include "IBspObject.h"
#include "ZApplication.h"
#include "ZObserver.h"
#include "ZMatch.h"
#include "ZGameClient.h"
#include "ZMyCharacter.h"
#include "ZCharacter.h"
#include "Config.h"
#include "ZMyInfo.h"
#include "ZGameConst.h"
#include "ZInput.h"

#define CAMERA_TRACKSPEED	.20f
#define CAMERA_WALL_TRACKSPEED		0.7f



ZCamera::ZCamera() : m_fAngleX(CAMERA_DEFAULT_ANGLEX) , m_fCurrentAngleX(CAMERA_DEFAULT_ANGLEX) , 
					m_fAngleZ(CAMERA_DEFAULT_ANGLEZ) , m_fCurrentAngleZ(CAMERA_DEFAULT_ANGLEZ) , 
					m_fDist(CAMERA_DEFAULT_DISTANCE) , m_fCurrentDist(CAMERA_DEFAULT_DISTANCE) ,
					m_fRecoilAngleX(0.0f),
					m_fRecoilAngleZ(0.0f),
					m_fMaxRecoilAngleX(0.0f),
					m_bShocked(false),
					m_Position(0,0,0),
					m_bAutoAiming(false),
					m_nLookMode(ZCAMERA_DEFAULT)
{

	m_vCameraTrackOffset = rvector(0,0,0);
}

rvector ZCamera::GetCurrentDir()
{
	return rvector(cosf(m_fCurrentAngleZ)*sinf(m_fCurrentAngleX),
						sinf(m_fCurrentAngleZ)*sinf(m_fCurrentAngleX),
						cosf(m_fCurrentAngleX));
}


void ZCamera::Update(float fElapsed)
{
	if (ZGetGame() == NULL) return;

	/////////////////////////////
	//////////////////////////// 먼저 target 을 설정한다
	ZCombatInterface*	pCombatInterface = ZGetGameInterface()->GetCombatInterface();
	ZCharacter*			pTargetCharacter = ZGetGame()->m_pMyCharacter;

	{
		rvector position,direction;
		position=pTargetCharacter->GetPosition();
		direction=pTargetCharacter->m_TargetDir;

		// 옵져버 모드
		if (pCombatInterface->GetObserver()->IsVisible()) 
		{
			ZCharacter *pObserverTargetCharacter = pCombatInterface->GetObserver()->GetTargetCharacter();
			if (pObserverTargetCharacter) 
				pTargetCharacter = pObserverTargetCharacter;

			if(pTargetCharacter)
			{
				pTargetCharacter->GetHistory(&position,&direction,ZGetGame()->GetTime()-pCombatInterface->GetObserver()->GetDelay());
				//direction = m_pMyCharacter;

				if (GetLookMode() == ZCAMERA_DEFAULT)
				{
//					direction = *pCombatInterface->GetObserver()->GetFreeLookTarget();
				}
				//direction = m_pMyCharacter->m_TargetDir;
			}
		}
		if(!pTargetCharacter) return;

		rvector right;
		rvector forward=direction;

		CrossProduct(&right,rvector(0,0,1),forward);

		// 캐릭터 중심의 좌표계.. x y z 축이 앞/오른쪽/위 로 대응

	#define CAMERA_TRACK_OFFSET		rvector(0,0,160)		// 머리위치 하드코드..


		// 카메라는 오른손쪽에 중심을 맞춘다.
	#define CAMERA_RIGHT_OFFSET		rvector(0,30.f,10.f)

		// 카메라를 위에 있을때 앞으로 나가는 만큼
	#define CAMERA_UP_OFFSET	rvector(50.f,0.f,0.f)

		// 카메라를 아래 있을때 앞으로 나가는 만큼
	#define CAMERA_DOWN_OFFSET	rvector(50.f,0,0.f)


		/*	TODO: 카메라가 발위치 + (0,0,160) (머리부근) 으로 고정했는데, 애니메이션에
			카메라의 더미가 있어서 추적할 위치를 애니메이션으로부터 얻어내면 좋은 카메라
			액션을 만들수 있다.
		*/ 


		m_vCameraTrackOffset = CAMERA_TRACK_OFFSET;

		m_vCameraTrackOffset += CAMERA_RIGHT_OFFSET.x*forward+CAMERA_RIGHT_OFFSET.y*right+CAMERA_RIGHT_OFFSET.z*rvector(0,0,1);


		m_Target = position + m_vCameraTrackOffset;
	}

	m_bCollision[0] = m_bCollision[1] = m_bCollision[2] = m_bCollision[3] = false;

	float fTrackSpeeed=CAMERA_TRACKSPEED;

	fTrackSpeeed=1.f;


	rvector up(0,0,1);

	rvector dir=GetCurrentDir();

	ZObserver* pObserver = ZGetCombatInterface()->GetObserver();
	if (pObserver->IsVisible() && GetLookMode() == ZCAMERA_DEFAULT)
	{
		if (pObserver->GetTargetCharacter() != NULL)
		{
			ZCharacter *pTargetCharacter=pObserver->GetTargetCharacter();
			rvector a_pos;
			rvector a_dir;
			if(pTargetCharacter->GetHistory(&a_pos,&a_dir,ZGetGame()->GetTime()-pObserver->GetDelay()))
			{
				dir = a_dir;

				// 방향은 다음과 같으므로 m_fAngleX 과 m_fAngleZ 를 dir 로 부터 구한다
//				rvector(cosf(m_fCurrentAngleZ)*sinf(m_fCurrentAngleX),
//						sinf(m_fCurrentAngleZ)*sinf(m_fCurrentAngleX),
//						cosf(m_fCurrentAngleX));

				m_fAngleX = acosf(dir.z);
				a_dir.z = 0;
				m_fAngleZ = GetAngleOfVectors(rvector(1,0,0),a_dir);
			}
		}
	}

	D3DXVec3Normalize(&dir,&dir);
	rvector right;
	D3DXVec3Cross(&right,&dir,&up);
	D3DXVec3Normalize(&right,&right);

	float fAngleX = m_fAngleX;
	float fAngleZ = m_fAngleZ;

#ifdef _RECOIL	// 기획상 삭제됨 - 버드
	// 반동 처리
	float fAngleX = m_fAngleX - m_fRecoilAngleX;
	m_fRecoilAngleX = m_fRecoilAngleX - 0.005f;
	if (m_fRecoilAngleX < 0.0f) m_fRecoilAngleX = 0.0f;

	float fAngleZ = m_fAngleZ + m_fRecoilAngleZ;
	if (m_fRecoilAngleZ > 0)
	{
		m_fRecoilAngleZ = m_fRecoilAngleZ - 0.01f;
		if (m_fRecoilAngleZ < 0.0f) m_fRecoilAngleZ = 0.0f;
	}
	else
	{
		m_fRecoilAngleZ = m_fRecoilAngleZ + 0.01f;
		if (m_fRecoilAngleZ > 0.0f) m_fRecoilAngleZ = 0.0f;
	}
#endif

	rvector shockoffset=rvector(0,0,0);
	if (m_bShocked)
	{
		float fA=RANDOMFLOAT*2*pi;
		float fB=RANDOMFLOAT*2*pi;
		rvector velocity=rvector(sin(fA)*sin(fB),cos(fA)*sin(fB),cos(fB));

		float fPower=(ZGetGame()->GetTime()-m_fShockStartTime)/m_fShockDuration;
		if(fPower>1.f)
			StopShock();
		else
		{
			fPower=1.f-fPower;

			{	// force feedback

				
				// 이 값을 넘어가는 파워는 최대진동, 이하 선형감소
				const float FFMAX_POWER = 300.f;	// 적당한 세기를 넣은것.

				float ffPower = fPower * m_fShockPower / FFMAX_POWER;
				ffPower = min(ffPower,1);
				ZGetInput()->SetDeviceForcesXY(ffPower,ffPower);
			}

			fPower=pow(fPower,1.5f);
			//fPower=cos(fPower*pi/2.f);
			rvector backdir=-m_CameraShockOffset;
			Normalize(backdir);
			m_CameraShockVelocity=(RANDOMFLOAT*m_fShockPower*velocity+m_fShockPower*backdir);
			m_CameraShockOffset+=fElapsed*m_CameraShockVelocity;
			shockoffset=fPower*m_CameraShockOffset;
		}
	}


	float fRealDist=m_fDist;
	rvector pos=m_CurrentTarget-dir*m_fDist;

	RBSPPICKINFO bpi;
//	int nCount=0;

	if(ZGetGame()->GetWorld()->GetBsp()->Pick(m_Target,-dir,&bpi)
		&& Magnitude(m_Target-bpi.PickPos)<Magnitude(m_Target-pos))
	{
		float fColDist=Magnitude(bpi.PickPos-pos);
		float fTargetDist=Magnitude(m_Target-pos);
		pos=bpi.PickPos+dir;

		fRealDist=Magnitude(m_Target-pos)-10.f;
	}

	bool bCollisionWall = CheckCollisionWall(fRealDist, pos, dir);

	m_CurrentTarget+=fTrackSpeeed*(m_Target-m_CurrentTarget);
	m_fCurrentAngleX+=fTrackSpeeed*(fAngleX-m_fCurrentAngleX);
	m_fCurrentAngleZ+=fTrackSpeeed*(fAngleZ-m_fCurrentAngleZ);

//	m_fCurrentDist+=CAMERA_TRACKSPEED*(fRealDist-m_fCurrentDist);

	float fAddedZ = 0.0f;
	CalcMaxPayneCameraZ(fRealDist, fAddedZ, fAngleX);

	if (bCollisionWall)
	{
		//m_fCurrentDist = fRealDist;
		m_fCurrentDist+=CAMERA_WALL_TRACKSPEED*(fRealDist-m_fCurrentDist);
	}
	else
	{
		m_fCurrentDist+=CAMERA_TRACKSPEED*(fRealDist-m_fCurrentDist);
	}

	if( GetLookMode()==ZCAMERA_DEFAULT || GetLookMode()==ZCAMERA_FREEANGLE ) {

		m_Position=m_CurrentTarget-dir*m_fCurrentDist;

		//if (!pMyCharacter->m_bWallHang)
		{
			m_Position.z += fAddedZ;
		}

		// 카메라는 나락 밑으로 내려갈 수 없다.
		if (m_Position.z <= DIE_CRITICAL_LINE) 
		{
			rvector campos = pTargetCharacter->GetCenterPos() - pTargetCharacter->GetDirection() * 20.0f;
			m_Position.x = campos.x;
			m_Position.y = campos.y;
			m_Position.z = DIE_CRITICAL_LINE;
			

			if (GetLookMode() == ZCAMERA_DEFAULT)
			{
				rvector tar = pTargetCharacter->GetCenterPos();
				if (tar.z < (DIE_CRITICAL_LINE-1000.0f)) tar.z = DIE_CRITICAL_LINE-1000.0f;
				dir = tar - m_Position;
				Normalize(dir);			
			}
		}
	}

	if (m_bShocked)
	{
		rvector CameraPos = m_Position + shockoffset;
		RSetCamera(CameraPos,CameraPos+dir,up);
	}
	else
	{
		RSetCamera(m_Position,m_Position+dir,up);
	}

	if (GetLookMode()==ZCAMERA_FREELOOK) 
	{
		if(!_isnan(RCameraDirection.x) && !_isnan(RCameraDirection.y) && !_isnan(RCameraDirection.z) )
		{
			ZCombatInterface*	pCombatInterface = ZGetGameInterface()->GetCombatInterface();
			pCombatInterface->GetObserver()->SetFreeLookTarget(rvector(RCameraDirection.x,RCameraDirection.y,RCameraDirection.z) );
		}
	}
}


void ZCamera::RecoilAngle(float fRecoilAngleX, float fRecoilAngleZ)
{
	m_fRecoilAngleX += fRecoilAngleX;

	if (m_fRecoilAngleX > m_fMaxRecoilAngleX)
	{
		m_fRecoilAngleX = m_fMaxRecoilAngleX;
	}

	m_fRecoilAngleZ = fRecoilAngleZ;
}

void ZCamera::SetMaxRecoilAngleX(float fMax)
{
	m_fMaxRecoilAngleX = fMax;
}

void ZCamera::Shock(float fPower, float fDuration,rvector& vDir)
{
	m_bShocked = true;

	m_fShockStartTime=ZGetGame()->GetTime();
	m_fShockPower=fPower;
	m_fShockDuration=fDuration;
	m_CameraShockOffset=rvector(0,0,0);
	m_CameraShockVelocity=rvector(0,0,0);
}

void ZCamera::StopShock()
{
	m_bShocked = false;
	ZGetInput()->SetDeviceForcesXY(0,0);
}

void ZCamera::SetDirection(rvector& dir)
{
	rvector a_dir = dir;
	Normalize(a_dir);

	float fAngleX=0.0f, fAngleZ=0.0f;

	fAngleX = acosf(a_dir.z);
	float fSinX = sinf(fAngleX);

	if (fSinX == 0) fAngleZ = 0.0f;
	else 
	{
		float fT = (a_dir.x / fSinX);
		if (fT > 1.0f) fT = 1.0f;
		else if (fT < -1.0f) fT = -1.0f;

		float fZ1 = acosf( fT );
		
		if (IS_EQ((sinf(fZ1) * fSinX), dir.y))
		{
			fAngleZ = fZ1;
		}
		else
		{
			fAngleZ = 2 * pi - fZ1;
		}

	}

	m_fCurrentAngleX = m_fAngleX = fAngleX;
	m_fCurrentAngleZ = m_fAngleZ = fAngleZ;
}

void ZCamera::Init()
{
	m_fAngleX = CAMERA_DEFAULT_ANGLEX;
	m_fCurrentAngleX = CAMERA_DEFAULT_ANGLEX;
    m_fAngleZ = CAMERA_DEFAULT_ANGLEZ;
	m_fCurrentAngleZ = CAMERA_DEFAULT_ANGLEZ;
    m_fDist = CAMERA_DEFAULT_DISTANCE;
	m_fCurrentDist = CAMERA_DEFAULT_DISTANCE;
	m_fRecoilAngleX = 0.0f;
    m_fRecoilAngleZ = 0.0f;
    m_fMaxRecoilAngleX = 0.0f;
	m_bShocked = false;
	m_Position = rvector(0,0,0);
	m_CurrentTarget = rvector(0,0,0);
}

bool ZCamera::CheckCollisionWall(float &fRealDist, rvector& pos, rvector& dir)
{
	RBSPPICKINFO bpi;
	float fNearZ = DEFAULT_NEAR_Z;
	rvector pos2 = pos;							// camera pos
	rvector tarpos = pos2 + (dir * fNearZ);		// near pos

	rvector up2,right2;
	up2 = rvector(0.0f, 0.0f, 1.0f);
	D3DXVec3Cross(&right2,&dir,&up2);
	D3DXVec3Normalize(&right2,&right2);

	D3DXVec3Cross(&up2,&right2,&dir);
	D3DXVec3Normalize(&up2,&up2);
	D3DXVec3Cross(&right2,&dir,&up2);
	D3DXVec3Normalize(&right2,&right2);

	float fov = g_fFOV;
	float e = 1 / (tanf(fov / 2));
	float fAspect = (float)RGetScreenWidth() / (float)RGetScreenHeight();
	float fPV = (fAspect * fNearZ / e);
	float fPH = (fNearZ / e);

	bool bCollisionWall = false;
	rmatrix matView;


	// 위
	pos2 = pos;
	rvector tar = tarpos + (up2 * fPV);
	rvector dir2;
	dir2 = tar - pos2;
	D3DXVec3Normalize(&dir2,&dir2);
	D3DXMatrixLookAtLH(&matView, &pos2, &dir, &up2);

	if(ZGetGame()->GetWorld()->GetBsp()->Pick(pos2,dir2,&bpi))
	{
		if (Magnitude(tar-bpi.PickPos)<Magnitude(tar-pos2))
		{
			rvector v1, v2, v3, v4;

			v1 = bpi.PickPos;
			v3 = tar;

			if(ZGetGame()->GetWorld()->GetBsp()->Pick(tarpos,up2,&bpi))
			{
				v2 = bpi.PickPos;

				float fD = Magnitude(tarpos-v2);
				if (fD < fPH)
				{
					rvector vv1 = v1-v2, vv2 = v2-v3; 
					D3DXVECTOR4 rV4;
					D3DXVec3Transform(&rV4, &vv1, &matView);
					vv1.x = rV4.x; vv1.y=rV4.y; vv1.z = rV4.z; 
					D3DXVec3Transform(&rV4, &vv2, &matView);
					vv2.x = rV4.x; vv2.y=rV4.y; vv2.z = rV4.z; 

					float fAng = GetAngleOfVectors(vv1, vv2);
					if (fAng < 0.0f) fAng = -fAng;

					if (fAng < pi)
					{
						bCollisionWall = true;
						m_bCollision[0] = true;

						float fX = fPV - fD;
						float fY = fX * tanf(fAng);

						float fMyRealDist=fRealDist-fY;
						fRealDist = min(fMyRealDist, fRealDist);
					}
				}
			}
		}
	}

	// 왼쪽
	pos2 = pos;
	tar = tarpos + (right2 * fPH);		// 엣지 중점
	dir2 = tar - pos2;
	D3DXVec3Normalize(&dir2,&dir2);

	if(ZGetGame()->GetWorld()->GetBsp()->Pick(pos2,dir2,&bpi))
	{
		if (Magnitude(tar-bpi.PickPos)<Magnitude(tar-pos2))
		{
			rvector v1, v2, v3, v4;

			v1 = bpi.PickPos;
			v3 = tar;

			if(ZGetGame()->GetWorld()->GetBsp()->Pick(tarpos,right2,&bpi))
			{
				v2 = bpi.PickPos;

				float fD = Magnitude(tarpos-v2);
				if (fD < fPH)
				{
					rvector vv1 = v1-v2, vv2 = v2-v3; 
					float fAng = GetAngleOfVectors(vv1, vv2);
					if (fAng < 0.0f) fAng = -fAng;

					if (fAng < (pi/2))
					{
						bCollisionWall = true;
						m_bCollision[1] = true;

						float fX = fPH - fD;
						float fY = fX * tanf(fAng);

						float fMyRealDist=fRealDist-fY;
						fRealDist = min(fMyRealDist, fRealDist);
					}
				}
			}		
		}
	}

	// 아래쪽
	pos2 = pos;
	tar = tarpos - (up2 * fPV);
	dir2 = tar - pos2;
	D3DXVec3Normalize(&dir2,&dir2);
	D3DXMatrixLookAtLH(&matView, &pos2, &dir2, &up2);

	if(ZGetGame()->GetWorld()->GetBsp()->Pick(pos2,dir2,&bpi))
	{
		if (Magnitude(tar-bpi.PickPos)<Magnitude(tar-pos2))
		{
			rvector v1, v2, v3, v4;

			v1 = bpi.PickPos;
			v3 = tar;

			if(ZGetGame()->GetWorld()->GetBsp()->Pick(tarpos,-up2,&bpi))
			{
				v2 = bpi.PickPos;

				float fD = Magnitude(tarpos-v2);
				if (fD < fPH)
				{
					bCollisionWall = true;
					m_bCollision[2] = true;

					rvector vv1 = v1-v2, vv2 = v2-v3; 
					D3DXVECTOR4 rV4;
					D3DXVec3Transform(&rV4, &vv1, &matView);
					vv1.x = rV4.x; vv1.y=rV4.y; vv1.z = rV4.z; 
					D3DXVec3Transform(&rV4, &vv2, &matView);
					vv2.x = rV4.x; vv2.y=rV4.y; vv2.z = rV4.z; 


					float fAng = GetAngleOfVectors(vv1, vv2);
					if (fAng < 0.0f) fAng = -fAng;

					if (fAng < (pi/2))
					{
						float fX = fPV - fD;
						float fY = fX * tanf(fAng);

						float fMyRealDist=fRealDist-fY;
						fRealDist = min(fMyRealDist, fRealDist);
					}
				}
			}
		}
	}


	// 오른쪽
	pos2 = pos;		// 카메라 위치
	tar = tarpos - (right2 * fPH);
	dir2 = tar - pos2;
	D3DXVec3Normalize(&dir2,&dir2);

	if(ZGetGame()->GetWorld()->GetBsp()->Pick(pos2,dir2,&bpi))
	{
		if (Magnitude(tar-bpi.PickPos)<Magnitude(tar-pos2))
		{
			rvector v1, v2, v3, v4;

			v1 = bpi.PickPos;
			v3 = tar;

			if(ZGetGame()->GetWorld()->GetBsp()->Pick(tarpos,-right2,&bpi))
			{
				v2 = bpi.PickPos;

				float fD = Magnitude(tarpos-v2);
				if (fD < fPH)
				{
					bCollisionWall = true;
					m_bCollision[3] = true;

					rvector vv1 = v1-v2, vv2 = v2-v3; 
					float fAng = GetAngleOfVectors(vv1, vv2);
					if (fAng < 0.0f) fAng = -fAng;

					if (fAng < (pi/2))
					{
						float fX = fPH - fD;
						float fY = fX * tanf(fAng);

						float fMyRealDist=fRealDist-fY;
						fRealDist = min(fMyRealDist, fRealDist);
					}
				}
			}
		}
	}
	m_fPH = fPH;



	if (fRealDist < 0) fRealDist = 0.0f;

	return bCollisionWall;
}

void ZCamera::CalcMaxPayneCameraZ(float &fRealDist, float& fAddedZ, float fAngleX)
{
	ZMyCharacter* pMyCharacter = ZGetGameInterface()->GetGame()->m_pMyCharacter;

	// 맥스페인 카메라 따라하기
	float fPayneDist = fRealDist;

	if(fAngleX<pi/2.f)
	{
		float fOffset = (pi/2.f - fAngleX) / (pi/2.f);
		float fOffset2 = 1.0f - sinf(((pi / 4.0f) * fOffset));
		fPayneDist = fOffset2 * (m_fDist - 80.0f) + 80.0f;	
	}

	if(fAngleX>pi/2.f)
	{
		float fOffset = (fAngleX - pi/2.f) / (pi/2.f);
		float fOffset2 = 1.0f - (cosf(pi + ((pi / 2.0f) * fOffset)) + 1.0f);
		fPayneDist = fOffset2 * (m_fDist - 100.0f) + 100.0f;	
	}

	if (fAngleX < 1.3f)	// 위에 볼때
	{
		float fOffset = (1.3f - fAngleX) / 1.3f;
		fOffset = fOffset * (pi/2.0f);
		float fOffset2 = 1.0f + sinf(pi+(pi/2.0f) + fOffset);
		fAddedZ = fOffset * 103.0f;
	}
/*
	if (fAngleX > (pi-1.3f))	// 아래 볼때
	{
		float fOffset = (fAngleX - (pi-1.3f)) / 1.3f;
		float fOffset2 = sinf(((pi / 4.0f) * fOffset));
		fAddedZ = -(fOffset2 * 80.0f);
	}
*/
	fRealDist = min(fRealDist, fPayneDist);


}

void ZCamera::SetLookMode(ZCAMERALOOKMODE mode)
{ 
	if( mode==ZCAMERA_FREELOOK) {

		/*
		// 프리룩인데 카메라가 solid(충돌영역) 안에 있으면 적절 위치로 조절해준다
		bool bCameraInSolid = ZGetGame()->GetWorld()->GetBsp()->CheckSolid(GetPosition(),ZFREEOBSERVER_RADIUS,0.f,RCW_SPHERE);

		if(bCameraInSolid)
		*/
		{
			ZObserver* pObserver = ZGetCombatInterface()->GetObserver();
			// 당연히 observer 모드여야 한다
			//_ASSERT( pObserver->IsVisible() && pObserver->GetTargetCharacter() != NULL);

			ZCharacter *pTargetCharacter=pObserver->GetTargetCharacter();
			rvector a_pos;
			rvector a_dir;
			bool bHistory = pTargetCharacter->GetHistory(&a_pos,&a_dir,ZGetGame()->GetTime()-pObserver->GetDelay());
			//_ASSERT(bHistory);

			// 이정도면 캐릭터의 몸속이므로 항상 solid에 있지 않다.
			rvector newPos = a_pos+ rvector(0,0,140);	

			rvector targetPosition;
			if(m_nLookMode==ZCAMERA_MINIMAP)
				targetPosition = m_Target - m_fCurrentDist * a_dir;
			else
				targetPosition = m_Position;

			// 현재 카메라의 위치까지 이어서 가까이 갈수있는곳까지 간다
			ZGetGame()->GetWorld()->GetBsp()->CheckWall(newPos,targetPosition,ZFREEOBSERVER_RADIUS+1,0.f,RCW_SPHERE);

			SetPosition(targetPosition);

			bool bCameraInSolid2 = ZGetGame()->GetWorld()->GetBsp()->CheckSolid(GetPosition(),ZFREEOBSERVER_RADIUS,0.f,RCW_SPHERE);
			//_ASSERT(!bCameraInSolid2);
		}
	}

	m_nLookMode = mode; 

	if(mode==ZCAMERA_MINIMAP) {
		if(!ZGetGameInterface()->OpenMiniMap())
			SetNextLookMode();
	}
}

void ZCamera::SetNextLookMode()
{
	ZCAMERALOOKMODE mode = ZCAMERALOOKMODE(m_nLookMode+1);
	if(mode>=ZCAMERA_FREELOOK)
		mode = ZCAMERA_DEFAULT;
	SetLookMode(mode);
}

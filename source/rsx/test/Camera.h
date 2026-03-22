#ifndef CAM_H
#define CAM_H

#include "../Interface.h"
namespace rsx {
namespace test {

class Camera
{
	Vector3f CamPos;
	Vector3f CamDir;
public:

	D3DXMATRIX RotX, RotY, RotZ, Trans, View, Proj;

	Camera(): CamPos(0, 0, 0), CamDir(0, 0, 0){}

	void SetPerspective(LPDIRECT3DDEVICE9 g_Dev, float FOV, float Aspect, float Near, float Far );

	void Move( float x, float y, float z );
	void MoveFront( float f );
	void MoveSide( float f );

	void Rotate( float x, float y );

	void SetPos( float x, float y, float z );
	void SetDir( float x, float y, float z );
	void Apply(LPDIRECT3DDEVICE9 g_Dev);

	Vector3f * GetPos(){ return &CamPos;}
	Vector3f * GetDir(){ return &CamDir;}
};

}
}
#endif
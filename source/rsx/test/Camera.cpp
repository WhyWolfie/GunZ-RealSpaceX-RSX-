#include "Camera.h"

namespace rsx {
namespace test {

#define TORAD(x) ((x) * 3.14159 / 180)

void Camera::Move(float x, float y, float z)
{
	CamPos.x += x;
	CamPos.y += y;
	CamPos.z += z;
}

void Camera::MoveFront(float f)
{
	CamPos.z += f * cosf( TORAD(CamDir.y) );
	CamPos.x += f * sinf( TORAD(CamDir.y) );
	printf("Pos: %f, %f\n", CamPos.x, CamPos.z);
}

void Camera::MoveSide(float f)
{
	CamPos.x += f * cosf( TORAD(CamDir.y) );
	CamPos.z -= f * sinf( TORAD(CamDir.y) );
	printf("Pos: %f, %f\n", CamPos.x, CamPos.z);
}

void Camera::Rotate(float x, float y)
{
	CamDir.x += x;
	CamDir.y += y;

	if( CamDir.x > 360.0f )
		CamDir.x -= 360.0f;
	if( CamDir.x < 0.0f )
		CamDir.x += 360.0f;
	if( CamDir.y > 360.0f )
		CamDir.y -= 360.0f;
	if( CamDir.y < 0.0f )
		CamDir.y += 360.0f;
}

void Camera::SetDir(float x, float y, float z)
{
	CamDir = Vector3f( x, y, z );
}

void Camera::SetPos(float x, float y, float z)
{
	CamPos = Vector3f( x, y, z );
}

void Camera::Apply(LPDIRECT3DDEVICE9 g_Dev)
{
	D3DXMatrixRotationX( &RotX, TORAD(-CamDir.x) );
	D3DXMatrixRotationY( &RotY, TORAD(-CamDir.y) );
	D3DXMatrixRotationZ( &RotZ, TORAD(-CamDir.z) );
	D3DXMatrixTranslation( &Trans, - CamPos.x , - CamPos.y,  -CamPos.z );

	D3DXMATRIX ROT90X, ROT180Y, TRANS;
	D3DXMatrixRotationX( &ROT90X, TORAD(-90.0f) );
	D3DXMatrixRotationY( &ROT180Y, TORAD(180.0f) );
	D3DXMatrixTranslation( &TRANS, 0 , 100,  -140.0f );
	/*D3DXMatrixRotationX( &ROT90X, TORAD(0) );
	D3DXMatrixRotationY( &ROT180Y, TORAD(0) );
	D3DXMatrixTranslation( &TRANS, 0 , 0,  0 );*/

	View = Trans * (RotZ * RotY * RotX);

	//g_Dev->SetTransform( D3DTS_VIEW , &(((TRANS*ROT90X)*ROT180Y)*View));

	g_Dev->SetTransform( D3DTS_VIEW , &(ROT90X*View));
	//g_Dev->SetTransform( D3DTS_VIEW , &View);
}

void Camera::SetPerspective(LPDIRECT3DDEVICE9 g_Dev, float FOV, float Aspect, float Near, float Far)
{
	D3DXMatrixPerspectiveFovLH( &Proj, FOV, Aspect, Near, Far );
	g_Dev->SetTransform(D3DTS_PROJECTION , &Proj);


	float fnear = Proj._43 * (-1/(Proj._33));
	float ffar = Proj._33*fnear / (Proj._33 - 1);
}

}
}
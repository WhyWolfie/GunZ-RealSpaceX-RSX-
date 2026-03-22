#pragma once

#pragma comment(lib, "D3dx9.lib")
#pragma comment(lib, "D3d9.lib")

#include "../Interface.h"
#include "Camera.h"

namespace rsx {
namespace test {

typedef void (*ONRENDERFUNC) ( void );
LRESULT CALLBACK MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

class DXWindow
{
	int Width, Height;
	bool bFullScreen;
	bool bQuit;
	std::string Title;
	std::string class_name;
	ONRENDERFUNC OnRender;
	HINSTANCE hInst;
	
	HWND hWnd;
	HDC hDC;
	HGLRC hRC;
	WNDCLASSA wc;

	LPDIRECT3D9 m_pD3D;
	LPDIRECT3DDEVICE9 m_pDevice;

public:
	DXWindow(int w, int h, char * Name, bool fs = false, ONRENDERFUNC f = NULL);
	~DXWindow() {Release();}
	bool Create();
	bool InitDX();
	int MainLoop();
	void SetRenderFunc( ONRENDERFUNC Func ) { OnRender = Func; }
	LPDIRECT3DDEVICE9 GetDevice();
	void Release();
	HRESULT Reset();
};

}
}


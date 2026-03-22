#include "DirectX.h"

namespace rsx {
namespace test {

Camera Cam;
bool Fixed = false;
bool useNM = false;

#define TORAD(x) ((x) * 3.14159 / 180)

DXWindow::DXWindow(int w, int h, char * Name, bool fs, ONRENDERFUNC f)
{
	Width = w;
	Height = h;
	Title = Name;
	bFullScreen = fs;
	OnRender = f;
	m_pD3D = NULL;
	m_pDevice = NULL;
}

void DXWindow::Release()
{
	if (m_pDevice)
		m_pDevice->Release();
	if (m_pD3D)
		m_pD3D->Release();
}

LPDIRECT3DDEVICE9 DXWindow::GetDevice()
{
	return m_pDevice;
}

bool DXWindow::InitDX()
{
	m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS d3dpp;
	D3DDISPLAYMODE d3ddm;

	m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&d3ddm);

	memset( &d3dpp, 0, sizeof(d3dpp) );
	d3dpp.Windowed = !bFullScreen;
	d3dpp.BackBufferFormat = d3ddm.Format;// D3DFMT_A8R8G8B8;
	d3dpp.BackBufferHeight =  bFullScreen ? Height = d3ddm.Height : Height;
	d3dpp.BackBufferWidth = bFullScreen ? Width = d3ddm.Width : Width;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.BackBufferCount = 2;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.Flags = NULL;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    HRESULT h = m_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &d3dpp, &m_pDevice);

	if (FAILED(h))
	{
		//printf(DXGetErrorString9A(h));
		return false;
	}

	D3DVIEWPORT9 vp;
	vp.Height = Height;
	vp.Width = Width;
	vp.MaxZ = 1.0f;
	vp.MinZ = 0.0f;
	vp.X = 0;
	vp.Y = 0;
	//m_pDevice->SetViewport(&vp);
	Cam.SetPerspective(m_pDevice, TORAD(45), (float)Width/(float)Height, 50, 20000);
	return true;
}

bool DXWindow::Create()
{
	bQuit = false;
	class_name = std::string(Title).append("_class");
	memset( &wc, 0, sizeof(wc) );

	hInst = GetModuleHandleA(NULL);

	wc.hInstance = hInst;
	wc.lpfnWndProc = MyWndProc;
	wc.lpszClassName = class_name.c_str();
	wc.hbrBackground	= (HBRUSH)(COLOR_WINDOW-1);
	wc.style = CS_HREDRAW | CS_VREDRAW;

	ATOM a = RegisterClassA(&wc);
	hWnd = CreateWindowA(class_name.c_str(),
		                 Title.c_str(),
						 WS_POPUP|WS_VISIBLE|WS_SYSMENU , /* WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX,*/
						 CW_USEDEFAULT,
						 CW_USEDEFAULT,
						 Width,
						 Height,
						 NULL,
						 NULL,
						 hInst,
						 NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, 1);
   UpdateWindow(hWnd);

   return TRUE;
}



int DXWindow::MainLoop()
{
	MSG msg;

    GetDevice()->SetRenderState(D3DRS_AMBIENT, 0xFF5F5F5F);
	GetDevice()->SetRenderState(D3DRS_LIGHTING, FALSE);
	GetDevice()->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);

    HRESULT H;
	D3DMATERIAL9 m;
	memset(&m,0,sizeof(m));
	D3DCOLORVALUE c; 

	c.r = c.b = c.g = c.a = 1.0f;
	m.Ambient = c;
	m.Diffuse = c;
	m.Specular = c;
	m.Power = 1;
	H = GetDevice()->SetMaterial( &m );

	while (!bQuit)
	{
		if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            else
            {
                TranslateMessage (&msg);
                DispatchMessage (&msg);
            }
        }
		else
		{//Draw
            m_pDevice->BeginScene();
			if (OnRender)
			{
				Cam.Apply(m_pDevice);
				OnRender();
			}
            m_pDevice->EndScene();
            m_pDevice->Present(NULL, NULL, NULL, NULL);
			Sleep( 10 );
		}
	}
	UnregisterClassA( class_name.c_str(), hInst );
	return msg.wParam;
}
HRESULT DXWindow::Reset()
{
D3DPRESENT_PARAMETERS d3dpp;
	D3DDISPLAYMODE d3ddm;

	m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&d3ddm);

	memset( &d3dpp, 0, sizeof(d3dpp) );
	d3dpp.Windowed = !bFullScreen;
	d3dpp.BackBufferFormat = d3ddm.Format;// D3DFMT_A8R8G8B8;
	d3dpp.BackBufferHeight =  bFullScreen ? Height = d3ddm.Height : Height;
	d3dpp.BackBufferWidth = bFullScreen ? Width = d3ddm.Width : Width;
	d3dpp.EnableAutoDepthStencil = TRUE;
	d3dpp.BackBufferCount = 2;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
	d3dpp.Flags = NULL;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

	HRESULT h = m_pDevice->Reset(&d3dpp);
	Cam.SetPerspective(m_pDevice, TORAD(45), (float)Width/(float)Height, 50, 10000);
	return h;
}

#define CM (100.0f)
#define ROT(x) ((x)*0.5f)
bool leftDown = false;
int lastX, lastY;
int dx; int dy;

extern void ChangeFocus(float f);
extern void Reload();

LRESULT CALLBACK MyWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_MOUSEMOVE:
		{

			if (leftDown)
			{
				dx = (lParam & 0xFFFF) - lastX;
				dy = (lParam >> 16) - lastY;
				Cam.Rotate(ROT(dy), ROT(dx));
				lastX = lParam & 0xFFFF;
				lastY = lParam >> 16;
			}
		break;
		}
	case WM_LBUTTONDOWN:
		leftDown = true;
		lastX = lParam & 0xFFFF;
		lastY = lParam >> 16;
		break;
	case WM_LBUTTONUP:
		leftDown = false;
		break;
	case WM_MOUSEWHEEL:
		{
			short amount = HIWORD(wParam);
			//ChangeFocus(amount*0.0001);
			//Cam.MoveFront( amount*0.1f );
		break;
		}
	case WM_KEYDOWN:
		switch (wParam)
        {
		case VK_LEFT:
			Cam.Rotate( 0, -1);
			break;
		case VK_RIGHT:
			Cam.Rotate( 0, 1);
			break;
		case VK_UP:
			Cam.Rotate( -1, 0);
			break;
		case VK_DOWN:
			Cam.Rotate( 1, 0);
			break;
		case 'w': case 'W':
			Cam.MoveFront( CM );
			break;
		case 's': case 'S':
			Cam.MoveFront( -CM );
			break;
		case 'a': case 'A':
			Cam.MoveSide( -CM );
			break;
		case 'd': case 'D':
			Cam.MoveSide( CM );
			break;
		case VK_HOME:
			Cam.Move( 0, CM, 0 );
			break;
		case VK_END:
			Cam.Move( 0, -CM, 0 );
			break;
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;
		case VK_SPACE :
            Cam.Move(0, CM, 0);
			break;
		}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

}
}
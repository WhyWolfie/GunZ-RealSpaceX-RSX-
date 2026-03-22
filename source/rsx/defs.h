#pragma once

#include <stdio.h>
#include <math.h>
#include <vector>
#include <list>
#include <string>
#include <map>
#include <memory>
#include <windows.h>
#include <windowsx.h>

#include "d3d9.h"
#include "D3dx9tex.h"



typedef unsigned char ubyte;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned __int64 uint64;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if ( (p) ) { (p)->Release(); (p) = 0; } }
#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(a) if( (a) != NULL ) delete (a); (a) = NULL;
#endif


#ifndef M_PI
    #define M_PI 3.141592654
#endif
#define TO_RAD(x) (x*M_PI/180)
#define TO_DEG(x) (x*180/M_PI)


#pragma warning( disable : 4305 4996 4018 )

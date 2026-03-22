/*
#include "DxRenderer.h"
#include "FileSystem.h"
#include "DxBufferManager.h"
#include "DxTextureManager.h"
*/
#include "rsx.h"
#include "test/DirectX.h"


rsx::test::DXWindow * pWin;
IEngine *engine;

void onDraw()
{
    pWin->GetDevice()->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,  D3DCOLOR_XRGB(50, 50, 50), 1.0f, 0);
    engine->getRenderer()->draw();
}

int main()
{
    rsx::test::DXWindow window(700, 700, "RSX");
    pWin = &window;
    window.Create();
    window.InitDX();
    window.SetRenderFunc(onDraw);

    engine = rsx::getEngine();
    engine->initialize(window.GetDevice());

    std::vector<IObject*> vobjects;
    std::vector<ILight*> vlights;

    std::vector<std::string> args;
    /*
    args.push_back("F:/Gunz2/gunz2_20130225_225959/datadump/Data/Maps/PvP_maps/pvp_mansion2/pvp_mansion2");
    args.push_back("F:/Gunz2/gunz2_20130225_225959/datadump/Data/Texture/Map/S_01/");
    args.push_back("F:/Gunz2/gunz2_20130225_225959/datadump/Data/Model/MapObject/S_01/pvp_mansion/");
    args.push_back("F:/Gunz2/gunz2_20130225_225959/datadump/Data/Model/MapObject/Props/");
    */

    args.push_back("maps/mansion/pvp_mansion2");
    args.push_back("maps/mansion/");
    args.push_back("texture/");
    args.push_back("prop/");

    engine->getObjectLoader()->load(args, vobjects, vlights);

    for (int i = 0; i < vobjects.size(); ++i)
    {
        engine->getRenderer()->addObject(vobjects[i]);
        engine->getCollisionSystem()->addObject(vobjects[i]);
    }

    for (int i = 0; i < vlights.size(); ++i)
        engine->getRenderer()->addLight(vlights[i]);

    window.MainLoop();
	return 0;
}



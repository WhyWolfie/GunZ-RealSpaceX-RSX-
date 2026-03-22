#include "stdafx.h"
#include "rsxmap.h"
#include "../../rsx/rsx.h"
#ifdef pi
#undef pi
#endif
#include "rapidxml\rapidxml.hpp"

#include "../../rsx/utils.h"

extern void drawDebugText(int x, int y, const char * str);

_NAMESPACE_REALSPACE2_BEGIN








void drawFormatted(int x, int y, const char * name, double avg, double _min, double _max)
{
    static char tmp[300];
    sprintf(tmp, "%s : %4.4f (avg) %4.4f (min) %4.4f (max)\n", name, avg, _min, _max);
    drawDebugText(x, y, tmp);
}

#if 0

rsx::utils::TimerCounter drawCounter("draw"),
                         colisionRayCounter("collision ray"),
                         colisionCylinderCounter("collision Cylinder"),
                         colisionSphereCounter("collision Sphere");

#define TIMER_END_D(x) do {                                                     \
                            double t = (x).end();                               \
                            drawFormatted(30, 100, (x).getName(), t, (x).getMin(), (x).getMax());\
                            } while (0);

#define TIMER_END_CYL(x) do {                                                     \
                            double t = (x).end();                               \
                            drawFormatted(30, 125, (x).getName(), t, (x).getMin(), (x).getMax());\
                            } while (0)       

#define TIMER_END_RAY(x) do {                                                     \
                            double t = (x).end();                               \
                            drawFormatted(30, 150, (x).getName(), t, (x).getMin(), (x).getMax());\
                            } while (0)     

#define TIMER_END_SPHERE(x) do {                                                     \
                            double t = (x).end();                               \
                            drawFormatted(30, 175, (x).getName(), t, (x).getMin(), (x).getMax());\
                            } while (0)     
#else
#define TIMER_END_D(x) 
#define TIMER_END_CYL(x) 
#define TIMER_END_RAY(x) 
#define TIMER_END_SPHERE(x)
#endif

/*
DumpOnDestruct::~DumpOnDestruct()
{
    rsx::utils::Logger::print("DRAW\n");
    rsx::utils::Logger::print("\tDraw calls: %d\n", drawCounter.getCount());
    rsx::utils::Logger::print("\tDraw min time: %f\n", drawCounter.getMin());
    rsx::utils::Logger::print("\tDraw max time: %f\n", drawCounter.getMax());
    rsx::utils::Logger::print("\tDraw mean time: %f\n", drawCounter.getMean());

    rsx::utils::Logger::print("\nCOLLISION\n");
    rsx::utils::Logger::print("::ray\n");
    rsx::utils::Logger::print("\tRay tests: %d\n", colisionRayCounter.getCount());
    rsx::utils::Logger::print("\tRay test min time: %f\n", colisionRayCounter.getMin());
    rsx::utils::Logger::print("\tRay test max time: %f\n", colisionRayCounter.getMax());
    rsx::utils::Logger::print("\tRay test mean time: %f\n", colisionRayCounter.getMean());

    rsx::utils::Logger::print("::cylinder\n");
    rsx::utils::Logger::print("\tcylinder tests: %d\n", colisionCylinderCounter.getCount());
    rsx::utils::Logger::print("\tcylinder test min time: %f\n", colisionCylinderCounter.getMin());
    rsx::utils::Logger::print("\tcylinder test maxtime: %f\n", colisionCylinderCounter.getMax());
    rsx::utils::Logger::print("\tcylinder test mean time: %f\n", colisionCylinderCounter.getMean());

    rsx::utils::Logger::print("::sphere\n");
    rsx::utils::Logger::print("\tsphere tests: %d\n", colisionSphereCounter.getCount());
    rsx::utils::Logger::print("\tsphere test min time: %f\n", colisionSphereCounter.getMin());
    rsx::utils::Logger::print("\tsphere test max time: %f\n", colisionSphereCounter.getMax());
    rsx::utils::Logger::print("\tsphere test  mean time: %f\n", colisionSphereCounter.getMean());
}
*/



bool RSXMap::Open(const char * name, const char * ext)
{
    std::string path = name;
    path.append(".").append(ext);
    std::unique_ptr<IFile> file = std::unique_ptr<IFile>(rsx::getEngine()->geFileManager()->open(path.c_str()));

    if (file.get() == nullptr || !file->isOpen())
        return nullptr;

    uint size = file->getSize();
    std::unique_ptr<char> data = std::unique_ptr<char>(new char[size + 1]);
    if (!file->read(size, data.get()) == size)
        return nullptr;

    data.get()[size] = 0;

    rapidxml::xml_document<> doc;
	doc.parse<0>(data.get());

    rapidxml::xml_node<> * node = doc.first_node("XML");
    if (node == nullptr)
        return nullptr;
    node = node->first_node("RSX");

    if (node == nullptr)
        return false;

    std::vector<std::string> args;
    rapidxml::xml_node<> * map_name = node->first_node("NAME");
    if (map_name != nullptr)
    {
        args.push_back(map_name->value());
        rapidxml::xml_node<> * node_path = map_name->next_sibling("PATH");
        while (node_path != nullptr)
        {
            args.push_back(node_path->value());
            node_path = node_path->next_sibling("PATH");
        }
        
    }
   
    rsx::getEngine()->getObjectLoader()->load(args, vobjects, vlights);
    rsx::getEngine()->getCollisionSystem()->clear();

    for (int i = 0; i < vobjects.size(); ++i)
    {
        rsx::getEngine()->getRenderer()->addObject(vobjects[i]);
        rsx::getEngine()->getCollisionSystem()->addObject(vobjects[i]);
    }

    for (int i = 0; i < vlights.size(); ++i)
        rsx::getEngine()->getRenderer()->addLight(vlights[i]);

    rsx::getEngine()->getCollisionSystem()->build();
    LoadDummy(path.c_str());

    return true;
}

void RSXMap::Release()
{
    rsx::getEngine()->getCollisionSystem()->clear();
    rsx::getEngine()->getBufferManager()->clear();
    for (int i = 0; i < vobjects.size(); ++i)
        rsx::getEngine()->getRenderer()->removeObject(vobjects[i]);

    for (int i = 0; i < vlights.size(); ++i)
        rsx::getEngine()->getRenderer()->removeLight(vlights[i]);
}

bool RSXMap::LoadDummy(const char * name)
{
	std::unique_ptr<IFile> pFile = std::unique_ptr<IFile>(getFileManager()->open(name));
    if (!pFile.get())
    {
        return false;
    }

    std::unique_ptr<char> buffer = std::unique_ptr<char>(new char[pFile->getSize()+1]);
    pFile->read(pFile->getSize(), buffer.get());
	buffer.get()[pFile->getSize()] = 0;

	
	MXmlDocument aXml;
	aXml.Create();
	if(!aXml.LoadFromMemory(buffer.get()))
	{
		return false;
	}

	int iCount, i;
	MXmlElement		aParent, aChild;
	aParent = aXml.GetDocumentElement();
	iCount = aParent.GetChildNodeCount();


	char szTagName[256];
	for (i = 0; i < iCount; i++)
	{
		aChild = aParent.GetChildNode(i);
		aChild.GetTagName(szTagName);
		if(stricmp(szTagName,RTOK_DUMMYLIST)==0)
		{
			m_DummyList.Open(&aChild);
		}
	}
	return true;
}

void RSXMap::updateMatrices()
{
    Matrix4f m;
    ((LPDIRECT3DDEVICE9)rsx::getEngine()->getContext())->GetTransform(D3DTS_PROJECTION, (D3DMATRIX*)&m);
    rsx::getEngine()->getRenderer()->setProjectionMatrix(m);

    ((LPDIRECT3DDEVICE9)rsx::getEngine()->getContext())->GetTransform(D3DTS_VIEW, (D3DMATRIX*)&m);
    rsx::getEngine()->getRenderer()->setViewMatrix(m);
}


bool RSXMap::Draw()
{
    TIMER_BEGIN(drawCounter);
    this->updateMatrices();
    IRenderer * pRenderer = rsx::getEngine()->getRenderer();
    pRenderer->setCullMode(RENDERER_CULL_NONE);
    pRenderer->setDepthFunc(RENDERER_CMP_LESS);
    pRenderer->setAlphaTest(false);
    pRenderer->setDepthTest(true);
    pRenderer->setDepthWrite(true);
    pRenderer->draw();
    TIMER_END_D(drawCounter);
    return true;
}
void RSXMap::DrawObjects() {}

void RSXMap::OnInvalidate() {}
void RSXMap::OnRestore() {}


rvector RSXMap::GetFloor(rvector &origin,float fRadius,float fHeight,rplane *pimpactplane)
{
	rvector targetpos = origin + rvector(0,0,-10000);
	//return rvector(origin.x, origin.y, 120);
	Vector3f to((float*)&targetpos), hit, normal;
    TIMER_BEGIN(colisionCylinderCounter);
    if (rsx::getEngine()->getCollisionSystem()->checkCylinder((Vector3f)origin, to, hit, normal, fRadius, fHeight))
	{
        TIMER_END_CYL(colisionCylinderCounter);
		targetpos = *(rvector*)&to;
		if (pimpactplane)
			D3DXPlaneFromPointNormal(pimpactplane, (rvector*)&hit, (rvector*)&normal);
		return targetpos;
	}
    TIMER_END_CYL(colisionCylinderCounter);
	return targetpos;
}

RPOLYGONINFO info__;
bool RSXMap::Pick(const rvector &pos, const rvector &dir, RBSPPICKINFO *pOut, DWORD dwPassFlag)
{
	rvector to, hit, normal;
	to = pos + 10000*dir;
    TIMER_BEGIN(colisionRayCounter);
	if (rsx::getEngine()->getCollisionSystem()->pick(*(Vector3f*)&pos, *(Vector3f*)&to, *(Vector3f*)&hit, *(Vector3f*)&normal))
	{
        TIMER_END_RAY(colisionRayCounter);
		pOut->PickPos = hit;
		pOut->pInfo = &info__;
		pOut->pNode = NULL;
		D3DXPlaneFromPointNormal(&info__.plane, &hit, &normal);
		return true;
	}
    TIMER_END_RAY(colisionRayCounter);
	return false;

}
bool RSXMap::PickTo(const rvector &pos, const rvector &to, RBSPPICKINFO *pOut, DWORD dwPassFlag)
{
	rvector hit, normal;
    TIMER_BEGIN(colisionRayCounter);
	if (rsx::getEngine()->getCollisionSystem()->pick(*(Vector3f*)&pos, *(Vector3f*)&to, *(Vector3f*)&hit, *(Vector3f*)&normal))
	{
        TIMER_END_RAY(colisionRayCounter);
		pOut->PickPos = hit;
		pOut->pInfo = &info__;
		pOut->pNode = NULL;
		D3DXPlaneFromPointNormal(&info__.plane, &hit, &normal);
		return true;
	}
    TIMER_END_RAY(colisionRayCounter);
	return false;
}
bool RSXMap::CheckSolid(rvector &pos, float fRadius, float fHeight, RCOLLISIONMETHOD method)
{
	Vector3f position((float*)&pos), hit, normal;
    TIMER_BEGIN(colisionSphereCounter);
	if (rsx::getEngine()->getCollisionSystem()->checkSphere(position, position, hit, normal, fRadius))
	{
        TIMER_END_SPHERE(colisionSphereCounter);
		return true;
	}
    TIMER_END_SPHERE(colisionSphereCounter);
	return false;
}
bool RSXMap::CheckWall(rvector &origin, rvector &targetpos, float fRadius, float fHeight, RCOLLISIONMETHOD method, int nDepth, rplane *pimpactplane)
{
	rvector to((float*)&targetpos), hit, normal;
	bool b;
	//targetpos is update inside CheckCylinder/Sphere if there is any collision
	if (method == RCW_SPHERE)
    {
        TIMER_BEGIN(colisionSphereCounter);
		b = rsx::getEngine()->getCollisionSystem()->checkSphere((Vector3f)origin, *(Vector3f*)&to, *(Vector3f*)&hit, *(Vector3f*)&normal, fRadius);
        TIMER_END_SPHERE(colisionSphereCounter);
    }
	else
    {
        TIMER_BEGIN(colisionCylinderCounter);
		b = rsx::getEngine()->getCollisionSystem()->checkCylinder((Vector3f)origin, *(Vector3f*)&to, *(Vector3f*)&hit, *(Vector3f*)&normal, fRadius, fHeight);
        TIMER_END_CYL(colisionCylinderCounter);
    }

	if (b)
	{
        ((Vector3f*)&normal)->normalize();
        /*Vector3f dir = (*(Vector3f*)&to) - ((Vector3f)origin);
        dir.normalize();
        Vector3f delta = dir * (-10.f);

        to += *(rvector*)&delta;

        Vector3f dir2 = (*(Vector3f*)&to) - ((Vector3f)origin);
        dir2.normalize();

        dir2 = dir - dir2;
        if (dir2.x <= 0 || dir2.y <= 0 ||dir2.z <= 0)
            targetpos = origin;
        else
            targetpos = (rvector)to;*/

		to += normal*4;
		targetpos = (rvector)to;
		

		if (pimpactplane)
			D3DXPlaneFromPointNormal(pimpactplane, &hit, &normal);
		return true;
	}
	return false;
}

RBSPMATERIAL __mat;

/*
 * Gunz uses this function to retrieve the sound name
 */
RBSPMATERIAL * RSXMap::GetMaterial(int nIndex)
{
	__mat.Name = "snd_name";
	return NULL;
}
RBSPMATERIAL * RSXMap::GetMaterial(RSBspNode *pNode, int nIndex)
{
	return NULL;
}

/*
 * Contains the spawns, cam_pos, etc.
 * 
 */

RDummyList * RSXMap::GetDummyList(){return &m_DummyList;}

//Implemented using pick
bool RSXMap::GetShadowPosition( rvector& pos_, rvector& dir_, rvector* outNormal_, rvector* outPos_ )
{
	return NULL;
}


bool RSXMap::Open(const char *fileName, const char* ext, ROpenFlag nOpenFlag, RFPROGRESSCALLBACK pfnProgressCallback, void *CallbackParam)
{
    return Open(fileName, ext);
}

void RSXMap::OptimizeBoundingBox() {}
bool RSXMap::IsVisible(rboundingbox &bb) {return true;}


void RSXMap::DrawLight(D3DLIGHT9 *pLight) {}

/*
 * Must return a list of ligths used to lit the characters
 */
RLightList zeroLigths;
RLightList * RSXMap::GetObjectLightList() {return &zeroLigths;}

/*
 * Must return a list of objects to be loaded
 */
RMapObjectList zeroObjects;
RMapObjectList * RSXMap::GetMapObjectList() {return &zeroObjects;}

/*
 * Used to store the loaded Objects
 * there is no objects so we can return null
 */
RMeshMgr * RSXMap::GetMeshManager() {return NULL;}

/*
 * No fog for now.
 */
FogInfo __fog;
FogInfo RSXMap::GetFogInfo() {return __fog;}

/*
 * Ambient sound none for now
 */
list<AmbSndInfo *> __ambSnd;
list<AmbSndInfo *> RSXMap::GetAmbSndList() {return __ambSnd;}

//do nothing
void RSXMap::SetWireframeMode(bool bWireframe) {}

//We can return null here, gunz have checks for null ptr.
RNavigationMesh * RSXMap::GetNavigationMesh() {return NULL;}

RLightList zeroSunLights;
RLightList * RSXMap::GetSunLightList() {return &zeroSunLights;}

//do nothing
void RSXMap::LightMapOnOff(bool b) {}
void RSXMap::SetMapObjectOcclusion(bool b) {}

RSBspNode aNode;

RSBspNode * RSXMap::GetRootNode()
{
	return &aNode;
}

_NAMESPACE_REALSPACE2_END
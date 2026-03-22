#include "elu.h"
#include "XMLParser.h"

extern void dumpFile(const char * name, IFile * file);

namespace rsx {

uint READ_UINT(IFile *file) {uint r; file->read(sizeof(uint), (char*)&r); return r;}
ushort READ_USHORT(IFile *file) {ushort r; file->read(sizeof(ushort), (char*)&r); return r;}

#define TRY_FOR_EACH_PATH(exp, name) do {                                                                    \
                                            std::vector<std::string>& paths =  PathManager::getPathList();   \
                                            std::string path = name;                                         \
                                            if (exp) break;                                                  \
                                            for (int i = 0; i < paths.size(); ++i)                           \
                                            {                                                                \
                                                path = paths[i];                                             \
                                                path.append(name);                                           \
                                                if (exp) break;                                              \
                                            }                                                                \
                                        } while (0);                                                         \


class PathManager
{
	static std::vector<std::string> m_vPath;
public:
	static std::vector<std::string>& getPathList() {return m_vPath;} 
	static void clear() {m_vPath.clear();}
	static void add(std::string& path) {m_vPath.push_back(path);}
	static void add(const char * path) {m_vPath.push_back(path);}
};

std::vector<std::string> PathManager::m_vPath;

EluLoader::EluLoader(IEngine * pEngine) : m_pTexMgr(pEngine->getTextureManager()), m_pFileMgr(pEngine->geFileManager()) 
{
}

IObject * EluLoader::load(const char * name)
{
    XMLParser::setFileManager(m_pFileMgr);
    std::map<std::string, simple_object_ref<IObject>>::iterator itor;
    itor = m_vObject.find(name);
    if (itor != m_vObject.end())
    {
        return ((EluObject*)itor->second.object)->clone();
    }

    IFile * fptr;

    TRY_FOR_EACH_PATH((fptr = m_pFileMgr->open(path.c_str())) != nullptr, name);
    std::unique_ptr<IFile> file(fptr);

    if (file.get() == nullptr || !file->isOpen())
    {
        printf("Falha ao abrir o arquio: %s", name);
        return nullptr;
    }

    EluHeader hdr;
    file->read(sizeof(hdr), (char*)&hdr);

    EluObject * pObject = new EluObject();

    pObject->m_pMeshes.reserve(hdr.meshCount);
    for (int i = 0; i < hdr.meshCount; ++i)
    {
        pObject->m_pMeshes.push_back(std::shared_ptr<EluMesh>(new EluMesh()));

        switch (hdr.Version)
        {
        case 0x5012:
            LoadMesh5012(file.get(), *pObject->m_pMeshes[i]);
            break;
        case 0x5013:
            LoadMesh5013(file.get(), *pObject->m_pMeshes[i]);
            break;
        }
    }

    std::string n = name;
    n.append(".xml");
    loadMaterial(n.c_str(), pObject->m_vMaterial);
    pObject->m_name = name;

    m_vObject[name] = pObject;
    return pObject;
}




void EluLoader::LoadMesh5012(IFile *file, EluMesh &mesh)
{
    char * str = (char*)file->read(READ_UINT(file));
    mesh.m_name = str;

    file->skip(READ_UINT(file));
    file->skip(12);

    file->read(sizeof(Matrix4f), &mesh.m_World);
    file->skip(8);

    uint npos = READ_UINT(file);
    Vector3f * position = (Vector3f*)file->read(sizeof(Vector3f) * npos);

    uint nnor = READ_UINT(file);
    Vector3f * normal = (Vector3f*)file->read(sizeof(Vector3f) * nnor);

    uint ntan = READ_UINT(file);
    Vector4f * tangent = (Vector4f*)file->read(sizeof(Vector4f) * ntan);

    file->skip(4);

    uint ntex = READ_UINT(file);
    Vector3f * texcoord = (Vector3f*)file->read(sizeof(Vector3f) * ntex);

    file->skip(sizeof(Vector3f) * READ_UINT(file));

    //Read indices
    uint ntris = READ_UINT(file);
    if (ntris > 0)
    {
        file->skip(4);
        ntris = READ_UINT(file);

        for (int i = 0; i < ntris; ++i)
        {
            uint nverts = READ_UINT(file);
            file->skip(12 * nverts);
            file->skip(2);
        }
    }

    file->skip(sizeof(Vector3f)*READ_UINT(file));
    file->skip(4);

    uint bcount = READ_UINT(file);
    for (int i = 0; i < bcount; ++i)
    {
        uint bi = READ_UINT(file);
        file->skip(8 * bi);
    }

    file->skip(4);

    uint nsubindex = READ_UINT(file);
    //pos, nor, tex, ??, tan, ??
    ushort * subindices = (ushort*) file->read(6 * sizeof(ushort) * nsubindex);

    file->skip((64 + 2) * READ_UINT(file));

    mesh.m_uiVertexCount = nsubindex;
    mesh.m_uiIndexCount = READ_UINT(file);
    mesh.m_vIndex = new ushort[mesh.m_uiIndexCount];
    file->read(mesh.m_uiIndexCount * sizeof(ushort), mesh.m_vIndex);

    mesh.m_vPosition = new Vector3f[nsubindex];
    mesh.m_vNormal =   new Vector3f[nsubindex];
    mesh.m_vTexCoord = new Vector2f[nsubindex];
    mesh.m_vTangent =  new Vector4f[nsubindex];

    for (int i = 0; i < nsubindex; ++i)
    {
        mesh.m_vPosition[i] = position[subindices[i * 6 + 0]];
        mesh.m_vNormal[i]   =   normal[subindices[i * 6 + 1]];

        mesh.m_vTexCoord[i].x = texcoord[subindices[i * 6 + 2]].x;
        mesh.m_vTexCoord[i].y = /*1.f -*/ texcoord[subindices[i * 6 + 2]].y;

        mesh.m_vTangent[i] =  tangent[subindices[i * 6 + 4]];
    }

    uint nsubmesh = READ_UINT(file);

    mesh.m_vDrawProp.reserve(nsubmesh);
    mesh.m_vDrawProp.resize(nsubmesh);

    if (nsubmesh > 0)
    {
        for (int i = 0; i < nsubmesh; ++i)
        {
            mesh.m_vDrawProp[i].material = READ_UINT(file);
            mesh.m_vDrawProp[i].indexBase = READ_USHORT(file);
            mesh.m_vDrawProp[i].count = READ_USHORT(file);
            mesh.m_vDrawProp[i].vertexBase = 0;
            file->skip(4);
        }
    }

    file->skip(2 * sizeof(Vector3f));
}

void EluLoader::LoadMesh5013(IFile *file, EluMesh &mesh)
{
    uint i = file->getOffset();
    char * str = (char*)file->read(READ_UINT(file));
    mesh.m_name = str;

    file->skip(4);

    file->skip(READ_UINT(file));

    file->read(sizeof(Matrix4f), &mesh.m_World);
    file->skip(16);

    uint npos = READ_UINT(file);
    Vector3f * position = (Vector3f*)file->read(sizeof(Vector3f) * npos);

    uint ntex = READ_UINT(file);
    Vector3f * texcoord = (Vector3f*)file->read(sizeof(Vector3f) * ntex);

    file->skip(sizeof(Vector3f) * READ_UINT(file));

    uint nnor = READ_UINT(file);
    Vector3f * normal = (Vector3f*)file->read(sizeof(Vector3f) * nnor);

    uint ntan = READ_UINT(file);
    Vector4f * tangent = (Vector4f*)file->read(sizeof(Vector4f) * ntan);

    file->skip(sizeof(Vector3f) * READ_UINT(file));


    //Read indices
    uint ntris = READ_UINT(file);
    if (ntris > 0)
    {
        file->skip(4);
        ntris = READ_UINT(file);

        for (int i = 0; i < ntris; ++i)
        {
            uint nverts = READ_UINT(file);
            file->skip(12 * nverts);
            file->skip(2);
        }
    }

    file->skip(sizeof(Vector3f)*READ_UINT(file));
    file->skip(4);

    uint bcount = READ_UINT(file);
    for (int i = 0; i < bcount; ++i)
    {
        uint bi = READ_UINT(file);
        file->skip(8 * bi);
    }

    file->skip(4);

    uint nsubindex = READ_UINT(file);
    //pos, nor, tex, ??, tan, ??
    ushort * subindices = (ushort*) file->read(6 * sizeof(ushort) * nsubindex);

    file->skip((64 + 2) * READ_UINT(file));

    uint nsubmesh = READ_UINT(file);

    mesh.m_vDrawProp.reserve(nsubmesh);
    mesh.m_vDrawProp.resize(nsubmesh);

    if (nsubmesh > 0)
    {
        for (int i = 0; i < nsubmesh; ++i)
        {
            mesh.m_vDrawProp[i].material = READ_UINT(file);
            mesh.m_vDrawProp[i].indexBase = READ_USHORT(file);
            mesh.m_vDrawProp[i].count = READ_USHORT(file);
            mesh.m_vDrawProp[i].vertexBase = 0;
            file->skip(4);
        }
    }

    mesh.m_uiVertexCount = nsubindex;
    mesh.m_uiIndexCount = READ_UINT(file);
    mesh.m_vIndex = new ushort[mesh.m_uiIndexCount];
    file->read(mesh.m_uiIndexCount * sizeof(ushort), mesh.m_vIndex);

    mesh.m_vPosition = new Vector3f[nsubindex];
    mesh.m_vNormal =   new Vector3f[nsubindex];
    mesh.m_vTexCoord = new Vector2f[nsubindex];
    mesh.m_vTangent =  new Vector4f[nsubindex];

    for (int i = 0; i < nsubindex; ++i)
    {
        mesh.m_vPosition[i] = position[subindices[i * 6 + 0]];
        mesh.m_vNormal[i]   =   normal[subindices[i * 6 + 1]];

        mesh.m_vTexCoord[i].x = texcoord[subindices[i * 6 + 2]].x;
        mesh.m_vTexCoord[i].y = /*1.f -*/ texcoord[subindices[i * 6 + 2]].y;

        mesh.m_vTangent[i] =  tangent[subindices[i * 6 + 4]];
    }

    file->skip(2 * sizeof(Vector3f));
}

bool EluLoader::loadMaterial(const char* name, std::vector<std::shared_ptr<EluMaterial>>& vMats)
{
    XMLMaterialVector xMats;
    bool succeeded = false;

    TRY_FOR_EACH_PATH(succeeded = XMLParser::parseXMLMaterial(path.c_str(), xMats) == true, name);
    if (!succeeded)
        return false;

    XMLMaterialVector::iterator itor;
    vMats.reserve(xMats.size());
    for (itor = xMats.begin(); itor != xMats.end(); ++itor)
    {
        EluMaterial * mtl = new EluMaterial();
        mtl->cAmbient = Vector4f(itor->abient[0], itor->abient[1], itor->abient[2], 1.0);
        mtl->cDiffuse = Vector4f(itor->diffuse[0], itor->diffuse[1], itor->diffuse[2], 1.0);
        mtl->cSpecular = Vector4f(itor->specular[0], itor->specular[1], itor->specular[2], 1.0);
        mtl->cEmissive = Vector4f(0.0f);
        mtl->shininess = 0.f;
        mtl->roughness = 0.f;
        mtl->m_pTexMgr = m_pTexMgr;

        if (itor->Flag & FLAG_DIFFUSE)
        {
            TRY_FOR_EACH_PATH((mtl->tDiffuse = m_pTexMgr->load(path.c_str())) != nullptr, itor->DiffuseMap.c_str())
            //mtl->tDiffuse = m_pTexMgr->load(itor->DiffuseMap.c_str());
        }
        if (itor->Flag & FLAG_NORMAL)
        {
            TRY_FOR_EACH_PATH((mtl->tNormal = m_pTexMgr->load(path.c_str())) != nullptr, itor->NormalMap.c_str())
            //mtl->tNormal = m_pTexMgr->load(itor->NormalMap.c_str());
        }
        if (itor->Flag & FLAG_SPECULAR)
        {
            TRY_FOR_EACH_PATH((mtl->tSpecular = m_pTexMgr->load(path.c_str())) != nullptr, itor->SpecularMap.c_str())
            //mtl->tSpecular = m_pTexMgr->load(itor->SpecularMap.c_str());
        }
        if (itor->Flag & FLAG_OPACITY)
        {
            TRY_FOR_EACH_PATH((mtl->tOpacity = m_pTexMgr->load(path.c_str())) != nullptr, itor->OpacityMap.c_str())
            //mtl->tOpacity = m_pTexMgr->load(itor->OpacityMap.c_str());
        }
        if (itor->Flag & FLAG_SELFILLUM)
        {
            TRY_FOR_EACH_PATH((mtl->tEmissive = m_pTexMgr->load(path.c_str())) != nullptr, itor->SelfIlluminationMap.c_str())
            //mtl->tEmissive = m_pTexMgr->load(itor->SelfIlluminationMap.c_str());
        }

        vMats.push_back(std::shared_ptr<EluMaterial>(mtl));
    }
    return true;
}

bool EluLoader::loadTree(const char * sceneName, std::vector<IObject*>& vObject, std::vector<ILight*> *vLights)
{
    XMLActor actor;
    XMLObjectVector ret;
    XMLLightVector lights;
    bool succeeded = false;

    TRY_FOR_EACH_PATH(succeeded = XMLParser::parseScene(path.c_str(), actor, ret, &lights) == true, sceneName);

    if (!succeeded)
        return false;

    if (vLights != nullptr)
    {
        XMLLightVector::iterator itor;
        for (itor = lights.begin(); itor != lights.end(); ++itor)
        {
            EluLight * l = new EluLight();
            l->position = itor->Position;
            l->direction = itor->Dir;
            l->diffuse = itor->Diffuse;
            l->attEnd = itor->AttEnd;
            l->intensity = itor->Intensity;
            vLights->push_back(l);
        }
    }

    if (actor.isValid)
    {
        IObject * pobj = load(actor.eluName.c_str());
        vObject.push_back(pobj);
    }

    XMLObjectVector::iterator itor;
    for (itor = ret.begin(); itor != ret.end(); ++itor)
    {
        loadTree(*itor, vObject, ret);
    }
    return true;
}

bool EluLoader::loadTree(XMLObject& xmlObject, std::vector<IObject*>& vObject, XMLObjectVector &ret)
{
    XMLActor actor;
    bool succeeded = false;

    TRY_FOR_EACH_PATH(succeeded = XMLParser::parseScene(path.c_str(), actor, ret) == true, xmlObject.SceneXMLFile.c_str());

    if (!succeeded)
        return false;

    if (actor.isValid)
    {
        IObject * pobj = load(actor.eluName.c_str());
        vObject.push_back(pobj);

        Matrix4f mt, mr;
        mt.translation(xmlObject.Position);
        mr.fromDirection(xmlObject.Dir, xmlObject.Up);
        *pobj->getWorldMatrix() = (mt * mr).transpose();

        return true;
    }
    return false;
}

bool EluLoader::loadPropTree(const char * propName, std::vector<IObject*>& vObject)
{
    XMLObjectVector ret;
    bool succeeded = false;

    TRY_FOR_EACH_PATH(succeeded = XMLParser::parseProp(path.c_str(), ret) == true, propName);
    if (!succeeded)
        return false;

    XMLObjectVector::iterator itor;
    for (itor = ret.begin(); itor != ret.end(); ++itor)
    {
        loadTree(*itor, vObject, ret);
    }
    return true;
}

bool EluLoader::load(std::vector<std::string>& args, std::vector<IObject*>& objects, std::vector<ILight*>& lights)
{
    if (args.size() < 1) return false;
    XMLParser::setFileManager(m_pFileMgr);

    PathManager::clear();
    for (int i = 1; i < args.size(); ++i)
    {
        PathManager::add(args[i]);
    }

    std::string name = args[0];
    loadTree(name.append(".scene.xml").c_str(), objects, &lights);
    name = args[0];
    return loadPropTree(name.append(".prop.xml").c_str(), objects);
}

void EluLoader::flush()
{
    m_vObject.clear();
}

}
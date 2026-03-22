#pragma once
#include "../defs.h"
#include "../Matrix.h"
#include "../Interface.h"
#include "XMLParser.h"

namespace rsx {

struct EluHeader
{
    uint Signature;
    uint Version;
    uint matCount;
    uint meshCount;
};

class EluLight : public ILight
{
public:
    Vector3f position;
    Vector3f direction;
    Vector3f ambient;
    Vector3f diffuse;
    Vector3f specular;
    float attEnd;
    float intensity;
    std::string name;

    virtual ~EluLight() {}

    uint getType() {return 0;}
    Vector3f& getPosition() {return position;}
    Vector3f& getDirection() {return direction;}
    Vector3f& getAmbientColor() {return ambient;}
    Vector3f& getDiffuseColor() {return diffuse;}
    Vector3f& getSpecularColor() {return specular;}
    const char * getName() {return name.c_str();}
    float getRadius() {return attEnd;}
    float getIntensity() {return intensity;}

};

class EluLoader;

class EluMaterial: public IMaterial
{
    friend EluLoader;

    Vector4f cDiffuse;
    Vector4f cAmbient;
    Vector4f cSpecular;
    Vector4f cEmissive;

    float shininess;
    float roughness;

    ITexture * tDiffuse;
    ITexture * tNormal;
    ITexture * tSpecular;
    ITexture * tOpacity;
    ITexture * tEmissive;

    ITextureManager * m_pTexMgr;
public:

    EluMaterial(): tDiffuse(nullptr), tNormal(nullptr),tSpecular(nullptr),tOpacity(nullptr),tEmissive(nullptr) {}
    virtual ~EluMaterial(){release();}

    void release()
    {
        if (m_pTexMgr)
        {
            m_pTexMgr->release(tDiffuse);
            m_pTexMgr->release(tNormal);
            m_pTexMgr->release(tSpecular);
            m_pTexMgr->release(tOpacity);
            m_pTexMgr->release(tEmissive);
        }
        tDiffuse = tNormal = tSpecular = tOpacity = tEmissive = nullptr;
    }

    ITexture * getDiffuseTexture() {return tDiffuse;}
    ITexture * getNormalTexture() {return tNormal;}
    ITexture * getSpecularTexture() {return tSpecular;}
    ITexture * getOpacityTexture() {return tOpacity;}
    ITexture * getEmissiveTexture() {return tEmissive;}

    Vector4f * getDiffuseColor() {return &cDiffuse;}
    Vector4f * getAmbientColor() {return &cAmbient;}
    Vector4f * getSpecularColor() {return &cSpecular;}
    Vector4f * getEmissiveColor() {return &cEmissive;}

    float getShininess() {return shininess;}
};

class EluMesh: public IMesh
{
    friend EluLoader;
protected:
    Vector3f * m_vPosition;
    Vector3f * m_vNormal;
    Vector2f * m_vTexCoord;
    Vector4f * m_vTangent;
    ushort * m_vIndex;

    uint m_uiIndexCount;
    uint m_uiVertexCount;

    std::vector<DrawProp> m_vDrawProp;

    Matrix4f m_World;

    std::string m_name;
public:
    virtual ~EluMesh() {release();}

    void release()
    {
        SAFE_DELETE(m_vPosition);
        SAFE_DELETE(m_vNormal);
        SAFE_DELETE(m_vTexCoord);
        SAFE_DELETE(m_vTangent);
        SAFE_DELETE(m_vIndex);
    }

    Vector3f * getPosition() {return m_vPosition;}
    Vector3f * getNormal() {return m_vNormal;}
    Vector2f * getTexCoord() {return m_vTexCoord;}
    Vector4f * getTangent() {return m_vTangent;}
    ushort * getIndex() {return m_vIndex;}

    uint getVertexCount() {return m_uiVertexCount;}
    uint getIndexCount() {return m_uiIndexCount;}

    Matrix4f * getWorldMatrix() {return &m_World;}

    std::vector<DrawProp>& getDrawProp() {return m_vDrawProp;}

    const char * getName() {return m_name.c_str();}

    uint getType() {return D3DPT_TRIANGLELIST;}
};

class EluObject: public IObject
{
    friend EluLoader;
protected:
    std::vector<std::shared_ptr<EluMesh>> m_pMeshes;
    Matrix4f m_World;
    std::string m_name;
    std::vector<std::shared_ptr<EluMaterial>> m_vMaterial;
public:
    virtual ~EluObject() {}
    IMesh * getMesh(uint i) {return &(*m_pMeshes[i]);}
    uint getMeshCount() {return m_pMeshes.size();}
    Matrix4f * getWorldMatrix() {return &m_World;}
    const char * getName() {return m_name.c_str();}
    IMaterial * getMaterial(uint id) {return &(*m_vMaterial[id]);}

    EluObject * clone()
    {
        EluObject * obj = new EluObject();
        obj->m_pMeshes = m_pMeshes;
        obj->m_World = m_World;
        obj->m_name = m_name;
        obj->m_vMaterial = m_vMaterial;

        return obj;
    }
};

class EluLoader: public IObjectLoader
{
    void LoadMesh5012(IFile *file, EluMesh &mesh);
    void LoadMesh5013(IFile *file, EluMesh &mesh);
    bool loadMaterial(const char* name, std::vector<std::shared_ptr<EluMaterial>>& vMats);

    ITextureManager * m_pTexMgr;
    IFileManager * m_pFileMgr;

    std::map<std::string, simple_object_ref<IObject>> m_vObject;
public:
    EluLoader(IEngine * pEngine);

    virtual IObject * load(const char * name);
    virtual bool release(IObject * pObj){ return false;}
    virtual bool load(std::vector<std::string>& args, std::vector<IObject*>& objects, std::vector<ILight*>& lights);


    bool loadTree(const char * sceneName, std::vector<IObject*>& vObject, std::vector<ILight*> *vLights = 0);
    bool loadTree(XMLObject& xmlObject, std::vector<IObject*>& vObject, XMLObjectVector &ret);
    bool loadPropTree(const char * propName, std::vector<IObject*>& vObject);

    void flush();
};

}



#pragma once
#include "defs.h"
#include "defines.h"
#include "Matrix.h"
#include "../FileSystem/IFileManager.h"

struct TextureDescriptor
{
    uint width;
    uint height;
    uint format;
    bool mip;
};


class ITexture
{
public:
    virtual ~ITexture() {}
    virtual TextureDescriptor * getDesc() = 0;
    virtual void bind(uint unit) = 0;
};


class ITextureManager
{
public:
    virtual ITexture * load(const char * name) = 0;
    virtual ITexture * create(TextureDescriptor& desc) = 0;
    virtual bool release(ITexture * pTexture) = 0;

    virtual void onLost() = 0;
    virtual void onReset() = 0;
};


class IMaterial
{
public:
    virtual ~IMaterial() {}
    virtual ITexture * getDiffuseTexture() = 0;
    virtual ITexture * getNormalTexture() = 0;
    virtual ITexture * getSpecularTexture() = 0;
    virtual ITexture * getOpacityTexture() = 0;
    virtual ITexture * getEmissiveTexture() = 0;

    virtual Vector4f * getDiffuseColor() = 0;
    virtual Vector4f * getAmbientColor() = 0;
    virtual Vector4f * getSpecularColor() = 0;
    virtual Vector4f * getEmissiveColor() = 0;

    virtual float getShininess() = 0;
};

struct DrawProp
{
    uint vertexBase;
    uint indexBase;
    uint count;
    int material;
};


class IMesh
{
public:
    virtual ~IMesh() {}
    virtual Vector3f * getPosition() = 0;
    virtual Vector3f * getNormal() = 0;
    virtual Vector2f * getTexCoord() = 0;
    virtual Vector4f * getTangent() = 0;
    virtual ushort * getIndex() = 0;

    virtual uint getVertexCount() = 0;
    virtual uint getIndexCount() = 0;

    virtual Matrix4f * getWorldMatrix() = 0;

    virtual const char * getName() = 0;

    virtual uint getType() = 0;

    virtual std::vector<DrawProp>& getDrawProp() = 0;
};

class ILight
{
public:
    virtual ~ILight() {}
    virtual uint getType() = 0;
    virtual Vector3f& getPosition() = 0;
    virtual Vector3f& getDirection() = 0;
    virtual Vector3f& getAmbientColor() = 0;
    virtual Vector3f& getDiffuseColor() = 0;
    virtual Vector3f& getSpecularColor() = 0;
    virtual const char * getName() = 0;
    virtual float getRadius() = 0;
    virtual float getIntensity() = 0;
};

class IObject
{
public:
    virtual ~IObject() {}
    virtual IMesh * getMesh(uint i) = 0;
    virtual uint getMeshCount() = 0;
    virtual Matrix4f * getWorldMatrix() = 0;
    virtual const char * getName() = 0;
    virtual IMaterial * getMaterial(uint id) = 0;
};


class IObjectLoader {
public:
    virtual IObject * load(const char * name) = 0;
    virtual bool load(std::vector<std::string>& args, std::vector<IObject*>& objects, std::vector<ILight*>& lights) = 0;
};

class IShader
{
};


class IRenderTarget
{
public:
    virtual ~IRenderTarget() {}
    virtual bool setDepth(ITexture * tex) = 0;
    virtual bool setColor(uint n, ITexture *tex) = 0;
    virtual bool bind() = 0;
    virtual bool unbind() = 0;
};

class IRenderer;
class IEngine;

class IRendererTechnique
{
public:
	virtual bool initialize(IEngine * engine) = 0;
	virtual void release() = 0;
    virtual void begin() = 0;
    virtual void draw(IObject *pObject) = 0;
    virtual void apply(IMaterial *pmtl) = 0;
    virtual void end() = 0;

    virtual void onLost() = 0;
    virtual void onReset() = 0;
};

class IRenderer
{
public:
    virtual void addObject(IObject * pObj) = 0;

    virtual void removeObject(IObject * pObj) = 0;
    virtual void removeObject(const char * name) = 0;

    virtual void addLight(ILight * pLight) = 0;

    virtual void removeLight(const char * name) = 0;
    virtual void removeLight(ILight * pLight) = 0;

    virtual const std::list<IObject*> * getObjects() const = 0;
    virtual const std::list<ILight*> * getLights() const = 0;

    virtual void setTechnique(IRendererTechnique * pTech) = 0;
    virtual IRendererTechnique * getTechnique() const = 0;
    virtual void draw() = 0;

    virtual IRenderTarget * createRenderTarget() = 0;

    virtual void getSize(uint& width, uint &height) = 0;

    virtual void setAlphaTest(bool status) = 0;
    virtual void setAlphaFunc(uint func) = 0;

    virtual void setBlendingEnable(bool status) = 0;
    virtual void setBlendingOp(uint op, uint src, uint dest) = 0;

    virtual void setDepthTest(bool status) = 0;
    virtual void setDepthWrite(bool status) = 0;
    virtual void setDepthFunc(uint func) = 0;

    virtual void setCullMode(uint mode) = 0;

    virtual void setTexMinFilter(uint unit, uint filter) = 0;
    virtual void setTexMagFilter(uint unit, uint filter) = 0;
    virtual void setTexModeU(uint unit, uint mode) = 0;
    virtual void setTexModeV(uint unit, uint mode) = 0;

    virtual void drawIndexed(uint primitive, uint vertexBase, uint indexBase, uint vertexCount, uint primitiveCount) = 0;

    virtual void onLost() = 0;
    virtual void onReset() = 0;

	virtual void clear(bool z) = 0;
	virtual Matrix4f& getViewMatrix() = 0;
	virtual void setViewMatrix(Matrix4f& m) = 0;
	virtual Matrix4f& getProjectionMatrix() = 0;
	virtual void setProjectionMatrix(Matrix4f& m) = 0;

	virtual bool setViewPort(uint x, uint y, uint width, uint height) = 0;

    virtual void drawQuad() = 0;

    virtual bool setDefaultRenderTarget() = 0;
};

class IBuffer
{
public:
    virtual ~IBuffer() {};
    virtual uint getSize() = 0;
    virtual uint getType() = 0;
    virtual void * map() = 0;
    virtual void unmap() = 0;
};

class IDrawBuffer 
{
public:
    virtual bool bind() = 0;
    virtual void unbind() = 0;
};

class IBufferManager 
{
public:
    virtual IBuffer * create(uint type, uint size) = 0;
    virtual IDrawBuffer * get(IObject * pObject) = 0;
    virtual void clear() = 0;

    virtual void onLost() = 0;
    virtual void onReset() = 0;
};

class ICollisionSystem
{
public:
    virtual bool addObject(IObject * pObject) = 0;
    virtual bool build() = 0;
    virtual void clear() = 0;

    virtual bool pick(Vector3f &from, Vector3f &to, Vector3f &hit, Vector3f &normal) const = 0;
	virtual bool checkCylinder(Vector3f &from, Vector3f &to, Vector3f &hit, Vector3f &normal, float radius, float height) const = 0;
	virtual bool checkSphere(Vector3f &from, Vector3f &to, Vector3f &hit, Vector3f &normal, float radius) const = 0;
};

class IEngine
{
public:
    virtual void * getContext() const = 0;
    virtual bool initialize(void * ctx) = 0;
    virtual void release() = 0;
    virtual IRenderer * getRenderer() const = 0;
    virtual ITextureManager * getTextureManager() const = 0;
    virtual IBufferManager * getBufferManager() const = 0;
    virtual IObjectLoader * getObjectLoader() const = 0;
    virtual IFileManager * geFileManager() const = 0;
    virtual ICollisionSystem * getCollisionSystem() const = 0;

    virtual void onLost() = 0;
    virtual void onReset() = 0;
};

template<class T> 
struct simple_object_ref {
    T * object;
    uint count;
    simple_object_ref() : object(nullptr), count(0) {}
    simple_object_ref(simple_object_ref<T>& other) 
    {
        this->object = other.object;
        this->count = other.count;
        other.object = nullptr;
    }
    simple_object_ref(T * obj) : object(obj), count(1) {}
    ~simple_object_ref() 
    {
        //SAFE_DELETE(object);
        if (object != nullptr) delete object;
        object = nullptr;
    }
    T * incRef() {++count; return object;}
    uint decRef() {return --count;}

    bool operator == (T * other) {return object == other;}

    simple_object_ref<T>& operator = (simple_object_ref<T>& other)
    {
        this->object = other.object;
        this->count = other.count;
        other.object = nullptr;
        return *this;
    }

    simple_object_ref<T>& operator = (T * other)
    {
        this->object = other;
        this->count = 1;
        return *this;
    }
};
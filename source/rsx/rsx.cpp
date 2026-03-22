#include "Interface.h"
#include "DxTextureManager.h"
#include "DxBufferManager.h"
#include "DxRenderer.h"
#include "DxSimpleTechnique.h"
#include "FileSystem.h"
#include "BulletCollision.h"
#include "DxDeferredTechnique.h"

#include "elu/elu.h"

namespace rsx {

class RSX : public IEngine
{
    DxRenderer * m_pRenderer;
    DxBufferManager * m_pBufferManager;
    DxTextureManager * m_pTextureManager;
    DxSimpleTechnique * m_pSimpleTechnique;
    DxDeferredTechnique * m_pDeferredTechnique;
    IFileManager * m_FileManager;
    EluLoader * m_pEluLoader;
    BulletCollision * m_pCollisionSystem;
    LPDIRECT3DDEVICE9 m_pD3DDevice;
public:
    RSX() : m_pD3DDevice(nullptr), m_FileManager(nullptr), m_pBufferManager(nullptr),
        m_pTextureManager(nullptr), m_pRenderer(nullptr), m_pSimpleTechnique(nullptr), m_pEluLoader(nullptr),
        m_pCollisionSystem(nullptr), m_pDeferredTechnique(nullptr)
    {
    }
    RSX(LPDIRECT3DDEVICE9 dev) {initialize(dev);}

    ~RSX()
    {
        release();
    }

    virtual bool initialize(void * ctx)
    {
        release();
        m_pD3DDevice = (LPDIRECT3DDEVICE9)ctx;
        m_FileManager = ::getFileManager();
        m_pBufferManager = new DxBufferManager(this);
        m_pTextureManager = new DxTextureManager(this);
        m_pRenderer = new DxRenderer(this);
        m_pSimpleTechnique = new DxSimpleTechnique();
        m_pEluLoader = new EluLoader(this);
        m_pCollisionSystem = new BulletCollision(this);
        m_pRenderer->setTechnique(m_pSimpleTechnique);

        m_pDeferredTechnique = new DxDeferredTechnique();
        m_pRenderer->setTechnique(m_pDeferredTechnique);

        return m_pD3DDevice && m_FileManager && m_pBufferManager && m_pTextureManager &&
        m_pRenderer && m_pSimpleTechnique &&  m_pEluLoader && m_pCollisionSystem;
    }

    void release()
    {
        SAFE_DELETE(m_pEluLoader);
        SAFE_DELETE(m_pSimpleTechnique);
		SAFE_DELETE(m_pDeferredTechnique);
        SAFE_DELETE(m_pBufferManager);
        SAFE_DELETE(m_pTextureManager);
        SAFE_DELETE(m_pRenderer);
        SAFE_DELETE(m_FileManager);
        SAFE_DELETE(m_pCollisionSystem);
    }

    virtual void * getContext() const {return m_pD3DDevice;}
    virtual IRenderer * getRenderer() const {return m_pRenderer;}
    virtual ITextureManager * getTextureManager() const {return m_pTextureManager;}
    virtual IBufferManager * getBufferManager() const {return m_pBufferManager;}
    virtual IObjectLoader * getObjectLoader() const {return m_pEluLoader;}
    virtual IFileManager * geFileManager(void) const {return m_FileManager;}
    virtual ICollisionSystem * getCollisionSystem() const {return m_pCollisionSystem;}

    virtual void onLost()
    {
        m_pBufferManager->onLost();
        m_pTextureManager->onLost();
        m_pRenderer->onLost();
    }

    virtual void onReset()
    {
        m_pBufferManager->onReset();
        m_pTextureManager->onReset();
        m_pRenderer->onReset();
    }
};

template<typename Interface, typename Impl = Interface>
class Singleton
{
    Interface * instance;
public:
    Singleton() : instance(nullptr) {}
    ~Singleton()
    {
        SAFE_DELETE(instance);
    }
    Interface * getInstance()
    {
        if (instance == nullptr)
            instance = new Impl();
        return instance;
    }

    Interface * operator -> () const
    {
        return instance;
    }
};

IEngine * getEngine()
{
    static Singleton<IEngine, RSX> engine;
    return engine.getInstance();
}

}
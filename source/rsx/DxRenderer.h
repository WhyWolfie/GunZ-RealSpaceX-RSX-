#pragma once
#include "Interface.h"
#include "DxTextureManager.h"

namespace rsx {

class DxRenderer;

class DxRenderTarget : public IRenderTarget
{
    DxRenderer * m_pRenderer;
    LPDIRECT3DDEVICE9 m_pD3DDevice;
    std::map<uint, DxTexture*> m_vTextures;
public:
    DxRenderTarget(DxRenderer *r, LPDIRECT3DDEVICE9 dev): m_pRenderer(r), m_pD3DDevice(dev) {}
    virtual bool setDepth(ITexture * tex);
    virtual bool setColor(uint n, ITexture *tex);
    virtual bool bind();
    virtual bool unbind();
};

class DefRenderTarget
{
    LPDIRECT3DSURFACE9 m_pDepthSurf;
    LPDIRECT3DSURFACE9 m_pColorSurf;
    LPDIRECT3DDEVICE9 m_pDev;
    D3DSURFACE_DESC desc;
public:
    DefRenderTarget() : m_pDev(nullptr), m_pColorSurf(nullptr), m_pDepthSurf(nullptr) {}
    DefRenderTarget(LPDIRECT3DDEVICE9 dev) :m_pDev(dev), m_pColorSurf(nullptr), m_pDepthSurf(nullptr) {reload();}
    ~DefRenderTarget() {release();}

    void reload()
    {
        release();
        m_pDev->GetDepthStencilSurface(&m_pDepthSurf);
        m_pDev->GetRenderTarget(0, &m_pColorSurf);
        if (m_pColorSurf != nullptr)
            m_pColorSurf->GetDesc(&desc);
    }

    uint getWidth()
    {
        return desc.Width;
    }

    uint getHeight()
    {
        return desc.Height;
    }

    void bind()
    {
        m_pDev->SetDepthStencilSurface(m_pDepthSurf);
        m_pDev->SetRenderTarget(0, m_pColorSurf);
        for (uint i = 1; i < 4; ++i)
        {
            m_pDev->SetRenderTarget(i, nullptr);
        }
    }

    void release()
    {
        SAFE_RELEASE(m_pDepthSurf);
        SAFE_RELEASE(m_pColorSurf);
        memset(&desc, 0, sizeof(D3DSURFACE_DESC));
    }
};


class DxRenderer : public IRenderer
{
    std::list<IObject*> m_vObjects;
    std::list<ILight*> m_vLights;
    DefRenderTarget m_defRT;
    IRendererTechnique *m_pTechnique;
    IBufferManager * m_pBufferManager;
    LPDIRECT3DDEVICE9 m_pD3DDevice;
    IEngine * m_pEngine;
	Matrix4f view, projection;
public:
    DxRenderer(IEngine * pEngine);
    virtual void addObject(IObject * pObj);

    virtual void removeObject(IObject * pObj);
    virtual void removeObject(const char * name);

    virtual void addLight(ILight * pLight);

    virtual void removeLight(const char * name);
    virtual void removeLight(ILight * pLight);

    virtual const std::list<IObject*> * getObjects() const;
    virtual const std::list<ILight*> * getLights() const;

    virtual void setTechnique(IRendererTechnique * pTech);
    virtual IRendererTechnique * getTechnique() const;
    virtual void draw();

    bool setDefaultRenderTarget();

    void getSize(uint& width, uint &height)
    {
        width = m_defRT.getWidth();
        height = m_defRT.getHeight();
    }

    IRenderTarget * createRenderTarget()  {return new DxRenderTarget(this, m_pD3DDevice);}

    void setAlphaTest(bool status)
    {
        m_pD3DDevice->SetRenderState(D3DRS_ALPHATESTENABLE, (uint)status);
    }

    void setAlphaFunc(uint func)
    {
        m_pD3DDevice->SetRenderState(D3DRS_ALPHAFUNC, func);
    }

    void setBlendingEnable(bool status)
    {
        m_pD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, (uint)status);
    }

    void setBlendingOp(uint op, uint src, uint dest)
    {
        m_pD3DDevice->SetRenderState(D3DRS_BLENDOP, op);
        m_pD3DDevice->SetRenderState(D3DRS_SRCBLEND, src);
        m_pD3DDevice->SetRenderState(D3DRS_DESTBLEND, dest);
    }

    void setDepthTest(bool status)
    {
        m_pD3DDevice->SetRenderState(D3DRS_ZENABLE, (uint)status);
    }

    void setDepthWrite(bool status)
    {
        m_pD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE, (uint)status);
    }

    void setDepthFunc(uint func)
    {
        m_pD3DDevice->SetRenderState(D3DRS_ZFUNC, func);
    }

    void setCullMode(uint mode)
    {
        m_pD3DDevice->SetRenderState(D3DRS_CULLMODE, mode);
    }

    void setTexMinFilter(uint unit, uint filter)
    {
        m_pD3DDevice->SetSamplerState(unit, D3DSAMP_MAGFILTER, filter);
    }

    void setTexMagFilter(uint unit, uint filter)
    {
        m_pD3DDevice->SetSamplerState(unit, D3DSAMP_MINFILTER, filter);
    }

    void setTexModeU(uint unit, uint mode)
    {
        m_pD3DDevice->SetSamplerState(unit, D3DSAMP_ADDRESSU, mode);
    }

    void setTexModeV(uint unit, uint mode)
    {
        m_pD3DDevice->SetSamplerState(unit, D3DSAMP_ADDRESSV, mode);
    }

    void drawIndexed(uint primitive, uint vertexBase, uint indexBase, uint vertexCount, uint primitiveCount)
    {
        m_pD3DDevice->DrawIndexedPrimitive((D3DPRIMITIVETYPE)primitive, vertexBase, 0, vertexCount, indexBase, primitiveCount);
    }

    void loadBuffers()
    {
        std::list<IObject*>::iterator itor2 = m_vObjects.begin();
        while (itor2 != m_vObjects.end())
        {
            IDrawBuffer * db = m_pEngine->getBufferManager()->get(*itor2);
            if (db == nullptr)
            {
                printf("No buffer for : %s\n", (*itor2)->getName());
            }
            printf("Buffer for : %s\n", (*itor2)->getName());

            ++itor2;
        }
    }

    virtual void onLost()
    {
        m_defRT.release();
		if (m_pTechnique) m_pTechnique->onLost();
    }
    virtual void onReset()
    {
        m_defRT.reload();
		if (m_pTechnique) m_pTechnique->onReset();
    }

	virtual void clear(bool z)
	{
		m_pD3DDevice->Clear(0, NULL,  z ? (D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER) : D3DCLEAR_TARGET, 0x0, 1.0f, 0);
	}

	virtual Matrix4f& getViewMatrix()
	{
		return view;
	}

	virtual void setViewMatrix(Matrix4f& m)
	{
		view = m;
	}

	virtual Matrix4f& getProjectionMatrix()
	{
		return projection;
	}

	virtual void setProjectionMatrix(Matrix4f& m)
	{
		projection = m;
	}

	virtual bool setViewPort(uint x, uint y, uint width, uint height)
	{
		D3DVIEWPORT9 vp = {x, y, width, height, 0.f, 1.f};
        return SUCCEEDED(m_pD3DDevice->SetViewport(&vp));
	}

    virtual void drawQuad() {

    }
};

}
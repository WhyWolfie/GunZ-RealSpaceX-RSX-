#include "Interface.h"
#include "TextureManager.h"
#include "Renderer.h"

class DxRenderTarget : public IRenderTarget
{
    DxRenderer * m_pRenderer;
    LPDIRECT3DDEVICE9 m_pD3DDevice;
    std::map<uint, DxTexture*> m_vTextures;
public:
    DxRenderTarget(DxRenderer *r, LPDIRECT3DDEVICE9 dev): m_pRenderer(r), m_pD3DDevice(dev) {}
    virtual bool setDepth(ITexture * tex)
    {
        DxTexture * t = dynamic_cast<DxTexture*>(tex);
        if (t == nullptr) return false;
        m_vTextures[0] = t;
        return true;
    }

    virtual bool setColor(uint n, ITexture *tex)
    {
        DxTexture * t = dynamic_cast<DxTexture*>(tex);
        if (t == nullptr) return false;
        m_vTextures[n] = t;
        return true;
    }
    virtual bool bind()
    {
        LPDIRECT3DSURFACE9 surf;
        std::map<uint, DxTexture*>::iterator itor;
        for (itor = m_vTextures.begin(); itor != m_vTextures.end(); ++itor)
        {
            itor->second->getTexture()->GetSurfaceLevel(0, &surf);
            if (itor->first == 0)
                m_pD3DDevice->SetDepthStencilSurface(surf);
            else
                m_pD3DDevice->SetRenderTarget(itor->first-1, surf);
            SAFE_RELEASE(surf);
        }
    }
    virtual bool unbind()
    {
        m_pRenderer->setDefaultRenderTarget();
    }
};
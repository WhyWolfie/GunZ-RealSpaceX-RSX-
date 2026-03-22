#include "DxRenderer.h"

namespace rsx {

bool DxRenderTarget::setDepth(ITexture * tex)
{
    DxTexture * t = dynamic_cast<DxTexture*>(tex);
    if (t == nullptr) return false;
    m_vTextures[0] = t;
    return true;
}

bool DxRenderTarget::setColor(uint n, ITexture *tex)
{
    DxTexture * t = dynamic_cast<DxTexture*>(tex);
    if (t == nullptr) return false;
    m_vTextures[n+1] = t;
    return true;
}

bool DxRenderTarget::bind()
{
    LPDIRECT3DSURFACE9 surf;
    std::map<uint, DxTexture*>::iterator itor;
    HRESULT h;
    for (itor = m_vTextures.begin(); itor != m_vTextures.end(); ++itor)
    {
        itor->second->getTexture()->GetSurfaceLevel(0, &surf);
        if (itor->first == 0)
            h = m_pD3DDevice->SetDepthStencilSurface(surf);
        else
            h = m_pD3DDevice->SetRenderTarget(itor->first-1, surf);
        SAFE_RELEASE(surf);
    }
    return true;
}

bool DxRenderTarget::unbind()
{
    return m_pRenderer->setDefaultRenderTarget();
}

DxRenderer::DxRenderer(IEngine * pEngine) : m_pD3DDevice((LPDIRECT3DDEVICE9)pEngine->getContext()),
	m_defRT((LPDIRECT3DDEVICE9)pEngine->getContext()), m_pTechnique(nullptr)
{
    m_pEngine = pEngine;
}

void DxRenderer::addObject(IObject * pObj)
{
    if (std::find(m_vObjects.begin(), m_vObjects.end(), pObj)  != m_vObjects.end()) return;
    m_vObjects.push_back(pObj);
}

void DxRenderer::removeObject(IObject * pObj)
{
    m_vObjects.remove(pObj);
}
void DxRenderer::removeObject(const char * name)
{
}

void DxRenderer::addLight(ILight * pLight)
{
    if (std::find(m_vLights.begin(), m_vLights.end(), pLight)  != m_vLights.end()) return;
    m_vLights.push_back(pLight);
}

void DxRenderer::removeLight(const char * name)
{
}

void DxRenderer::removeLight(ILight * pLight)
{
    m_vLights.remove(pLight);
}

const std::list<IObject*> *  DxRenderer::getObjects() const
{
    return &m_vObjects;
}

const std::list<ILight*> *  DxRenderer::getLights() const
{
    return &m_vLights;
}

void DxRenderer::setTechnique(IRendererTechnique * pTech)
{
	if (m_pTechnique) m_pTechnique->release();
    m_pTechnique = pTech;
	m_pTechnique->initialize(m_pEngine);
}

IRendererTechnique *  DxRenderer::getTechnique() const
{
    return m_pTechnique;
}
void DxRenderer::draw()
{
    //m_pD3DDevice->BeginScene();
    m_pTechnique->begin();
    
    std::list<IObject*>::iterator itor = m_vObjects.begin();
    //int i = 0;
    while (itor != m_vObjects.end())
    {
        //if (i == 142) 
            m_pTechnique->draw(*itor);
        ++itor;
        //++i;
    }

    m_pTechnique->end();
    //m_pD3DDevice->EndScene();
}


bool DxRenderer::setDefaultRenderTarget()
{
    m_defRT.bind();
    return true;
}

}
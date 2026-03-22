#pragma once
#include "DxErr.h"
#include "Interface.h"

#include "Sphere.h"

namespace rsx {

class DxDeferredTechnique : public IRendererTechnique
{
    IEngine * m_pEngine;
    ID3DXEffect * m_pEffect;
    ID3DXEffect * m_pEffect2;
    ID3DXEffect * m_pEffect3;
    Matrix4f proj, view, view_proj;

    IRenderTarget * m_pRT;
    IRenderTarget * m_pRT2;
    ITexture * m_pTexColor;
    ITexture * m_pTexNormal;
    ITexture * m_pTexAmbient;
    ITexture * m_pTexDepth;
    ITexture * m_pTexLinearDepth;
    ITexture * m_pTexColor2;

    uint width, height;
    float m_dx, m_dy;
    float fnear, ffar;
    uint ssaa;

public:
    DxDeferredTechnique() : m_pEngine(nullptr), m_pRT(nullptr), m_pRT2(nullptr), m_pTexColor(nullptr), m_pEffect2(nullptr), m_pEffect(nullptr),
        m_pTexNormal(nullptr), m_pTexAmbient(nullptr), m_pTexDepth(nullptr), m_pTexLinearDepth(nullptr), m_pTexColor2(nullptr)
    {
    }

    ~DxDeferredTechnique()
    {
        release();
    }

    bool initialize(IEngine * engine)
    {
        m_pEngine = engine;
        release();

        std::unique_ptr<IFile> file  = std::unique_ptr<IFile>(m_pEngine->geFileManager()->open("rsx/shaders/deferred.txt"));
        if (file.get() == nullptr)
            return false;

        LPD3DXBUFFER errbuf = NULL;
        HRESULT h = D3DXCreateEffect((LPDIRECT3DDEVICE9)m_pEngine->getContext(), file->read(file->getSize()), file->getSize(),
            nullptr, nullptr, 0, nullptr, &m_pEffect, &errbuf);

        if (FAILED(h))
            return false;

        file  = std::unique_ptr<IFile>(m_pEngine->geFileManager()->open("rsx/shaders/lighting.txt"));
        if (file.get() == nullptr)
            return false;

        h = D3DXCreateEffect((LPDIRECT3DDEVICE9)m_pEngine->getContext(), file->read(file->getSize()), file->getSize(),
            nullptr, nullptr, 0, nullptr, &m_pEffect2, &errbuf);

        if (FAILED(h))
        {
            printf("%s", errbuf->GetBufferPointer());
            return false;
        }

        file  = std::unique_ptr<IFile>(m_pEngine->geFileManager()->open("rsx/shaders/depth.txt"));
        if (file.get() == nullptr)
            return false;

        h = D3DXCreateEffect((LPDIRECT3DDEVICE9)m_pEngine->getContext(), file->read(file->getSize()), file->getSize(),
            nullptr, nullptr, 0, nullptr, &m_pEffect3, &errbuf);

        if (FAILED(h))
        {
            printf("%s", errbuf->GetBufferPointer());
            return false;
        }

        createRT();
        return true;
    }

    void release()
    {
        SAFE_RELEASE(m_pEffect);
        SAFE_RELEASE(m_pEffect2);
        releaseRT();
    }

    void releaseRT()
    {
        SAFE_DELETE(m_pRT);
        SAFE_DELETE(m_pTexColor);
        SAFE_DELETE(m_pTexNormal);
        SAFE_DELETE(m_pTexAmbient);
        SAFE_DELETE(m_pTexDepth);
        SAFE_DELETE(m_pTexLinearDepth);
        SAFE_DELETE(m_pRT2);
        SAFE_DELETE(m_pTexColor2);
    }

    void createRT()
    {
        ssaa = 0;
        TextureDescriptor desc;
        m_pEngine->getRenderer()->getSize(desc.width, desc.height);
        m_dx = 0.5f / desc.width;
        m_dy = 0.5f / desc.height;
        width = desc.width;
        height = desc.height;

        desc.width <<= ssaa;
        desc.height <<= ssaa;

        m_pRT = m_pEngine->getRenderer()->createRenderTarget();
        m_pRT2 = m_pEngine->getRenderer()->createRenderTarget();

        desc.mip = false;
        desc.format = TEX_FMT_A2RGB10;
        m_pTexColor2 = m_pEngine->getTextureManager()->create(desc);
        desc.format = TEX_FMT_ARGB8;
        m_pTexColor = m_pEngine->getTextureManager()->create(desc);
        m_pTexNormal = m_pEngine->getTextureManager()->create(desc);
        m_pTexAmbient = m_pEngine->getTextureManager()->create(desc);

        desc.format = TEX_FMT_D24S8;
        m_pTexDepth = m_pEngine->getTextureManager()->create(desc);
        desc.format = TEX_FMT_R32F;
        m_pTexLinearDepth = m_pEngine->getTextureManager()->create(desc);

        m_pRT->setDepth(m_pTexDepth);
        m_pRT->setColor(0, m_pTexColor);
        m_pRT->setColor(1, m_pTexNormal);
        m_pRT->setColor(2, m_pTexLinearDepth);
        m_pRT->setColor(3, m_pTexAmbient);



        m_pRT2->setDepth(m_pTexDepth);
        m_pRT2->setColor(0, m_pTexColor2);
    }

    virtual void begin()
    {
        m_pRT->bind();
        IRenderer * pRenderer = m_pEngine->getRenderer();
        pRenderer->clear(true);

        HRESULT h = m_pEffect->SetTechnique("DeferredFill");
        uint nPass;
        h = m_pEffect->Begin(&nPass, 0);
        h = m_pEffect->BeginPass(0);

        pRenderer->setViewPort(0, 0, width<<ssaa, height<<ssaa);

        this->proj = pRenderer->getProjectionMatrix();
        this->view = pRenderer->getViewMatrix();
        fnear = proj(4,3) * (-1.0f / proj(3,3));
        ffar = proj(3,3) * fnear / (proj(3,3) - 1.0f);

        m_pEffect->SetFloat("g_Near", fnear);
        m_pEffect->SetFloat("g_Far", ffar);
    }

    virtual void draw(IObject *pObject)
    {
        IRenderer * pRenderer = m_pEngine->getRenderer();
        IDrawBuffer * db = m_pEngine->getBufferManager()->get(pObject);

        if (db == nullptr)
            return;

        db->bind();
        pRenderer->setTexMagFilter(0, RENDERER_TEXF_ANISO);
        pRenderer->setTexMinFilter(0, RENDERER_TEXF_ANISO);

        uint vb = 0;
        uint ib = 0;

        for (int i = 0; i < pObject->getMeshCount(); ++i)
        {
            IMesh * pMesh = pObject->getMesh(i);
            std::vector<DrawProp> &dp = pMesh->getDrawProp();
            std::vector<DrawProp>::iterator itor = dp.begin();

            Matrix4f world =  *pMesh->getWorldMatrix() * *pObject->getWorldMatrix();
            applyWorldMatrix(world);

            while (itor != dp.end())
            {
                if (itor->material >= 0)
                {
                    apply(pObject->getMaterial(itor->material));
                    m_pEffect->CommitChanges();
                    pRenderer->drawIndexed(pMesh->getType(), itor->vertexBase + vb, itor->indexBase + ib, pMesh->getVertexCount(), itor->count);
                }
                itor++;
                ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(0, nullptr);
                ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(1, nullptr);
                ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(2, nullptr);
                ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(3, nullptr);
                ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(4, nullptr);
            }
            vb += pMesh->getVertexCount();
            ib += pMesh->getIndexCount();
        }
        db->unbind();
    }

    virtual void apply(IMaterial *pmtl)
    {
        HRESULT h = m_pEffect->SetFloatArray("cDiffuse", (float*)pmtl->getDiffuseColor(), 3);
        h = m_pEffect->SetFloatArray("cAmbient", (float*)pmtl->getAmbientColor(), 3);
        h = m_pEffect->SetFloatArray("cSpec", (float*)pmtl->getSpecularColor(), 3);
        h = m_pEffect->SetFloat("SpecLevel", pmtl->getShininess());
        h = m_pEffect->SetFloat("Glossines", 1.f);

        float f = 1.f;

        if (pmtl->getDiffuseTexture() != nullptr)
            pmtl->getDiffuseTexture()->bind(0);
        if (pmtl->getNormalTexture() != nullptr)
            pmtl->getNormalTexture()->bind(1);
        if (pmtl->getSpecularTexture() != nullptr)
            pmtl->getSpecularTexture()->bind(2);
        if (pmtl->getOpacityTexture() != nullptr)
        {
            pmtl->getOpacityTexture()->bind(3);
            f = 0.f;
        }
        if (pmtl->getEmissiveTexture() != nullptr)
            pmtl->getEmissiveTexture()->bind(4);

        h = m_pEffect->SetFloat("g_Opacity", f);

    }

    virtual void end()
    {
        m_pEffect->EndPass();
        m_pEffect->End();
        m_pRT->unbind();

        /*
        dumpTexture(m_pTexColor, "texcolor.jpg");
        dumpTexture(m_pTexNormal, "texnormal.jpg");
        dumpTexture(m_pTexAmbient, "texamb.jpg");
        dumpTexture(m_pTexLinearDepth, "texdepth.jpg");
        */

        //m_pRT2->bind();
        IRenderer * pRenderer = m_pEngine->getRenderer();
        pRenderer->clear(true);
        beginLighting();
        drawLighting();
        endLighting();
        //m_pRT2->unbind();
        //dumpTexture(m_pTexColor2, "texcolor201.jpg");
        pRenderer->setViewPort(0, 0, width, height);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(0, nullptr);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(1, nullptr);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(2, nullptr);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(3, nullptr);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(4, nullptr);

        depthCopy();


        /*m_pTexLinearDepth->bind(0);
        clearMatrix();
        drawQuad();
        restoreMatrix();*/

    }

    void applyWorldMatrix(Matrix4f& mat)
    {

        Matrix4f mvp, mv;
        mv = mat * view;
        mvp = mv * proj;

        HRESULT h = m_pEffect->SetMatrix("g_WorldViewProj", (D3DXMATRIX*)&mvp);
        h = m_pEffect->SetMatrix("g_World", (D3DXMATRIX*)&mat);
        h = m_pEffect->SetMatrix("g_WorldView", (D3DXMATRIX*)&mv);
    }

    virtual void onLost() 
    {
        m_pEffect->OnLostDevice();
        m_pEffect2->OnLostDevice();
        releaseRT();
    }
    virtual void onReset()
    {
        m_pEffect->OnResetDevice();
        m_pEffect2->OnResetDevice();
        createRT();
    }

    void clearMatrix()
    {
        Matrix4f identity;
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTransform(D3DTS_PROJECTION, (D3DXMATRIX*)&identity);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTransform(D3DTS_VIEW, (D3DXMATRIX*)&identity);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTransform(D3DTS_WORLD, (D3DXMATRIX*)&identity);
    }

    void restoreMatrix()
    {
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTransform(D3DTS_PROJECTION, (D3DXMATRIX*)&proj);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTransform(D3DTS_VIEW, (D3DXMATRIX*)&view);
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTexture(0, nullptr);
    }

    void drawQuad()
    {

        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1);

        static float quad[] = {-1, -1, 0, 0 + m_dx, 1 + m_dy,
            -1,  1, 0, 0 + m_dx, 0 + m_dy,
            1,  1, 0, 1 + m_dx, 0 + m_dy,
            1, -1, 0, 1 + m_dx, 1 + m_dy};
        static unsigned short idx[] = {0, 1, 2, 2, 3, 0};
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, 4, 2, idx, D3DFMT_INDEX16, quad, 5*sizeof(float));

    }

    void beginLighting()
    {
        HRESULT h;
        h = m_pEffect2->SetFloat("g_Near", fnear);
        h = m_pEffect2->SetFloat("g_Far", ffar);

        float g_InvRes[] = {1.0f/width, 1.0f/height};
        float g_InvFoc[] = {1.f/proj(1,1), 1.f/proj(2,2)}; // _11, _22

        h = m_pEffect2->SetFloatArray("g_InvRes", g_InvRes, 2);
        h = m_pEffect2->SetFloatArray("g_InvFocalLen", g_InvFoc, 2);
        h = m_pEffect2->SetMatrix("g_Proj", (D3DXMATRIX*)&proj);
    }

    void endLighting()
    {
        IRenderer * pRenderer = m_pEngine->getRenderer();
        pRenderer->setCullMode(RENDERER_CULL_BACK);
        pRenderer->setBlendingEnable(false);
        pRenderer->setDepthWrite(true);
    }

    void drawLighting()
    {
        unsigned int nPass;
        HRESULT hr;

        IRenderer * pRenderer = m_pEngine->getRenderer();
        pRenderer->setCullMode(RENDERER_CULL_NONE);
        pRenderer->setDepthWrite(false);
        pRenderer->setDepthTest(false);

        m_pTexAmbient->bind(0);
        clearMatrix();
        drawQuad();
        restoreMatrix();

        hr = m_pEffect2->SetTechnique("PointLight");
        hr = m_pEffect2->Begin(&nPass, 0);
        hr = m_pEffect2->BeginPass(0);

        m_pTexColor->bind(0);
        m_pTexNormal->bind(1);
        m_pTexLinearDepth->bind(2);
        m_pTexAmbient->bind(3);

        pRenderer->setCullMode(RENDERER_CULL_FRONT);
        pRenderer->setBlendingEnable(true);
        pRenderer->setBlendingOp(RENDERER_BLEND_OP_ADD, RENDERER_BLEND_ONE, RENDERER_BLEND_ONE);
        pRenderer->setDepthWrite(false);

        const std::list<ILight*> * lights =  pRenderer->getLights();
        std::list<ILight*>::const_iterator itor = lights->begin();

        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetFVF(D3DFVF_XYZ);
        while (itor != lights->end())
        {
            Vector3f pos = view * (*itor)->getPosition();
            float light_view[] = {pos.x, pos.y, pos.z, (*itor)->getRadius()};
            m_pEffect2->SetFloatArray("g_Light", light_view, 4);
            m_pEffect2->SetFloatArray("g_LightColor", (float*)&(*itor)->getDiffuseColor(), 3);
            m_pEffect2->CommitChanges();

            if (pos.length() < light_view[3]*1.3 + fnear)
            {
                pRenderer->setDepthTest(false);
                pRenderer->setCullMode(RENDERER_CULL_FRONT);
            }
            else
            {
                pRenderer->setDepthTest(true);
                pRenderer->setCullMode(RENDERER_CULL_BACK);
            }

            ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->DrawPrimitiveUP(D3DPT_TRIANGLELIST, data::nvert/3, data::sphere, 3*sizeof(float));

            ++itor;
        }

        hr = m_pEffect2->EndPass();
        hr = m_pEffect2->End();

        pRenderer->setBlendingEnable(false);
        pRenderer->setDepthTest(true);
        pRenderer->setDepthWrite(true);
    }

    void depthCopy()
    {
        HRESULT h;
        unsigned int nPass;
        IRenderer * pRenderer = m_pEngine->getRenderer();

        LPDIRECT3DDEVICE9 dev = (LPDIRECT3DDEVICE9)m_pEngine->getContext();
        h = dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
        //const char * str = DXGetErrorStringA(h);
        

        pRenderer->setDepthTest(true);
        pRenderer->setDepthWrite(true);
        m_pTexLinearDepth->bind(0);

        h = m_pEffect3->SetFloat("g_Near", fnear);
        h = m_pEffect3->SetFloat("g_Far", ffar);
        m_pEffect3->CommitChanges();
        h = m_pEffect3->SetTechnique("DepthCopy");
        h = m_pEffect3->Begin(&nPass, 0);
        h = m_pEffect3->BeginPass(0);
        clearMatrix();
        drawQuad();
        restoreMatrix();
        h = m_pEffect3->EndPass();
        h = m_pEffect3->End();
        h = dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF); //D3DCOLORWRITEENABLE_RED
    }


    bool dumpTexture(ITexture * tex, const char * name)
    {
        DxTexture * t = (DxTexture*)tex;
        IDirect3DSurface9 * pSurf;
        t->getTexture()->GetSurfaceLevel(0, &pSurf);
        HRESULT h = D3DXSaveSurfaceToFile(name, D3DXIFF_JPG, pSurf, nullptr, nullptr);
        return SUCCEEDED(h);
    }
};

}
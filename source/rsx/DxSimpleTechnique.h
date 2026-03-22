#pragma once
#include "Interface.h"

namespace rsx {

class DxSimpleTechnique : public IRendererTechnique
{
    IEngine * m_pEngine;
public:
    DxSimpleTechnique() : m_pEngine(nullptr){}

	virtual bool initialize(IEngine * engine) 
	{
		m_pEngine = engine; return true;
	}
	virtual void release() {}

    virtual void begin()
    {
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
                    pRenderer->drawIndexed(pMesh->getType(), itor->vertexBase + vb, itor->indexBase + ib, pMesh->getVertexCount(), itor->count);
                    //pRenderer->drawIndexed(pMesh->getType(), vb, ib, pMesh->getVertexCount(), pMesh->getIndexCount()/3);
                    //((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetFVF(D3DFVF_XYZ);
                    //((LPDIRECT3DDEVICE9)m_pEngine->getContext())->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, pMesh->getVertexCount(), pMesh->getIndexCount()/3, pMesh->getIndex(), D3DFMT_INDEX16, pMesh->getPosition(), sizeof(Vector3f));
                }
                itor++;
            }
            vb += pMesh->getVertexCount();
            ib += pMesh->getIndexCount();
        }
        db->unbind();
    }

    virtual void apply(IMaterial *pmtl)
    {
        pmtl->getDiffuseTexture()->bind(0);
    }

    virtual void end()
    {
    }

    void applyWorldMatrix(Matrix4f& mat)
    {
        ((LPDIRECT3DDEVICE9)m_pEngine->getContext())->SetTransform(D3DTS_WORLD, (D3DMATRIX*)&mat);
    }

    virtual void onLost() {}
    virtual void onReset() {}
};

}
#pragma once
#include <stdio.h>
#include "interface.h"

namespace rsx {

class DxTextureManager;

class DxTexture : public ITexture
{
    LPDIRECT3DTEXTURE9 m_pTexture;
    LPDIRECT3DDEVICE9 m_pD3DDevice;
    friend DxTextureManager;
    TextureDescriptor td;
public:
    DxTexture(LPDIRECT3DDEVICE9 dev, LPDIRECT3DTEXTURE9 t, DxTextureManager * mgr) : m_pD3DDevice(dev), m_pTexture(t){}
    ~DxTexture(){SAFE_RELEASE(m_pTexture);}
    TextureDescriptor * getDesc()
    {
        D3DSURFACE_DESC desc;
        m_pTexture->GetLevelDesc(0, &desc);
        td.format = desc.Format;
        td.height = desc.Height;
        td.width = desc.Width;
        td.mip = m_pTexture->GetLevelCount() > 1;
        return &td;
    }

    void bind(uint unit)
    {
        m_pD3DDevice->SetTexture(unit, m_pTexture);
    }

    LPDIRECT3DTEXTURE9 getTexture() {return m_pTexture;}
};


class DxTextureManager : public ITextureManager
{
    LPDIRECT3DDEVICE9 m_pD3DDevice;
    IFileManager * m_pFileManager;
    std::map<std::string, simple_object_ref<ITexture>> m_vTextures;
public:
    DxTextureManager(IEngine * pEngine) : m_pD3DDevice((LPDIRECT3DDEVICE9)pEngine->getContext()),m_pFileManager(pEngine->geFileManager()) {}

    ITexture * load(const char * name)
    {
        std::string sname(name);
        std::map<std::string, simple_object_ref<ITexture>>::iterator itor = m_vTextures.find(sname);
        if (itor != m_vTextures.end())
        {
            return itor->second.incRef();
        }
        else
        {
            LPDIRECT3DTEXTURE9 hTexture;
            std::unique_ptr<IFile> file = std::unique_ptr<IFile>(m_pFileManager->open(sname.c_str()));
            if (file.get() == nullptr)
            {
                sname.append(".dds");
                file = std::unique_ptr<IFile>(m_pFileManager->open(sname.c_str()));
                if (file.get() == nullptr)
                    return nullptr;
            }

            byte * data = file->read(file->getSize());
            if (data == nullptr)
                return nullptr;

            HRESULT h = D3DXCreateTextureFromFileInMemory(m_pD3DDevice, data, file->getSize(), &hTexture);

            if (FAILED(h))
                return nullptr;

            DxTexture * t = new DxTexture(m_pD3DDevice, hTexture, this);
            if (t != nullptr) 
            {
                m_vTextures[sname] = t;
            }
            else hTexture->Release();
            return t;
        }
    }

    ITexture * create(TextureDescriptor& desc)
    {
        bool depth = desc.format == TEX_FMT_D16 || desc.format == TEX_FMT_D24S8 || desc.format == TEX_FMT_D32;
        LPDIRECT3DTEXTURE9 hTexture;
        HRESULT h = m_pD3DDevice->CreateTexture(desc.width, desc.height, desc.mip ? 0 : 1, depth ? D3DUSAGE_DEPTHSTENCIL : D3DUSAGE_RENDERTARGET,
                                    (D3DFORMAT)desc.format, D3DPOOL_DEFAULT, &hTexture, 0);

        if (SUCCEEDED(h))
        {
            ITexture * t = new DxTexture(m_pD3DDevice, hTexture, this);
            if (t!= nullptr)
                return t;
            hTexture->Release();
        }

        return nullptr;
    }

    bool release(ITexture * pTexture)
    {
        if (pTexture == nullptr) return false;
        std::map<std::string, simple_object_ref<ITexture>>::iterator itor = m_vTextures.begin();
        while (itor != m_vTextures.end())
        {
            if (itor->second == pTexture)
            {
                if (itor->second.decRef() == 0) m_vTextures.erase(itor);
                return true;
            }
            ++itor;
        }

        delete pTexture;
        return false;
    }

    void clear()
    {
        m_vTextures.clear();
    }

    virtual void onLost()
    {
        std::map<std::string, simple_object_ref<ITexture>>::iterator itor = m_vTextures.begin();
        while (itor != m_vTextures.end())
        {
            DxTexture * pTexture = (DxTexture*)itor->second.object;
            SAFE_RELEASE(pTexture->m_pTexture);
            ++itor;
        }
    }

    virtual void onReset()
    {
        std::map<std::string, simple_object_ref<ITexture>>::iterator itor = m_vTextures.begin();
        while (itor != m_vTextures.end())
        {
            DxTexture * pTexture = (DxTexture*)itor->second.object;
            SAFE_RELEASE(pTexture->m_pTexture);

            std::unique_ptr<IFile> file = std::unique_ptr<IFile>(m_pFileManager->open(itor->first.c_str()));
            if (file.get() == nullptr)
                return;

            byte * data = file->read(file->getSize());
            if (data == nullptr)
                return;

            HRESULT h = D3DXCreateTextureFromFileInMemory(m_pD3DDevice, data, file->getSize(), &pTexture->m_pTexture);

            ++itor;
        }
    }
};

}
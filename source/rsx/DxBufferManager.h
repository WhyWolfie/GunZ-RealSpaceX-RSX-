#pragma once
#include <stdio.h>
#include "interface.h"

namespace rsx {

class DxBufferManager;

class DxBuffer : public IBuffer
{
	friend DxBufferManager;
	union anyBuf
	{
		anyBuf():pVertex(0) {}
		anyBuf(LPDIRECT3DVERTEXBUFFER9 vertex) : pVertex(vertex) {}
		anyBuf(LPDIRECT3DINDEXBUFFER9 index) :pIndex(index) {}

		LPDIRECT3DVERTEXBUFFER9 pVertex;
		LPDIRECT3DINDEXBUFFER9 pIndex;
	} buffer;

	uint type;
	uint size;
public:
	virtual ~DxBuffer()
	{
		release();
	}

	virtual void release()
	{
		switch (type)
		{
		case BUFFER_TYPE_VERTEX:
			SAFE_RELEASE(buffer.pVertex);
			break;
		case BUFFER_TYPE_INDEX:
			SAFE_RELEASE(buffer.pIndex);
			break;
		}
	}

	virtual uint getSize() {return size;}
	virtual uint getType() {return type;}

	virtual void * map() 
	{
		void * ptr;
		HRESULT h;
		switch (type)
		{
		case BUFFER_TYPE_VERTEX:
			h = buffer.pVertex->Lock(0, size, &ptr, 0);
			break;
		case BUFFER_TYPE_INDEX:
			h = buffer.pIndex->Lock(0, size, &ptr, 0);
			break;
		}
		if (SUCCEEDED(h)) return ptr;
		return nullptr;
	}

	void unmap()
	{
		switch (type)
		{
		case BUFFER_TYPE_VERTEX:
			buffer.pVertex->Unlock();
			break;
		case BUFFER_TYPE_INDEX:
			buffer.pIndex->Unlock();
			break;
		}
	}

	LPDIRECT3DVERTEXBUFFER9 getVertexBuffer() {return buffer.pVertex;}
	LPDIRECT3DINDEXBUFFER9 getIndexBuffer() {return buffer.pIndex;}
};


class DxDrawBuffer : public IDrawBuffer
{
	friend DxBufferManager;
	std::unique_ptr<DxBuffer> m_Indices;
	std::unique_ptr<DxBuffer> m_Vertices;
	uint m_nIndices;
	uint m_nVertices;
	LPDIRECT3DDEVICE9 m_pD3DDevice;
	LPDIRECT3DVERTEXDECLARATION9 m_pVertexDecl;
public:
	DxDrawBuffer(LPDIRECT3DDEVICE9 dev, DxBuffer * ind, DxBuffer * vert, LPDIRECT3DVERTEXDECLARATION9 decl)
		: m_pD3DDevice(dev), m_Indices(ind), m_Vertices(vert), m_pVertexDecl(decl) {}

	virtual bool bind()
	{
		m_pD3DDevice->SetVertexDeclaration(m_pVertexDecl);
		m_pD3DDevice->SetIndices(m_Indices->getIndexBuffer());
		m_pD3DDevice->SetStreamSource(0, m_Vertices->getVertexBuffer(), 0, 4 * (3 + 3 + 2 + 4));
        return true;
	}
	virtual void unbind()
	{
		m_pD3DDevice->SetVertexDeclaration(nullptr);
		m_pD3DDevice->SetIndices(nullptr);
		m_pD3DDevice->SetStreamSource(0, nullptr, 0, 0);
	}
};


class DxBufferManager : public IBufferManager 
{
	LPDIRECT3DDEVICE9 m_pD3DDevice;
	LPDIRECT3DVERTEXDECLARATION9 m_pVertexDecl;
	std::map<std::string, simple_object_ref<DxDrawBuffer>> m_vBuffers;
	std::map<IObject*, IDrawBuffer*> m_vObjBuffers;

	bool createVertexDeclaration()
	{
		D3DVERTEXELEMENT9 dwDecl[] = 
		{
			//  Stream  Offset         Type                   Method                 Usage          Usage Index       
			{     0,      0,    D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0      },
			{     0,      3*4,    D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0      },
			{     0,      6*4,    D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0      },
			{     0,      8*4,    D3DDECLTYPE_FLOAT4,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT,     0      },
			D3DDECL_END()
		};

		return SUCCEEDED(m_pD3DDevice->CreateVertexDeclaration(dwDecl, &m_pVertexDecl));
	}
public:
	DxBufferManager(IEngine * pEngine) : m_pD3DDevice((LPDIRECT3DDEVICE9)pEngine->getContext()), m_pVertexDecl(nullptr) {createVertexDeclaration();}
	~DxBufferManager() {SAFE_RELEASE(m_pVertexDecl);}
    IBuffer * create(uint type, uint size)
	{
		HRESULT h;
		std::unique_ptr<DxBuffer> pRet = std::unique_ptr<DxBuffer>(new DxBuffer());
		switch (type)
		{
		case BUFFER_TYPE_VERTEX:
			h = m_pD3DDevice->CreateVertexBuffer(size, 0, 0, D3DPOOL_MANAGED, &pRet->buffer.pVertex, NULL);
			break;
		case BUFFER_TYPE_INDEX:
			h = m_pD3DDevice->CreateIndexBuffer(size*sizeof(short), 0, D3DFMT_INDEX16, D3DPOOL_MANAGED, &pRet->buffer.pIndex, NULL);
			break;
		}
		if (SUCCEEDED(h))
		{
			pRet->type = type;
			pRet->size = size;
			return pRet.release();
		}
        return nullptr;
	}

	IDrawBuffer * get(IObject * pObject)
	{
		std::map<IObject*, IDrawBuffer*>::iterator itor = m_vObjBuffers.find(pObject);
		if (itor ==  m_vObjBuffers.end())
		{
			IDrawBuffer * d = create(pObject);
            if (d == nullptr) return nullptr;
			m_vObjBuffers[pObject] = d;
			return d;
		}
		return itor->second;
	}

	IDrawBuffer * create(IObject * pObject)
	{
		if (m_vBuffers.find(pObject->getName()) == m_vBuffers.end())
		{
			uint totalIndices = 0;
			uint totalVertices = 0;
			for (int i = 0; i < pObject->getMeshCount(); ++i)
			{
				IMesh * pMesh = pObject->getMesh(i);
				totalIndices += pMesh->getIndexCount();
				totalVertices += pMesh->getVertexCount();
			}


			std::unique_ptr<IBuffer> ind =  std::unique_ptr<IBuffer>(create(BUFFER_TYPE_INDEX, totalIndices));
			std::unique_ptr<IBuffer> vert =  std::unique_ptr<IBuffer>(create(BUFFER_TYPE_VERTEX, totalVertices * 4 * (3 + 3 + 2 + 4)));

			if (!ind || !vert) return nullptr;

			float * vertPtr = (float*)vert->map();
			ushort * indexPtr = (ushort*)ind->map();
			if (!vertPtr || ! indexPtr)
			{
				vert->unmap();
				ind->unmap();
				return nullptr;
			}

			for (int i = 0; i < pObject->getMeshCount(); ++i)
			{
				IMesh * pMesh = pObject->getMesh(i);
				uint size = pMesh->getVertexCount();
				Vector3f * position = pMesh->getPosition();
				Vector3f * normal = pMesh->getNormal();
				Vector2f * texcoord = pMesh->getTexCoord();
				Vector4f * tangent = pMesh->getTangent();

				for (uint j = 0; j < size; ++j)
				{
					*vertPtr++ = position[j].x;
					*vertPtr++ = position[j].y;
					*vertPtr++ = position[j].z;

					*vertPtr++ = normal[j].x;
					*vertPtr++ = normal[j].y;
					*vertPtr++ = normal[j].z;

					*vertPtr++ = texcoord[j].x;
					*vertPtr++ = texcoord[j].y;

					*vertPtr++ = tangent[j].x;
					*vertPtr++ = tangent[j].y;
					*vertPtr++ = tangent[j].z;
					*vertPtr++ = tangent[j].w;
				}
				memcpy(indexPtr, pMesh->getIndex(), pMesh->getIndexCount() * sizeof (ushort));
				indexPtr += pMesh->getIndexCount();
			}
			vert->unmap();
			ind->unmap();

			DxDrawBuffer * db = new DxDrawBuffer(m_pD3DDevice, (DxBuffer*)ind.release(), (DxBuffer*)vert.release(), m_pVertexDecl);
			db->m_nIndices = totalIndices;
			db->m_nVertices = totalVertices;

			m_vBuffers[pObject->getName()] = db;
			return db;
		}
		else 
			return m_vBuffers[pObject->getName()].incRef();
	}

	/*
	virtual void release(IObject * pObject)
	{
		std::map<std::string, simple_object_ref<DxDrawBuffer>>::iterator itor = m_vBuffers.begin();
		while (itor !=  m_vBuffers.end())
		{
			if (itor->second.decRef() == 0)
			{
				m_vBuffers.erase(itor);
				return;
			}
			++itor;
		}
	}
	*/

	virtual void clear()
	{
		m_vObjBuffers.clear();
		m_vBuffers.clear();
	}

    virtual void onLost()
    {
        SAFE_RELEASE(m_pVertexDecl);
        std::map<std::string, simple_object_ref<DxDrawBuffer>>::iterator itor = m_vBuffers.begin();
		while (itor !=  m_vBuffers.end())
		{
            itor->second.object->m_pVertexDecl = nullptr;
			++itor;
		}
    }

    virtual void onReset()
    {
        SAFE_RELEASE(m_pVertexDecl);
        createVertexDeclaration();
        std::map<std::string, simple_object_ref<DxDrawBuffer>>::iterator itor = m_vBuffers.begin();
		while (itor !=  m_vBuffers.end())
		{
            itor->second.object->m_pVertexDecl = m_pVertexDecl;
			++itor;
		}
    }
};
}
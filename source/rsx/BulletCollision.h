#pragma once

#include "Interface.h"
#include "btBulletCollisionCommon.h"

namespace rsx {

class BulletCollision : public ICollisionSystem
{
    btBroadphaseInterface * m_pBroadphase;
    btDefaultCollisionConfiguration * m_pConfiguration;
    btCollisionDispatcher * m_pDispatcher;
	btCollisionWorld * m_pCollisionWorld;

	btTriangleIndexVertexArray *pTriangles;
	btBvhTriangleMeshShape *pBvhMesh;
public:
    BulletCollision(IEngine *)
    {
        pTriangles = nullptr;
	    pBvhMesh = nullptr;

	    m_pBroadphase = new btDbvtBroadphase();
        m_pConfiguration = new btDefaultCollisionConfiguration();
        m_pDispatcher = new btCollisionDispatcher(m_pConfiguration);
	    m_pCollisionWorld = new btCollisionWorld(m_pDispatcher, m_pBroadphase, m_pConfiguration);
    }

    ~BulletCollision()
    {
        clear();
        delete m_pBroadphase;
        delete m_pConfiguration;
        delete m_pDispatcher;
	    delete m_pCollisionWorld;
    }

    virtual bool addObject(IObject * pObject)
    {
        if (pTriangles == nullptr)
            pTriangles = new btTriangleIndexVertexArray();

        btIndexedMesh mesh;
        mesh.m_triangleIndexStride = 3*sizeof(unsigned short);
        mesh.m_vertexStride = sizeof(Vector3f);

        for (int i = 0; i < pObject->getMeshCount(); ++i)
        {
            Matrix4f world;
            IMesh *pMesh = pObject->getMesh(i);

            world = *pMesh->getWorldMatrix() * *pObject->getWorldMatrix();

            mesh.m_numTriangles = pMesh->getIndexCount() / 3;
            mesh.m_numVertices = pMesh->getVertexCount();

            mesh.m_triangleIndexBase = (const unsigned char *)pMesh->getIndex();
            mesh.m_vertexBase = (const unsigned char *)new Vector3f[mesh.m_numVertices];

            D3DXVec3TransformCoordArray((D3DXVECTOR3*)mesh.m_vertexBase, sizeof(Vector3f), (D3DXVECTOR3*)pMesh->getPosition(),
                                            sizeof(Vector3f), (D3DXMATRIX*)&world, pMesh->getVertexCount());

            pTriangles->addIndexedMesh(mesh, PHY_SHORT);
        }

        return true;
    }

    virtual bool build()
    {
        bool optimised = true;

        if (pTriangles && !pBvhMesh)
        {
            pBvhMesh = new btBvhTriangleMeshShape(pTriangles, optimised, false);
            pBvhMesh->buildOptimizedBvh();

            btCollisionObject * pObj = new btCollisionObject();

            pObj->setCollisionShape(pBvhMesh);
            btTransform t;
            t.setIdentity();
            pObj->setWorldTransform(t);
            pObj->setCollisionFlags(pObj->getCollisionFlags() | btCollisionObject::CF_STATIC_OBJECT);

            m_pCollisionWorld->addCollisionObject(pObj);

            return true;
        }
        return false;
    }

    virtual void clear()
    {
        if (pBvhMesh)
            delete pBvhMesh;
        pBvhMesh = nullptr;

        if (pTriangles)
        {
            for (int i = 0; i < pTriangles->getNumSubParts(); ++i)
            {
                if (pTriangles->getIndexedMeshArray()[i].m_vertexBase)
                    delete pTriangles->getIndexedMeshArray()[i].m_vertexBase;
            }

            delete pTriangles;
        }
        pTriangles = nullptr;

        if (m_pCollisionWorld)
        {
            for (int i = m_pCollisionWorld->getCollisionObjectArray().size() - 1; i >= 0; --i)
            {
                btCollisionObject *pObj = m_pCollisionWorld->getCollisionObjectArray()[i];
                m_pCollisionWorld->removeCollisionObject(pObj);
                delete pObj;
            }
        }
    }

    virtual bool pick(Vector3f &from, Vector3f &to, Vector3f &hit, Vector3f &normal) const
    {
        btCollisionWorld::ClosestRayResultCallback cr(btVector3(from.x, from.y, from.z), btVector3(to.x, to.y, to.z));

        m_pCollisionWorld->rayTest(btVector3(from.x, from.y, from.z), btVector3(to.x, to.y, to.z), cr);

        if (cr.hasHit())
        {
            hit = Vector3f((float*)&cr.m_hitPointWorld);
            normal = Vector3f((float*)&cr.m_hitNormalWorld);
            return true;
        }
        return false;
    }

	virtual bool checkCylinder(Vector3f &from, Vector3f &to, Vector3f &hit, Vector3f &normal, float radius, float height) const
    {
        btCylinderShapeZ cylinder(btVector3(radius, radius, height*0.5f)); 

        btVector3 vFrom(from.x, from.y, from.z), vTo(to.x, to.y, to.z);
        btCollisionWorld::ClosestConvexResultCallback cc(vFrom, vTo);
        btTransform tFrom, tTo;
        tFrom.setIdentity();
        tFrom.setOrigin(btVector3(from.x, from.y, from.z+height*0.5f));
        tTo.setIdentity();
        tTo.setOrigin(btVector3(to.x, to.y, to.z+height*0.5f));


        m_pCollisionWorld->convexSweepTest(&cylinder, tFrom, tTo, cc);

        if (cc.hasHit())
        {
            btVector3 point;
            point.setInterpolate3(vFrom, vTo, cc.m_closestHitFraction);
            hit = Vector3f((float*)&cc.m_hitPointWorld);
            to = Vector3f((float*)&point);
            normal = Vector3f((float*)&cc.m_hitNormalWorld);
            return true;
        }
        return false;
    }

	virtual bool checkSphere(Vector3f &from, Vector3f &to, Vector3f &hit, Vector3f &normal, float radius) const
    {
        btSphereShape sphere(radius); 

        btVector3 vFrom(from.x, from.y, from.z), vTo(to.x, to.y, to.z);
        btCollisionWorld::ClosestConvexResultCallback cc(vFrom, vTo);
        btTransform tFrom, tTo;
        tFrom.setIdentity();
        tFrom.setOrigin(vFrom);
        tTo.setIdentity();
        tTo.setOrigin(vTo);


        m_pCollisionWorld->convexSweepTest(&sphere, tFrom, tTo, cc);

        if (cc.hasHit())
        {
            btVector3 point;
            point.setInterpolate3(vFrom, vTo, cc.m_closestHitFraction);
            hit = Vector3f((float*)&cc.m_hitPointWorld);
            to = Vector3f((float*)&point);
            normal = Vector3f((float*)&cc.m_hitNormalWorld);
            return true;
        }
        return false;
    }
};

}
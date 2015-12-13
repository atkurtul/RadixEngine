#include "Uncollider.hpp"
#include <cmath>
#include <Portal.hpp>

namespace glPortal {

std::vector<Entity*> Uncollider::portals;

Uncollider::Uncollider(Scene &scene) : 
  scene(scene) {
}

class ContactResultHolder : public btCollisionWorld::ContactResultCallback {
public:
  bool collides = false;
  btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper*, int, int,
    const btCollisionObjectWrapper*, int, int) {
    collides = true;
    return 1;
  }
};

bool Uncollider::isPointInUncollideVolume(const btVector3 &p) {
  if (portals.size() > 0) {
    ContactResultHolder cb;
    btDefaultMotionState ms;
    ms.setWorldTransform(btTransform(btQuaternion(), p));
    btSphereShape sh(0);
    btRigidBody::btRigidBodyConstructionInfo ci(0, &ms, &sh, btVector3(0, 0, 0));
    btRigidBody rb(ci);
    for (Entity *pEnt : portals) {
      Portal &pComp = pEnt->getComponent<Portal>();
      if (pComp.open and pComp.uncollider) {
        // TODO: add AABB checks (performance sink else)
        pEnt->manager.scene.physics.world->contactPairTest(
          pComp.uncollider.get(),
          &rb,
          cb);
        if (cb.collides) {
          return true;
        }
      }
    }
  }
  return false;
}

void Uncollider::beforePhysicsStep() {
  for (Entity *pEnt : Uncollider::portals) {
    Portal &pComp = pEnt->getComponent<Portal>();
    if (pComp.open and pComp.uncollider) {
      btVector3 ext = ((btBoxShape*)(pComp.uncolliderShape.get()))
        ->getHalfExtentsWithoutMargin();
      scene.physics.world->getDebugDrawer()->drawBox(-ext, ext,
        pComp.uncollider->getWorldTransform(), btVector3(1, .5, 0));
    }
  }
}

void Uncollider::nearCallback(btBroadphasePair &collisionPair,
  btCollisionDispatcher &dispatcher, const btDispatcherInfo &dispatchInfo) {

  btCollisionObject *colObj0 = (btCollisionObject*)collisionPair.m_pProxy0->m_clientObject;
  btCollisionObject *colObj1 = (btCollisionObject*)collisionPair.m_pProxy1->m_clientObject;
  
  if (dispatcher.needsCollision(colObj0, colObj1)) {
    btCollisionObjectWrapper obj0Wrap(0, colObj0->getCollisionShape(),
      colObj0, colObj0->getWorldTransform(), -1, -1);
    btCollisionObjectWrapper obj1Wrap(0, colObj1->getCollisionShape(),
      colObj1, colObj1->getWorldTransform(), -1, -1);

    if (!collisionPair.m_algorithm) {
      collisionPair.m_algorithm = dispatcher.findAlgorithm(&obj0Wrap, &obj1Wrap);
    }

    if (collisionPair.m_algorithm) {
      btManifoldResult contactPointResult(&obj0Wrap, &obj1Wrap);
      if (dispatchInfo.m_dispatchFunc == btDispatcherInfo::DISPATCH_DISCRETE) {
        collisionPair.m_algorithm->processCollision(&obj0Wrap, &obj1Wrap, dispatchInfo, &contactPointResult);
        for (int i = 0; i < contactPointResult.getPersistentManifold()->getNumContacts(); ++i) {
          const btManifoldPoint &pt = contactPointResult.getPersistentManifold()->getContactPoint(i);
          const btVector3 &cp = pt.getPositionWorldOnA();
          if (isPointInUncollideVolume(cp)) {
            contactPointResult.getPersistentManifold()->removeContactPoint(i--);
          }
        }
      } else {
        btScalar toi = collisionPair.m_algorithm->calculateTimeOfImpact(colObj0, colObj1, dispatchInfo, &contactPointResult);
        if (dispatchInfo.m_timeOfImpact > toi)
          dispatchInfo.m_timeOfImpact = toi;
      }
    }
  }
}
  
} /* namespace glPortal */

/**
 * @author Max Bebök
 * @license TBD
 */
#include "collision/scene.h"
#include "collision/bvh.h"
#include "debug/debugDraw.h"
#include "collision/resolver.h"

namespace {
  constexpr float MIN_PENETRATION = 0.00005f;
  constexpr float FLOOR_ANGLE = 0.7f;

  constexpr bool isFloor(const Coll::IVec3 &normal) {
    return normal.v[1] > (int16_t)(0x7FFF * FLOOR_ANGLE);
  }
  constexpr bool isFloor(const T3DVec3 &normal) {
    return normal.v[1] > FLOOR_ANGLE;
  }
  constexpr bool isFloor(float normY) {
    return normY > FLOOR_ANGLE;
  }
}

Coll::CollInfo Coll::Scene::vsBCS(BCS &bcs, const T3DVec3 &velocity, float deltaTime) {
  uint64_t ticksStart = get_ticks();
  float len2 = fm_vec3_len2(&velocity);

  bool isBox = bcs.flags & BCSFlags::SHAPE_BOX;

  int steps = (int)(len2 * 0.8f);
  if(steps <= 0)steps = 1;
  if(steps > 10)steps = 10;

  auto velocityStep = velocity * (deltaTime / steps);

  Coll::CollInfo res{};
  Coll::BVHResult bvhRes{};

  for(int s=0; s<steps; ++s)
  {
    bcs.center = bcs.center + velocityStep;

    for(auto meshInst : meshes)
    {
      auto &mesh = *meshInst->mesh;

      auto bcsLocal = bcs;
      bcsLocal.center = bcsLocal.center - meshInst->pos;

      auto ticksBvhStart = get_ticks();
      bvhRes.reset();
      mesh.bvh->vsBCS(bcsLocal, bvhRes);

      ticksBVH += get_ticks() - ticksBvhStart;
      if(bvhRes.count >= Coll::MAX_RESULT_COUNT-1) {
        //debugf("BVH count: %d\n", bvhRes.count);
      }

      for(int b=0; b<bvhRes.count; ++b) {
        uint32_t t = bvhRes.triIndex[b];

        int idxA = mesh.indices[t*3];
        int idxB = mesh.indices[t*3+1];
        int idxC = mesh.indices[t*3+2];
        auto &norm = mesh.normals[t];

        Triangle tri{
          .normal = {{
           (float)norm.v[0] * (1.0f / 32767.0f),
           (float)norm.v[1] * (1.0f / 32767.0f),
           (float)norm.v[2] * (1.0f / 32767.0f)
          }},
          .v = {&mesh.verts[idxA], &mesh.verts[idxB], &mesh.verts[idxC]}
        };

        auto collInfo = isBox
          ? mesh.vsBox(bcsLocal, tri)
          : mesh.vsSphere(bcsLocal, tri);

        if(collInfo.collCount)
        {
          float penLen2 = t3d_vec3_len2(&collInfo.penetration);
          if(penLen2 < MIN_PENETRATION)continue;

          ++res.collCount;
          res.penetration = res.penetration + collInfo.penetration;

          bool hitFloor = isFloor(collInfo.floorWallAngle.y);
          bcs.hitTriTypes |= hitFloor ? TriType::FLOOR : TriType::WALL;
          if(hitFloor) {
            res.floorWallAngle.y = collInfo.floorWallAngle.y;
          } else {
            res.floorWallAngle.x = collInfo.floorWallAngle.x;
            res.floorWallAngle.z = collInfo.floorWallAngle.z;
          }

          bcs.center = bcs.center - collInfo.penetration;
          bcsLocal.center = bcs.center - meshInst->pos;
        }
      } // BVH res
    } // meshes
  } // steps

  ticks += get_ticks() - ticksStart;
  return res;
}

void Coll::Scene::update(float deltaTime)
{
  for(auto sp : collBCS) {
    sp->hitTriTypes = 0;
  }

  for(uint32_t s=0; s < collBCS.size(); ++s) {
    auto &bcsA = collBCS[s];

    // Static/Triangle mesh collision
    bool checkColl = bcsA->maskRead & Mask::TRI_MESH;

    // first check for void-sphere, those cut-out a section of geometry...
    if(checkColl) {
      for(uint32_t v=0; v<VOID_SPHERE_COUNT; ++v) {
        float radSum = voidSpheres[v].getRadius();
        if(radSum <= 0)continue;
        auto dist2 = t3d_vec3_distance2(voidSpheres[v].center, bcsA->center);
        if(dist2 < radSum * radSum) {
          checkColl = false;
          bcsA->center += bcsA->velocity * deltaTime;
          break;
        }
      }
    }

    // ...if we are not inside one, check actual mesh data
    if(checkColl) {
      auto res = vsBCS(*bcsA, bcsA->velocity, deltaTime);
      if(res.collCount) {
        bool hitFloor = bcsA->hitTriTypes & TriType::FLOOR;
        if(bcsA->flags & BCSFlags::BOUNCY) {
          //T3DVec3 norm;
          //t3d_vec3_norm(res.floorWallAngle);
          bcsA->velocity = bcsA->velocity - res.floorWallAngle * 2.0f * t3d_vec3_dot(bcsA->velocity, res.floorWallAngle);
          bcsA->velocity *= 0.8f;
        } else if(hitFloor) {
          if(bcsA->velocity.y < 0) {
            bcsA->velocity.v[1] = 0.0f;
          } else {
            bcsA->hitTriTypes &= ~TriType::FLOOR;
          }
        }
      }
    }

    // Dynamic Colliders
    for(uint32_t s2=s+1; s2 < collBCS.size(); ++s2)
    {
      bool maskMatchA = bcsA->maskRead & collBCS[s2]->maskWrite;
      bool maskMatchB = collBCS[s2]->maskRead & bcsA->maskWrite;
      if(!maskMatchA && !maskMatchB)continue;

      auto bcsB = collBCS[s2];

      bool isBoxA = bcsA->flags & BCSFlags::SHAPE_BOX;
      bool isBoxB = bcsB->flags & BCSFlags::SHAPE_BOX;

      bool isColl = false;

      if(!isBoxA && !isBoxB) {
        isColl = sphereVsSphere(*bcsA, *bcsB);
      } else if(isBoxA && !isBoxB) {
        isColl = sphereVsBox(*bcsB, *bcsA);
      } else if(!isBoxA && isBoxB) {
        isColl = sphereVsBox(*bcsA, *bcsB);
      } else {
        isColl = boxVsBox(*bcsA, *bcsB);
      }

      if(isColl) {
        if(bcsA->callback && maskMatchA)bcsA->callback(*bcsB);
        if(bcsB->callback && maskMatchB)bcsB->callback(*bcsA);
      }
    }
  }
}

Coll::RaycastRes Coll::Scene::raycastFloor(const T3DVec3 &pos) {
  ++raycastCount;
  Coll::RaycastRes res{
    .hitPos = T3DVec3{0.0f, 0.0f, 0.0f},
    .normal = T3DVec3{0.0f, 0.0f, 0.0f},
  };

  float highestFloor = -99999.0f;
  for(auto meshInst : meshes)
  {
    auto &mesh = *meshInst->mesh;
    auto posLocal = pos - meshInst->pos;
    Coll::IVec3 posInt = {
      .v = {
        (int16_t)(posLocal.v[0] * 64.0f),
        (int16_t)(posLocal.v[1] * 64.0f),
        (int16_t)(posLocal.v[2] * 64.0f)
      }
    };

    Coll::BVHResult bvhRes{};
    mesh.bvh->raycastFloor(posInt, bvhRes);

    for(int b=0; b<bvhRes.count; ++b)
    {
    //for(uint32_t b=0; b<mesh.triCount; ++b) {
      uint32_t t = bvhRes.triIndex[b];
      //uint32_t t = b;
      if(!isFloor(mesh.normals[t]))continue;

      int idxA = mesh.indices[t*3];
      int idxB = mesh.indices[t*3+1];
      int idxC = mesh.indices[t*3+2];
      auto &norm = mesh.normals[t];

      Triangle tri{
        .normal = {{
         (float)norm.v[0] / 32767.0f,
         (float)norm.v[1] / 32767.0f,
         (float)norm.v[2] / 32767.0f
        }},
        .v = {&mesh.verts[idxA], &mesh.verts[idxB], &mesh.verts[idxC]}
      };

      auto collInfo = mesh.vsFloorRay(posLocal, tri);
      if(collInfo.hasResult() && collInfo.hitPos.v[1] > highestFloor)
      {
        res.hitPos = collInfo.hitPos + meshInst->pos;
        res.normal = collInfo.normal;
        highestFloor = collInfo.hitPos.v[1];
      }
    }
  }

  // check dynamic colliders (boxes only)
  for(auto sphere : collBCS) {
    if(sphere->flags & BCSFlags::SHAPE_BOX) {
      // inside 2D rect (top-down)
      if(pos.v[0] >= sphere->getMinAABB().v[0] && pos.v[0] <= sphere->getMaxAABB().v[0] &&
         pos.v[2] >= sphere->getMinAABB().v[2] && pos.v[2] <= sphere->getMaxAABB().v[2])
      {
        float localHeight = sphere->center.v[1] + sphere->halfExtend.v[1];
        if(localHeight > highestFloor && pos.y > localHeight) {
          highestFloor = localHeight;
          res.hitPos = {pos.v[0], localHeight, pos.v[2]};
          res.normal = {0.0f, 1.0f, 0.0f};
        }
      }
    }
  }

  if (res.hasResult()) {
    for(uint32_t v=0; v<VOID_SPHERE_COUNT; ++v) {
      float radSum = voidSpheres[v].halfExtend.y;
      if(radSum <= 0)continue;
      auto dist2 = t3d_vec3_distance2(voidSpheres[v].center, res.hitPos);
      if(dist2 < radSum * radSum) {
        res.normal = {0.0f, 0.0f, 0.0f};
        return res;
      }
    }
  }

  return res;
}

void Coll::Scene::debugDraw(bool showMesh, bool showSpheres)
{
  if(showMesh) {
    for(const auto &meshInst : meshes) {
      auto &mesh = *meshInst->mesh;
      for(uint32_t t=0; t<mesh.triCount; ++t) {
        int idxA = mesh.indices[t*3];
        int idxB = mesh.indices[t*3+1];
        int idxC = mesh.indices[t*3+2];
        auto v0 = (mesh.verts[idxA] + meshInst->pos) * 16.0f;
        auto v1 = (mesh.verts[idxB] + meshInst->pos) * 16.0f;
        auto v2 = (mesh.verts[idxC] + meshInst->pos) * 16.0f;

        if(mesh.normals[t].v[2] < 0)continue;
        auto color = isFloor(mesh.normals[t])
          ? color_t{0x00, 0xAA, 0xEE, 0xFF}
          : color_t{0x00, 0xEE, 0x42, 0xFF};

        Debug::drawLine(v0, v1, color);
        Debug::drawLine(v1, v2, color);
        Debug::drawLine(v2, v0, color);
      }
    }
  }

  if(showSpheres) {
    for(const auto &sphere : collBCS) {
      if(sphere->maskWrite == 0)continue;

      color_t col{0xFF, 0xFF, 0x00, 0xFF};
      if(sphere->maskWrite & Mask::SOLID) {
        col = color_t{0x44, 0x44, 0x44, 0xFF};
        bool isFloor = sphere->hitTriTypes & TriType::FLOOR;
        bool isWall = sphere->hitTriTypes & TriType::WALL;
        if(isFloor)col.b = 0xEE;
        if(isWall)col.r = 0xEE;
      }

      if(sphere->flags & BCSFlags::SHAPE_BOX) {
        Debug::drawAABB(sphere->center * 16.0f, sphere->halfExtend * 16.0f, col);
      } else {
        Debug::drawSphere(sphere->center * 16.0f, sphere->halfExtend.y * 16.0f, col);
      }
    }

    for(uint32_t v=0; v<VOID_SPHERE_COUNT; ++v) {
      if(voidSpheres[v].halfExtend.y <= 0)continue;
      Debug::drawSphere(voidSpheres[v].center * 16.0f, voidSpheres[v].halfExtend.y * 16.0f, {0x00, 0x00, 0x00, 0xFF});
    }
  }
}

bool Coll::Scene::isInVoid(const T3DVec3 &pos) const {
  for(uint32_t v=0; v<VOID_SPHERE_COUNT; ++v) {
    float radSum = voidSpheres[v].halfExtend.y;
    if(radSum <= 0)continue;
    auto dist2 = t3d_vec3_distance2(voidSpheres[v].center, pos);
    if(dist2 < radSum * radSum) {
      return true;
    }
  }
  return false;
}

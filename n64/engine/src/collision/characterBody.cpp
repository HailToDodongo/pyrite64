/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "collision/characterBody.h"
#include "collision/capsuleSweep.h"
#include "collision/gfxScale.h"
#include "scene/sceneManager.h"
#include "scene/scene.h"
#include "scene/object.h"
#include "scene/components/collMesh.h"
#include "debug/debugDraw.h"

#include <cmath>

using namespace P64::Coll;

CharacterBody::CharacterBody(Object *owner_)
  : owner(owner_)
{
  inputVelocity = VEC3_ZERO;
  velocity = VEC3_ZERO;
  onFloor = false;
  onSteepSurface = false;
  refreshCache();
  contactNormal = normUp;
}

void CharacterBody::configure(const Settings& s)
{
  settings = s;
  refreshCache();
  contactNormal = normUp;
}

void CharacterBody::refreshCache()
{
  normUp = vec3NormalizeOrFallback(settings.up, VEC3_UP);

  // Rotate centerOffset from its authored +Y-up space to the current up.
  fm_quat_t q;
  if(normUp.y < -0.9999f) {
    q = {1.0f, 0.0f, 0.0f, 0.0f};
  } else {
    q = {normUp.z, 0.0f, -normUp.x, 1.0f + normUp.y};
    fm_quat_norm(&q, &q);
  }
  cachedCenterOffset = q * settings.centerOffset;

  halfHeight = fmaxf(settings.height * 0.5f, settings.radius);
  innerHalfHeight = halfHeight - settings.radius;
  walkCos = fm_cosf(settings.floorMaxAngle);
}

void CharacterBody::setUp(const fm_vec3_t& newUp)
{
  const fm_vec3_t up = vec3NormalizeOrFallback(newUp, VEC3_UP);
  if(fm_vec3_dot(&up, &normUp) > 0.9999f) return;
  settings.up = up;
  refreshCache();
}

void CharacterBody::setCenterOffset(const fm_vec3_t& offset)
{
  settings.centerOffset = offset;
  refreshCache();
}

fm_vec3_t CharacterBody::capsuleCenter() const
{
  return owner->pos * getInvGfxScale() + cachedCenterOffset;
}

fm_vec3_t CharacterBody::getFootPos() const
{
  return (capsuleCenter() - normUp * halfHeight) * getGfxScale();
}

float CharacterBody::extentAlong(const fm_vec3_t& dir) const
{
  const float alongUp = fabsf(fm_vec3_dot(&dir, &normUp));
  return alongUp * innerHalfHeight + settings.radius;
}

void CharacterBody::teleport(const fm_vec3_t& ownerPos, bool resetForces)
{
  owner->pos = ownerPos;
  if(resetForces) {
    velocity       = VEC3_ZERO;
    inputVelocity  = VEC3_ZERO;
    onFloor        = 0;
    onSteepSurface = 0;
    probeFoundFloor = 0;
    contactNormal  = normUp;
  }
}

void CharacterBody::moveAndSlide(float deltaTime)
{
  CollisionScene& scene = SceneManager::getCurrent().getCollision();
  const float gfxScale = getGfxScale();
  const bool wasOnFloor = onFloor;
  const bool wasOnSteepSurface = onSteepSurface;
  const bool wasProbeFloor = probeFoundFloor;
  const fm_vec3_t& up = normUp;
  onSteepSurface = 0;
  probeFoundFloor = 0;
  floorObjId = 0;
  movedByFloor = 0;

  // Track transform of object you are standing on (tracked at foot-position).
  // This is applied before anything else
  if(settings.followFloor) {
    const fm_vec3_t foot = capsuleCenter() - up * halfHeight;
    fm_vec3_t carryDiff = floorAttach.update(foot);
    owner->pos = owner->pos - carryDiff * gfxScale;
    // The floor dragged us this frame if the contact point moved (translation/rotation).
    if(fm_vec3_len2(&carryDiff) > 1e-8f) movedByFloor = 1;
  }

  // Capsule geometry in physics units
  const float r   = settings.radius;
  const float ih  = innerHalfHeight;

  // Shorten the physics capsule from the bottom by stepHeight so that stair
  // risers below that height are invisible to the sweep. The floor snap corrects
  // the vertical position afterward.
  // Clamp stepH to floorSnapDistance: when the swept loop hits a floor, it
  // places the capsule stepH below the correct position; the floor snap must
  // compensate, which requires stepH <= floorSnapDistance. Also clamp to ih
  // so ih_phys stays >= 0.
  const float stepH   = fminf(fminf(settings.stepHeight, ih), settings.floorSnapDistance);
  const float ih_phys = ih - stepH;

  // Build per-frame velocity
  auto horiz = inputVelocity - up * fm_vec3_dot(&inputVelocity, &up);
  float vAlongUp = fm_vec3_dot(&velocity, &up);
  if(wasOnFloor && !wasOnSteepSurface && wasProbeFloor) {
    // handle cases where the up-vector changes between frames and causes a bit of noise in the ground detection
    constexpr float UP_IMPULSE_THRESHOLD = 0.5f;
    vAlongUp = (vAlongUp > UP_IMPULSE_THRESHOLD) ? vAlongUp : 0.0f;
  } else {
    vAlongUp -= settings.gravity * deltaTime;
  }
  if(vAlongUp < -settings.maxFallSpeed) vAlongUp = -settings.maxFallSpeed;
  velocity = horiz + up * vAlongUp;

  // Reshape displacement for slope-following when grounded
  fm_vec3_t stepVel = velocity;
  if(wasOnFloor && !wasOnSteepSurface && wasProbeFloor) {
    fm_vec3_t along = horiz - vec3ProjectOntoUnit(horiz, contactNormal);
    float horizLen2 = fm_vec3_len2(&horiz);
    float alongLen2 = fm_vec3_len2(&along);
    if(horizLen2 > FM_EPSILON * FM_EPSILON && alongLen2 > FM_EPSILON * FM_EPSILON) {
      along = along * sqrtf(horizLen2 / alongLen2);
    }
    stepVel = along + up * vAlongUp;
  }

  // Swept slide loop:
  bool sweptWalkableFloor = false;
  fm_vec3_t sweptFloorNormal = up;
  fm_vec3_t displacement = stepVel * deltaTime;
  fm_vec3_t prevHitNormal = VEC3_ZERO;
  bool      hasPrevHit    = false;

  for(uint8_t iter = 0; iter < settings.maxSlides; ++iter) {
    float dispLen2 = fm_vec3_len2(&displacement);
    if(dispLen2 < FM_EPSILON * FM_EPSILON) break;

    CapsuleSweepHit hit;
    bool didHit = scene.capsuleSweep(
      capsuleCenter(), up, r, ih_phys,
      displacement,
      settings.collTypes, settings.readMask,
      hit, owner
    );

    if(!didHit) {
      owner->pos = owner->pos + displacement * gfxScale;
      break;
    }

    float dispLen = sqrtf(dispLen2);

    // If the capsule is already overlapping at t==0, push out first, then re-try the full step.
    if(hit.t <= 0.0f) {
      constexpr float MAX_DEPEN = 0.05f; // metres per iteration
      float pushOut = fminf(hit.depth + FM_EPSILON, MAX_DEPEN);
      fm_vec3_t pushDir = vec3AssumeNormalized(hit.normal, up);
      const float pushUp = fm_vec3_dot(&pushDir, &up);
      if(pushUp > FM_EPSILON) {
        pushDir = pushDir - up * pushUp;
        const float len2 = fm_vec3_len2(&pushDir);
        if(len2 < FM_EPSILON * FM_EPSILON) { continue; }
        pushDir = pushDir * (1.0f / sqrtf(len2));
      }
      // Don't push into the previous surface, constraining the push direction
      // avoids ping-pong oscillation between two walls of a corner.
      if(hasPrevHit) {
        const float intoPrev = fm_vec3_dot(&pushDir, &prevHitNormal);
        if(intoPrev < -FM_EPSILON) {
          pushDir = pushDir - prevHitNormal * intoPrev;
          const float len2 = fm_vec3_len2(&pushDir);
          if(len2 < FM_EPSILON * FM_EPSILON) { continue; }
          pushDir = pushDir * (1.0f / sqrtf(len2));
        }
      }
      owner->pos = owner->pos + pushDir * (pushOut * gfxScale);
      // Strip the into-wall component from displacement so re-tries don't re-enter.
      const float dispInto = fminf(0.0f, fm_vec3_dot(&displacement, &pushDir));
      displacement = displacement - pushDir * dispInto;
      // Crease fix for t=0: when stripping this surface would press displacement back
      // into the previous one (V-corner trap), project onto the crease of both planes.
      if(hasPrevHit && fm_vec3_dot(&displacement, &prevHitNormal) < -FM_EPSILON) {
        fm_vec3_t crease;
        fm_vec3_cross(&crease, &pushDir, &prevHitNormal);
        const float creaseLen2 = fm_vec3_len2(&crease);
        if(creaseLen2 > FM_EPSILON * FM_EPSILON) {
          crease = crease * (1.0f / sqrtf(creaseLen2));
          displacement = crease * fm_vec3_dot(&displacement, &crease);
        } else {
          displacement = VEC3_ZERO;
        }
      }
      // When nothing was stripped from displacement (it was already parallel
      // to this surface) and it doesn't point into any previous surface, apply
      // it now. This stops gravity accumulation from being silently discarded
      // in sharp corners where every sweep returns t=0.
      if(fabsf(dispInto) <= FM_EPSILON) {
        if(!hasPrevHit || fm_vec3_dot(&displacement, &prevHitNormal) >= -FM_EPSILON) {
          owner->pos = owner->pos + displacement * gfxScale;
          displacement = VEC3_ZERO;
        }
      }
      prevHitNormal = pushDir;
      hasPrevHit    = true;
      continue;
    }

    // Advance to the contact point
    float allowed = hit.t * dispLen;
    owner->pos = owner->pos + displacement / dispLen * (allowed * gfxScale);

    fm_vec3_t normal = vec3AssumeNormalized(hit.normal, up);
    fm_vec3_t remaining = displacement / dispLen * (dispLen - allowed);
    fm_vec3_t slide = remaining - vec3ProjectOntoUnit(remaining, normal);

    const float normalUp = fm_vec3_dot(&normal, &up);
    const float dirUp = fm_vec3_dot(&displacement, &up) / dispLen;
    if(normalUp >= walkCos && dirUp < -FM_EPSILON) {
      sweptWalkableFloor = true;
      sweptFloorNormal = normal;
    }

    // Prevent sliding along a steep wall while grounded.
    // Strip the vertical component from the slide so the character doesn't
    // climb or dive into the wall, and damp the toward-wall velocity to avoid
    // residual energy accumulating frame-to-frame (causes jitter in corners).
    if(wasOnFloor && !wasOnSteepSurface && normalUp < walkCos) {
      const float slideUp = fm_vec3_dot(&slide, &up);
      slide = slide - up * slideUp;
      const float velInto = fm_vec3_dot(&velocity, &normal);
      if(velInto > 0.0f) {
        velocity = velocity - normal * velInto;
      }
    }

    // Crease fix: when the slide points back into the previously-hit surface the
    // character is trapped (e.g. a V-corner). Project onto the intersection line
    // of both planes so motion follows the crease (straight down for vertical walls).
    if(hasPrevHit && fm_vec3_dot(&slide, &prevHitNormal) < -FM_EPSILON) {
      fm_vec3_t crease;
      fm_vec3_cross(&crease, &normal, &prevHitNormal);
      const float creaseLen2 = fm_vec3_len2(&crease);
      if(creaseLen2 > FM_EPSILON * FM_EPSILON) {
        crease = crease * (1.0f / sqrtf(creaseLen2));
        slide = crease * fm_vec3_dot(&slide, &crease);
      } else {
        slide = VEC3_ZERO;
      }
    }
    prevHitNormal = normal;
    hasPrevHit    = true;

    displacement = slide;

    // Cancel upward velocity on ceiling hit
    const float velUp = fm_vec3_dot(&velocity, &up);
    if(normalUp < -0.1f && velUp > 0.0f) {
      velocity = velocity - up * velUp;
    }
  }

  // Fire a zero-length capsule sweep to find any remaining lateral overlaps,
  // then push out along the contact normal. This resolves initial penetrations
  // that the swept loop could not observe (e.g. the capsule side already inside
  // a wall while moving parallel to it).
  {
    constexpr fm_vec3_t depProbe = VEC3_ZERO; // zero displacement → overlap query

    fm_vec3_t prevDepN = VEC3_ZERO;
    bool hasPrevDep = false;

    // Run a few iterations to clear compound overlaps
    for(int di = 0; di < 3; ++di) {
      CapsuleSweepHit depHit;
      bool hasOverlap = scene.capsuleSweep(
        capsuleCenter(), up, r, ih_phys,
        depProbe,
        settings.collTypes, settings.readMask,
        depHit, owner
      );
      if(!hasOverlap || depHit.depth <= FM_EPSILON) break;

      const fm_vec3_t origN = vec3AssumeNormalized(depHit.normal, up);
      const float normalUp = fm_vec3_dot(&origN, &up);
      // Skip floors — handled by floor snap below
      if(normalUp > FM_EPSILON) break;

      fm_vec3_t pushDir = origN;
      // Don't push into the previously resolved surface — in obtuse corners
      // pushing perpendicular to one wall drives the capsule into the other.
      if(hasPrevDep) {
        const float intoPrev = fm_vec3_dot(&pushDir, &prevDepN);
        if(intoPrev < -FM_EPSILON) {
          pushDir = pushDir - prevDepN * intoPrev;
          const float len2 = fm_vec3_len2(&pushDir);
          if(len2 < FM_EPSILON * FM_EPSILON) { break; }
          pushDir = pushDir * (1.0f / sqrtf(len2));
        }
      }

      float pushOut = depHit.depth;
      // When the push direction was constrained away from the original normal,
      // scale up to compensate — depth is measured along origN, not pushDir.
      {
        const float efficiency = fm_vec3_dot(&pushDir, &origN);
        if(efficiency > FM_EPSILON) {
          pushOut = fminf(pushOut / efficiency, 0.1f);
        }
      }
      owner->pos = owner->pos + pushDir * (pushOut * gfxScale);
      prevDepN = pushDir;
      hasPrevDep = true;
    }
  }

  // ── Floor probe + un-sink + snap ──────────────────────────────────────────
  onFloor = sweptWalkableFloor;
  if(sweptWalkableFloor) {
    contactNormal = sweptFloorNormal;
  }
  {
    const float maxSnap = settings.floorSnapDistance;
    const float effectiveReach = halfHeight;
    const fm_vec3_t origin = capsuleCenter();
    const float probeDist = effectiveReach + maxSnap;

    Raycast probe = Raycast::create(
      origin, -up, probeDist,
      settings.collTypes, false, settings.readMask
    );
    RaycastHit hit;
    const bool floorHit = scene.raycast(probe, hit) && hit.didHit;
    if(floorHit) {
      const float clearance = hit.distance - effectiveReach;
      const float hitNormalUp = fm_vec3_dot(&hit.normal, &up);
      const bool supportSurface = hitNormalUp > FM_EPSILON;

      bool inSnapRange = clearance <= maxSnap && clearance >= -maxSnap;

      // Remember which object we are grounded on (for platforms reacting to us).
      if(inSnapRange && supportSurface) floorObjId = hit.hitObjectId;

      float effectiveClearance = clearance;
      if(inSnapRange && supportSurface && clearance < 0.0f) {
        const float velUp = fm_vec3_dot(&velocity, &up);
        // Lift when grounded (stair stepping) or falling (landing correction).
        const bool shouldLift = (wasOnFloor && !wasOnSteepSurface) || velUp < 0.0f;
        if(shouldLift) {
          owner->pos = owner->pos + up * (-clearance * gfxScale);
          effectiveClearance = 0;
          // Only zero falling velocity for walkable surfaces; steep surfaces must let gravity accumulate.
          if(velUp < 0.0f && hitNormalUp >= walkCos) velocity = velocity - up * velUp;
        }
      }

      if(inSnapRange && hitNormalUp >= walkCos) {
        const float velUp = fm_vec3_dot(&velocity, &up);

        // more handling if the up-vector changes across frames
        constexpr float STICK_VEL_THRESHOLD = 0.5f;
        const float stickVelUp = (fabsf(velUp) < STICK_VEL_THRESHOLD) ? 0.0f : velUp;

        const bool stick  = wasOnFloor && stickVelUp <= 0.0f;
        const bool landed = !wasOnFloor && stickVelUp <= 0.0f && effectiveClearance == 0;

        if(stick) {
          const float delta = effectiveClearance;
          if(fabsf(delta) > 1e-5f) {
            owner->pos = owner->pos - up * (delta * gfxScale);
          }
        }
        if(stick || landed) {
          onFloor = true;
          probeFoundFloor = 1;
          contactNormal = vec3AssumeNormalized(hit.normal, up);
          velocity = velocity - up * fm_vec3_dot(&velocity, &up);

          if(settings.followFloor && hit.hitObjectId != 0) {
            auto* floorObj = SceneManager::getCurrent().getObjectById(hit.hitObjectId);
            auto* floorMesh = floorObj ? floorObj->getComponent<Comp::CollMesh>() : nullptr;
            if(floorMesh && floorMesh->meshCollider) {
              floorAttach.setReference(floorMesh->meshCollider);
            }
          }
        }
      } else if(inSnapRange && supportSurface) {
        onFloor = true;
        onSteepSurface = true;
        contactNormal = vec3AssumeNormalized(hit.normal, up);
      }
    }
  }
}

void CharacterBody::debugDraw() const
{
  const float gfxScale = getGfxScale();
  const fm_vec3_t& up = normUp;
  const float r    = settings.radius;
  const float hh   = halfHeight;
  const float ih   = innerHalfHeight;
  const float stepH   = fminf(fminf(settings.stepHeight, ih), settings.floorSnapDistance);
  const float ih_phys = ih - stepH;

  const fm_vec3_t center = capsuleCenter();

  // Dim outline of the full logical capsule
  Debug::drawCapsule(center * gfxScale, r * gfxScale, ih * gfxScale, QUAT_IDENTITY,
    color_t{0x40, 0x40, 0x40, 0xFF});

  // Physics capsule (what actually collides) — green on floor, orange on steep, white airborne
  color_t physColor = {0xFF, 0xFF, 0xFF, 0xFF};
  if(onFloor) {
    physColor = onSteepSurface
      ? color_t{0xFF, 0xA0, 0x00, 0xFF}
      : color_t{0x00, 0xFF, 0x40, 0xFF};
  }
  Debug::drawCapsule(center * gfxScale, r * gfxScale, ih_phys * gfxScale, QUAT_IDENTITY, physColor);

  // Step zone indicator: horizontal line at the physics capsule bottom
  if(stepH > 0.0f) {
    const fm_vec3_t physBottom = center - up * (ih_phys + r);
    const fm_vec3_t side = fm_vec3_t{{up.y, up.z, up.x}} * r; // arbitrary perpendicular
    Debug::drawLine((physBottom - side) * gfxScale, (physBottom + side) * gfxScale,
      color_t{0xFF, 0xFF, 0x00, 0xFF});
  }

  // Floor snap probe: line from capsule center showing full probe reach
  const float probeDist = hh + settings.floorSnapDistance;
  const fm_vec3_t probeEnd = center - up * probeDist;
  Debug::drawLine(center * gfxScale, probeEnd * gfxScale, color_t{0x80, 0x80, 0xFF, 0xFF});

  // Contact normal from capsule bottom (cyan, only when on floor)
  if(onFloor) {
    const fm_vec3_t bottom = center - up * hh;
    Debug::drawLine(bottom * gfxScale, (bottom + contactNormal * r) * gfxScale,
      color_t{0x00, 0xFF, 0xFF, 0xFF});
  }
}

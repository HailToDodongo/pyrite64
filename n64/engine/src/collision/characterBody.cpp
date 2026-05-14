/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "collision/characterBody.h"
#include "collision/capsuleSweep.h"
#include "collision/gfxScale.h"
#include "scene/object.h"
#include "debug/debugDraw.h"

#include <cmath>

using namespace P64::Coll;

CharacterBody::CharacterBody(Object &owner_)
  : owner(&owner_)
{
  inputVelocity = VEC3_ZERO;
  velocity = VEC3_ZERO;
  contactNormal = vec3NormalizeOrFallback(settings.up, VEC3_UP);
  onFloor = false;
  onSteepSurface = false;
}

fm_vec3_t CharacterBody::capsuleCenter() const
{
  return owner->pos * getInvGfxScale() + settings.centerOffset;
}

float CharacterBody::extentAlong(const fm_vec3_t& dir) const
{
  const fm_vec3_t up = vec3NormalizeOrFallback(settings.up, VEC3_UP);
  const float r = settings.radius;
  const float halfHeight = fmaxf(settings.height * 0.5f, r);
  const float alongUp = fabsf(fm_vec3_dot(&dir, &up));
  return alongUp * (halfHeight - r) + r;
}

void CharacterBody::moveAndSlide(float deltaTime, CollisionScene& scene)
{
  const float gfxScale = getGfxScale();
  const float walkCos = fm_cosf(settings.floorMaxAngle);
  const bool wasOnFloor = onFloor;
  const bool wasOnSteepSurface = onSteepSurface;
  const fm_vec3_t up = vec3NormalizeOrFallback(settings.up, VEC3_UP);
  onSteepSurface = 0;
  snappedFloor = 0;

  // Capsule geometry in physics units
  const float r   = settings.radius;
  const float hh  = fmaxf(settings.height * 0.5f, r); // total half-height
  const float ih  = hh - r;                            // inner (cylindrical) half-height

  // Shorten the physics capsule from the bottom by stepHeight so that stair
  // risers below that height are invisible to the sweep. The floor snap corrects
  // the vertical position afterward.
  // Clamp stepH to floorSnapDistance: when the swept loop hits a floor, it
  // places the capsule stepH below the correct position; the floor snap must
  // compensate, which requires stepH <= floorSnapDistance. Also clamp to ih
  // so ih_phys stays >= 0.
  const float stepH   = fminf(fminf(settings.stepHeight, ih), settings.floorSnapDistance);
  const float ih_phys = ih - stepH;

  // Build per-frame velocity (same logic as before)
  auto horiz = inputVelocity - up * fm_vec3_dot(&inputVelocity, &up);
  float vAlongUp = fm_vec3_dot(&velocity, &up);
  if(wasOnFloor && !wasOnSteepSurface) {
    vAlongUp = fmaxf(vAlongUp, 0.0f);
  } else {
    vAlongUp -= settings.gravity * deltaTime;
  }
  if(vAlongUp < -settings.maxFallSpeed) vAlongUp = -settings.maxFallSpeed;
  velocity = horiz + up * vAlongUp;

  // Reshape displacement for slope-following when grounded
  fm_vec3_t stepVel = velocity;
  if(wasOnFloor && !wasOnSteepSurface) {
    fm_vec3_t along = horiz - vec3Project(horiz, contactNormal);
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

  for(uint8_t iter = 0; iter < settings.maxSlides; ++iter) {
    float dispLen2 = fm_vec3_len2(&displacement);
    if(dispLen2 < FM_EPSILON * FM_EPSILON) break;

    CapsuleSweepHit hit;
    bool didHit = scene.capsuleSweep(
      capsuleCenter(), up, r, ih_phys,
      displacement,
      settings.collTypes, settings.readMask,
      hit
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
      owner->pos = owner->pos + hit.normal * (pushOut * gfxScale);
      // don't consume displacement, next iteration handles it.
      continue;
    }

    // Advance to the contact point
    float allowed = hit.t * dispLen;
    owner->pos = owner->pos + displacement / dispLen * (allowed * gfxScale);

    fm_vec3_t normal = vec3NormalizeOrFallback(hit.normal, up);
    fm_vec3_t remaining = displacement / dispLen * (dispLen - allowed);
    fm_vec3_t slide = remaining - vec3Project(remaining, normal);

    const float normalUp = fm_vec3_dot(&normal, &up);
    const float dirUp = fm_vec3_dot(&displacement, &up) / dispLen;
    if(normalUp >= walkCos && dirUp <= FM_EPSILON) {
      sweptWalkableFloor = true;
      sweptFloorNormal = normal;
    }

    // Prevent sliding downward along a steep wall while grounded
    if(wasOnFloor && !wasOnSteepSurface && normalUp < walkCos) {
      const float slideUp = fm_vec3_dot(&slide, &up);
      if(slideUp < 0.0f) slide = slide - up * slideUp;
    }
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

    // Run a few iterations to clear compound overlaps
    for(int di = 0; di < 3; ++di) {
      CapsuleSweepHit depHit;
      bool hasOverlap = scene.capsuleSweep(
        capsuleCenter(), up, r, ih_phys,
        depProbe,
        settings.collTypes, settings.readMask,
        depHit
      );
      if(!hasOverlap || depHit.depth <= FM_EPSILON) break;

      const fm_vec3_t normal = vec3NormalizeOrFallback(depHit.normal, up);
      const float normalUp = fm_vec3_dot(&normal, &up);
      // Skip floors — handled by floor snap below
      if(normalUp > FM_EPSILON) break;

      float pushOut = depHit.depth;
      owner->pos = owner->pos + normal * (pushOut * gfxScale);
    }
  }

  // ── Floor probe + un-sink + snap ──────────────────────────────────────────
  onFloor = sweptWalkableFloor;
  if(sweptWalkableFloor) {
    contactNormal = sweptFloorNormal;
    const float velUp = fm_vec3_dot(&velocity, &up);
    if(velUp < 0.0f) velocity = velocity - up * velUp;
  }
  {
    const float halfHeight = fmaxf(settings.height * 0.5f, settings.radius);
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

      float effectiveClearance = clearance;
      if(inSnapRange && supportSurface && clearance < -0) {
        const float lift = -clearance;
        owner->pos = owner->pos + up * (lift * gfxScale);
        effectiveClearance = 0;
        const float velUp = fm_vec3_dot(&velocity, &up);
        if(velUp < 0.0f) velocity = velocity - up * velUp;
      }

      if(inSnapRange && hitNormalUp >= walkCos) {
        const float velUp = fm_vec3_dot(&velocity, &up);

        const bool stick  = wasOnFloor && velUp <= 0.0f;
        const bool landed = !wasOnFloor && velUp <= 0.0f && effectiveClearance == 0;

        if(stick) {
          const float delta = effectiveClearance;
          if(fabsf(delta) > 1e-5f) {
            owner->pos = owner->pos - up * (delta * gfxScale);
          }
        }
        if(stick || landed) {
          onFloor = true;
          contactNormal = vec3NormalizeOrFallback(hit.normal, up);
          velocity = velocity - up * fm_vec3_dot(&velocity, &up);
        }
      } else if(inSnapRange && supportSurface) {
        onFloor = true;
        onSteepSurface = true;
        contactNormal = vec3NormalizeOrFallback(hit.normal, up);
      }
    }
  }
}

void CharacterBody::debugDraw() const
{
  const float gfxScale = getGfxScale();
  const fm_vec3_t up = vec3NormalizeOrFallback(settings.up, VEC3_UP);
  const float r    = settings.radius;
  const float hh   = fmaxf(settings.height * 0.5f, r);
  const float ih   = hh - r;
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

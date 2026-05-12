/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "collision/characterBody.h"
#include "collision/gfxScale.h"
#include "scene/object.h"

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
  // Capsule = cylinder of length L along `up` capped with hemispheres of
  // radius r. The segment endpoints sit at center +/- (halfHeight - r) * up.
  // Support along a unit `dir`: abs(dir dot up) * (halfHeight - r) + r.
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

  // Build the per-frame velocity.
  // Persistent state is only the gravity accumulator (along `up`) plus the
  // latest input (orthogonal to `up`). Slope-following is realized by
  // reshaping this frame's displacement, never by writing to velocity_,
  // so leaving the ground doesn't carry slope-induced +up into the next
  // frame.
  auto horiz = inputVelocity - up * fm_vec3_dot(&inputVelocity, &up);
  float vAlongUp = fm_vec3_dot(&velocity, &up);
  if(wasOnFloor && !wasOnSteepSurface) {
    // Drop accumulated downward (gravity) drift while planted on walkable
    // floor, but keep any upward impulse the caller supplied via setVelocity.
    vAlongUp = fmaxf(vAlongUp, 0.0f);
  } else {
    vAlongUp -= settings.gravity * deltaTime;
  }
  if(vAlongUp < -settings.maxFallSpeed) vAlongUp = -settings.maxFallSpeed;
  velocity = horiz + up * vAlongUp;

  // This frame's displacement vector. When grounded on a walkable floor we
  // redirect horizontal input along the floor plane (slope following) and add
  // back any up-axis component from velocity_, so a jump impulse on the same
  // frame as walking still launches. Steep floors intentionally skip this so
  // stale walkable floor normals don't pin movement to the wrong plane.
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

  // Swept slide loop.
  bool sweptWalkableFloor = false;
  fm_vec3_t sweptFloorNormal = up;
  fm_vec3_t displacement = stepVel * deltaTime;
  for(uint8_t iter = 0; iter < settings.maxSlides; ++iter) {
    float dispLen2 = fm_vec3_len2(&displacement);
    if(dispLen2 < FM_EPSILON * FM_EPSILON) break;

    float dispLen = sqrtf(dispLen2);
    fm_vec3_t dir = displacement / dispLen;
    // Capsule support along the sweep direction so the long axis can't tunnel.
    const float reach = extentAlong(dir);

    Raycast ray = Raycast::create(
      capsuleCenter(), dir, dispLen + reach,
      settings.collTypes, false, settings.readMask
    );
    RaycastHit hit;
    bool didHit = scene.raycast(ray, hit) && hit.didHit;

    if(!didHit || hit.distance >= dispLen + reach) {
      owner->pos = owner->pos + displacement * gfxScale;
      break;
    }

    float allowed = fmaxf(hit.distance - reach, 0.0f);
    owner->pos = owner->pos + dir * (allowed * gfxScale);

    fm_vec3_t normal = vec3NormalizeOrFallback(hit.normal, up);
    fm_vec3_t remaining = dir * (dispLen - allowed);
    fm_vec3_t slide = remaining - vec3Project(remaining, normal);

    // When grounded, don't let a wall / steep-slope reprojection push the body downward
    // the tangent of a non-walkable surface generally has a -up component,
    // so each iteration converts a slice of horizontal input into a tiny descent,
    // which manifests as the body slowly sinking into the floor (the snap pass pops it back when input stops).
    // Clip the -up part so the slide stays in the floor plane.
    const float normalUp = fm_vec3_dot(&normal, &up);
    const float dirUp = fm_vec3_dot(&dir, &up);
    if(normalUp >= walkCos && dirUp <= FM_EPSILON) {
      sweptWalkableFloor = true;
      sweptFloorNormal = normal;
    }

    if(wasOnFloor && !wasOnSteepSurface && normalUp < walkCos) {
      const float slideUp = fm_vec3_dot(&slide, &up);
      if(slideUp < 0.0f) slide = slide - up * slideUp;
    }
    displacement = slide;

    // Only zero the gravity accumulator when the obstacle would push us
    // along -up (a ceiling while jumping). Wall and floor hits don't write
    // to velocity_; wall slides reshape displacement, floor contact is
    // resolved by the snap pass below.
    const float velUp = fm_vec3_dot(&velocity, &up);
    if(normalUp < -0.1f && velUp > 0.0f) {
      velocity = velocity - up * velUp;
    }
  }

  // Wall separation
  constexpr float DEPEN_SLIDE_RATE = 160.0f; // physics units / second
  const float maxPushPerFrame = DEPEN_SLIDE_RATE * deltaTime;
  {
    constexpr fm_vec3_t cardinalDirs[4]{
      { 1.0f, 0.0f,  0.0f},
      {-1.0f, 0.0f,  0.0f},
      { 0.0f, 0.0f,  1.0f},
      { 0.0f, 0.0f, -1.0f}
    };

    // Two probe origins along the capsule's central axis.
    // The center origin catches walls that overlap the body around shoulder height
    // the lower origin catches walls that sit entirely below the capsule center.
    // The lower probe sits one snap-distance above the body's bottom so anything shorter than `floorSnapDistance` stays invisible to de-penetrate and
    // remains auto-steppable, while anything taller registers and stops the body.
    // Each probe's cast length is the capsule's actual horizontal extent at that height
    // using the full radius low in the hemisphere would false-stop on geometry the capsule never actually touches.
    const float halfHeight = fmaxf(settings.height * 0.5f, settings.radius);
    const float r = settings.radius;
    const float lowerH = settings.floorSnapDistance;
    const float lowerExtent = (lowerH >= r)
      ? r
      : sqrtf(fmaxf(r*r - (r - lowerH)*(r - lowerH), 0.0f));

    struct Probe { fm_vec3_t origin; float reach; };
    const Probe probes[2] = {
      { capsuleCenter(), r },
      { capsuleCenter() + up * (-(halfHeight - lowerH)), lowerExtent },
    };

    for(const Probe& probe : probes)
    {
      const float castLen = probe.reach;
      for(int i = 0; i < 4; ++i)
      {
        const fm_vec3_t& dir = cardinalDirs[i];
        Raycast probeRay = Raycast::create(
          probe.origin, dir, castLen,
          settings.collTypes, false, settings.readMask
        );
        RaycastHit hit;
        if(!scene.raycast(probeRay, hit) || !hit.didHit) continue;
        if(hit.distance >= castLen) continue;

        const fm_vec3_t normal = vec3NormalizeOrFallback(hit.normal, dir * -1.0f);
        const float normalUp = fm_vec3_dot(&normal, &up);
        // Skip floors, those are handled by the floor snap below.
        if(normalUp > FM_EPSILON) continue;

        const float overlap = castLen - hit.distance;
        const float pushOut = fminf(overlap, maxPushPerFrame);
        owner->pos = owner->pos - dir * (pushOut * gfxScale);
      }
    }
  }

  // Floor probe + un-sink + snap.
  // Probe runs from the capsule center downward. Starting at the top made
  // sloped ceilings win as the nearest downward hit, which then looked like
  // a floor correction. The center origin keeps the probe focused on the
  // lower half of the body while still detecting normal floor penetration.
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

      // Reject hits outside the snap window
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

        const bool stick = wasOnFloor && velUp <= 0.0f;
        const bool landed = !wasOnFloor && velUp <= 0.0f &&
                            effectiveClearance == 0;

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

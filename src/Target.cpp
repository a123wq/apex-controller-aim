#include "App.hpp"
#include <glm/geometric.hpp>
#include <cstdlib>
#include <cmath>

// Deterministic-ish float RNG (std::rand is fine for a trainer).
static float randUnit() {
    return (std::rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
}
static float randRange(float lo, float hi) {
    return lo + (std::rand() / static_cast<float>(RAND_MAX)) * (hi - lo);
}

Target::Target() { reset(); }

// Half-height of the target's vertical extent from its center, by model.
// Sphere: radius. Capsule: capsuleHeight/2 (hemispheres included).
float Target::halfHeight() const {
    return m_model == TargetModel::Capsule ? capsuleHeight * 0.5f : m_state.radius;
}

void Target::reset() {
    m_state.x = 0.0f;
    m_state.y = halfHeight();  // resting on the floor (bottom touches y=0)
    m_state.z = baseZ;
    m_state.radius = 0.45f;
    m_state.active = true;
    m_vy = 4.0f;  // start with an upward kick so it begins hopping immediately
    m_aimed = false;
    m_aimCooldown = 0.0f;
    m_aimLossGrace = 0.0f;
    m_redirectTimer = 0.0f;
    pickNewDirection();
}

void Target::pickNewDirection() {
    // Pick a random horizontal direction (XZ plane only), preferring lateral
    // motion so the target rarely just charges straight at/away from the camera.
    for (int attempt = 0; attempt < 8; ++attempt) {
        float dx = randUnit();
        float dz = randUnit();
        float len = std::sqrt(dx*dx + dz*dz);
        if (len > 0.001f) {
            m_dirX = dx / len;
            m_dirZ = dz / len;
            break;
        }
    }
    if (m_dirX == 0.0f && m_dirZ == 0.0f) {
        m_dirX = 1.0f; m_dirZ = 0.0f;
    }
    m_speed = randRange(minSpeed, maxSpeed);

    // Schedule the next NON-aimed auto re-direction (lively idle motion).
    m_redirectTimer = randRange(waitTimeMin, waitTimeMax);
}

void Target::pickEscapeDirection(float camFx, float camFy, float camFz) {
    setCrashStep("[T] pickEscapeDirection enter");
    // When the crosshair is on us, flee PERPENDICULAR to the aim line so we
    // actually exit the crosshair silhouette instead of jittering along it.
    //
    // The plane normal to the camera forward is spanned by:
    //   right = cross(worldUp, forward)   (purely lateral)
    //   up   = cross(forward, right)      (vertical component)
    // We build the escape vector mostly from `right` (sideways, the most
    // effective way to leave a crosshair) with a smaller `up` component.
    glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
    glm::vec3 fwd(camFx, camFy, camFz);
    float fl = glm::length(fwd);
    if (fl > 0.0001f) fwd /= fl; else fwd = glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 right = glm::cross(worldUp, fwd);
    if (glm::length(right) < 0.0001f) right = glm::vec3(1.0f, 0.0f, 0.0f);
    right = glm::normalize(right);
    // up component is ignored for horizontal-only movement

    // Randomly pick left or right (unpredictable which way it breaks).
    float sideSign = (std::rand() & 1) ? 1.0f : -1.0f;
    m_dirX = right.x * sideSign;
    m_dirZ = right.z * sideSign;
    // Normalize
    float len = std::sqrt(m_dirX*m_dirX + m_dirZ*m_dirZ);
    if (len > 0.001f) {
        m_dirX /= len;
        m_dirZ /= len;
    } else {
        m_dirX = 1.0f; m_dirZ = 0.0f;
    }
    // Boost speed when fleeing so it decisively clears the crosshair.
    m_speed = randRange(minSpeed, maxSpeed) * aimSpeedBoost;
}

void Target::setAimed(bool aimed, float camFx, float camFy, float camFz, double dt) {
    float dtf = static_cast<float>(dt);

    // --- Hysteresis on the aim-hit signal ---
    // The ray-sphere test flickers true/false at the silhouette edge. To stop
    // that flicker from re-rolling the escape direction every frame, we keep
    // the aim-lock "sticky": once aimed, a brief drop-off (<= aimLossGraceTime)
    // does NOT count as leaving. Only a sustained drop-off releases the lock.
    if (aimed) {
        m_aimLossGrace = aimLossGraceTime;  // refresh grace while on target
    } else {
        m_aimLossGrace -= dtf;
        if (m_aimLossGrace > 0.0f) {
            aimed = true;  // still within grace → treat as still aimed
        }
    }

    bool wasAimed = m_aimed;
    m_aimed = aimed;
    m_aimCooldown -= dtf;

    if (aimed) {
        // Re-roll the escape direction ONLY when:
        //   (a) we just acquired the lock (wasAimed false), OR
        //   (b) the cooldown window (aimRedirectMin..Max) has elapsed.
        // The grace period above guarantees (a) only fires on a REAL fresh
        // acquisition, not on edge jitter — so aimRedirectMin/Max now actually
        // control how long a direction holds.
        if (!wasAimed || m_aimCooldown <= 0.0f) {
            pickEscapeDirection(camFx, camFy, camFz);
            m_aimCooldown = randRange(aimRedirectMin, aimRedirectMax);
            // Suppress the idle redirect timer so the aimed window governs
            // how long this direction holds.
            m_redirectTimer = m_aimCooldown;
        }
    } else {
        // Genuinely left the target (past the grace window): let normal idle
        // redirection resume. Do NOT touch m_aimCooldown here — leaving it
        // nonzero would suppress the next acquisition's fresh re-roll only if
        // wasAimed were true, which it isn't, so it's harmless either way; but
        // resetting it to 0 is cleanest.
        m_aimCooldown = 0.0f;
    }
}

void Target::update(double dt) {
    if (!m_state.active) return;
    float dtf = static_cast<float>(dt);

    // --- Bounce physics: gravity + ground collision ---
    m_vy += bounceGravity * dtf;
    m_state.y += m_vy * dtf;

    // Ground collision: the target's BOTTOM touches y=0, so center >= halfHeight.
    float floorY = halfHeight();
    if (m_state.y <= floorY) {
        m_state.y = floorY;
        if (m_vy < 0.0f) {
            // Reflect upward with restitution.
            m_vy = -m_vy * bounceRestitution;
            // If the bounce is too weak to be visible, give it a fresh kick
            // so the target keeps hopping (a trainer target should stay lively).
            if (m_vy < 1.5f) {
                m_vy = 4.0f;  // fixed re-hop velocity (~0.8m hop)
            }
        }
    }

    // --- Horizontal movement (XZ plane only) ---
    m_state.x += m_dirX * m_speed * dtf;
    m_state.z += m_dirZ * m_speed * dtf;

    // Bounce off the bounding box walls (reflect the velocity component) so the
    // target stays in view. Reflection keeps speed constant; only direction flips.
    if (m_state.x >  boundX) { m_state.x =  boundX; m_dirX = -std::abs(m_dirX); }
    if (m_state.x < -boundX) { m_state.x = -boundX; m_dirX =  std::abs(m_dirX); }

    float zMin = baseZ - boundZ, zMax = baseZ + boundZ;
    if (m_state.z > zMax) { m_state.z = zMax; m_dirZ = -std::abs(m_dirZ); }
    if (m_state.z < zMin) { m_state.z = zMin; m_dirZ =  std::abs(m_dirZ); }

    // Hard camera-clearance clamp: no matter how the user sets boundZ, the
    // target must never cross in front of the camera (z=0) closer than
    // cameraClearDist. If the bounds above allowed it, push it back and flip
    // the z-velocity so it flees away from the camera.
    {
        float zNear = -cameraClearDist;  // camera looks toward -Z, so "near" is -dist
        if (m_state.z > zNear) {
            m_state.z = zNear;
            if (m_dirZ > 0.0f) m_dirZ = -m_dirZ;  // reverse any camera-ward motion
        }
    }

    // Periodic random re-direction, but ONLY when not currently in an aimed
    // escape (the aimed window governs direction while you're on target).
    if (!m_aimed) {
        m_redirectTimer -= dtf;
        if (m_redirectTimer <= 0.0f) {
            pickNewDirection();
        }
    }
}
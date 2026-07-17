#include "App.hpp"
#include <glm/geometric.hpp>
#include <cmath>

// =============================================================================
// AimAssist — controller-path aim assist, modeled on Apex's three mechanisms:
//   1. Slowdown       — lowers right-stick sensitivity inside the AA bubble
//                        (stick 100% → camera 40-60%). Produces "stickiness".
//   2. Rotational      — adds a small yaw/pitch that follows the target's motion
//                        (velocity-follow) + optional pull toward bubble center.
//   3. Movement gate   — rotational requires movement input (left stick/WASD);
//                        slowdown is independent of movement.
//
// Acts ONLY on the trainer's own simulated targets (camera behavior). No Apex
// interaction — see CLAUDE.md scoped exception. Mouse path never calls this.
//
// Pipeline: ResponseCurve → AimAssist.apply (slowdown + rotational) → Camera.
// Camera is untouched (still just consumes angular velocity).
// =============================================================================

namespace {
// GLM lacks a smoothstep we can rely on without pulling <glm/glm.hpp>; roll our
// own (C++20 has no std::smoothstep). Standard Hermite smoothstep, clamped.
float smoothstep(float e0, float e1, float x) {
    if (e1 - e0 < 1e-6f) return x >= e1 ? 1.0f : 0.0f;  // degenerate → step
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

AimAssist::AAOutput AimAssist::apply(float inYaw, float inPitch,
                                      const glm::vec3& eye, const glm::vec3& forward,
                                      const glm::vec3& tgtPos, const glm::vec3& tgtVel,
                                      float moveMag, const AimProfile& p, double dt) {
    (void)dt;  // rotational term is instantaneous (velocity-follow); no dt needed.

    // --- 1. Direction & distance to target ---
    glm::vec3 toTarget = tgtPos - eye;
    float dist = glm::length(toTarget);
    if (dist < 0.5f) {
        // Target essentially on top of the camera — no meaningful bubble.
        // Honor M1: clear any residual rotation so AA stops immediately.
        m_inBubble = false;
        m_smoothRotYaw = 0.0f;
        m_smoothRotPitch = 0.0f;
        return { inYaw, inPitch, false };
    }
    glm::vec3 dir = toTarget / dist;  // unit direction eye→target

    // Defensive: forward should already be unit (Trainer::cameraForward
    // normalizes), but normalize again so apply() is robust to any caller.
    glm::vec3 fwd = forward;
    float fl = glm::length(fwd);
    if (fl > 1e-6f) fwd /= fl; else fwd = glm::vec3(0.0f, 0.0f, -1.0f);

    // --- 2. Angle between aim ray and target direction ---
    float cosA = std::clamp(glm::dot(fwd, dir), -1.0f, 1.0f);
    bool  inFront = cosA > 0.0f;
    float angle = std::acos(cosA);  // radians, 0 = dead on

    // --- 3. Bubble test (L3: aaBubbleAngle is stored in DEGREES) ---
    float bubbleRad = glm::radians(p.aaBubbleAngle);
    bool inBubble = p.aaEnabled && inFront && angle <= bubbleRad && dist <= p.aaMaxDistance;

    // --- 4. Bubble ramp 0..1 (center → edge) ---
    float bubbleT = 0.0f;
    if (inBubble) {
        bubbleT = (bubbleRad > 1e-6f)
            ? std::clamp(1.0f - angle / bubbleRad, 0.0f, 1.0f)
            : 1.0f;
    }

    // --- 5. Slowdown (stickiness axis, independent of movement) ---
    // aaStickiness: 0 = not sticky (no slowdown, factor 1.0), 1 = most sticky
    // (stick 100% → camera 40%, the doc's slowdown-range floor). lerp by bubbleT
    // so the slowdown ramps in across the bubble (edge → center).
    float slowdownFactor = std::lerp(1.0f, 0.4f, p.aaStickiness);
    float factor = std::lerp(1.0f, slowdownFactor, bubbleT);
    float outYaw   = inYaw   * factor;
    float outPitch = inPitch * factor;

    // --- 6. Rotational term (only while in the bubble) ---
    float rotYaw = 0.0f;
    float rotPitch = 0.0f;
    if (inBubble) {
        // view basis: right = cross(forward, worldUp) (horizontal, = "right" at
        // yaw=0). L2: degenerate at extreme pitch (looking straight up/down) →
        // fall back to +X. up = cross(right, forward) (unit: right⊥forward).
        glm::vec3 worldUp(0.0f, 1.0f, 0.0f);
        glm::vec3 right = glm::cross(fwd, worldUp);
        if (glm::length(right) < 1e-4f) right = glm::vec3(1.0f, 0.0f, 0.0f);
        right = glm::normalize(right);
        glm::vec3 up = glm::cross(right, fwd);

        // Target velocity projected onto view-right / view-up.
        float vRight = glm::dot(tgtVel, right);
        float vUp    = glm::dot(tgtVel, up);

        // Movement gate: rotational fades in as the player moves. Slowdown is
        // NOT gated (it stays active while stationary) — matches Apex.
        float moveF = smoothstep(p.aaMovementGate, p.aaMovementGate + 0.2f, moveMag);

        const float RAD2DEG = 57.295779513082323f;  // 180/π

        // Velocity-follow: ω_target ≈ vRight/dist (rad/s, small-angle). gain =
        // fraction of target angular velocity the camera matches (0.4 PC / 0.6
        // console). Signs (verified vs Camera.cpp:25-26 & cameraForward):
        //   target rightward (vRight>0) → turn right → yaw↓   → rotYaw  negative
        //   target upward    (vUp>0)    → look up    → pitch↑ → rotPitch positive
        rotYaw   = -(vRight / dist) * p.aaRotationalGain * bubbleT * moveF * RAD2DEG;
        rotPitch =  (vUp    / dist) * p.aaRotationalGain * bubbleT * moveF * RAD2DEG;

        // Position pull toward bubble center (default aaPullGain=0 → skipped).
        // Stronger the further off-center (∝ angle), zero at dead-center. Same
        // sign logic as velocity-follow: pull the crosshair TOWARD the target.
        if (p.aaPullGain > 0.0f) {
            float pullDeg = std::clamp(angle, 0.0f, bubbleRad)
                          * p.aaPullGain * bubbleT * moveF;
            float side     = glm::dot(dir, right);  // >0: target right of aim
            float vertSide = glm::dot(dir, up);      // >0: target above aim
            rotYaw   += (side     > 0.0f ? -pullDeg : pullDeg);   // right → yaw↓
            rotPitch += (vertSide > 0.0f ?  pullDeg : -pullDeg);  // up  → pitch↑
        }

        // Clamp the rotational term so it can't yank the view too hard.
        rotYaw   = std::clamp(rotYaw,   -p.aaRotMaxSpeed, p.aaRotMaxSpeed);
        rotPitch = std::clamp(rotPitch, -p.aaRotMaxSpeed, p.aaRotMaxSpeed);
    }

    // --- 7. EMA smoothing + M1 exit-clear ---
    // M1: leaving the bubble zeroes the smoothed rotation immediately — no
    // ~30-60ms rotational tail (Apex AA "stops immediately on leaving bubble").
    // The slowdown (bubbleT lerp) is already continuous and snaps instantly.
    if (!inBubble) {
        m_smoothRotYaw = 0.0f;
        m_smoothRotPitch = 0.0f;
    } else {
        float alpha = 1.0f - p.aaSmoothing;  // 0 smoothing → alpha 1 (instant)
        m_smoothRotYaw   += alpha * (rotYaw   - m_smoothRotYaw);
        m_smoothRotPitch += alpha * (rotPitch - m_smoothRotPitch);
    }

    // --- 8. Apply rotational term on top of the (slowed) player input ---
    outYaw   += m_smoothRotYaw;
    outPitch += m_smoothRotPitch;

    m_inBubble = inBubble;
    return { outYaw, outPitch, inBubble };
}

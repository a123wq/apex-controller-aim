#include "App.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <cmath>

// =============================================================================
// Ray intersection utilities — shared by Trainer + all GameMode subclasses
// (Modes.cpp). `fwd` MUST be unit length. Returns the entry param t>0 along the
// ray (eye + fwd*t) of the first surface hit, or -1.0f if no hit.
// =============================================================================

// Ray vs sphere. Closest point on the ray to `center`; hit if within `radius`.
float raySphereHitT(const glm::vec3& eye, const glm::vec3& fwd,
                    const glm::vec3& center, float radius) {
    glm::vec3 oc = eye - center;
    float b = glm::dot(oc, fwd);
    float c = glm::dot(oc, oc) - radius * radius;
    float disc = b * b - c;
    if (disc < 0.0f) return -1.0f;
    float t = -b - std::sqrt(disc);   // nearest intersection
    if (t < 0.0f) t = -b + std::sqrt(disc);  // ray starts inside → take far hit
    return t >= 0.0f ? t : -1.0f;
}

// Ray vs capsule (vertical segment a→b, radius). Distance from ray to the
// segment <= radius counts as a hit; returns the ray param of the entry point.
float rayCapsuleHitT(const glm::vec3& eye, const glm::vec3& fwd,
                     const glm::vec3& a, const glm::vec3& b, float radius) {
    // Closest approach of ray (eye + s*fwd) to segment (a + t*u).
    glm::vec3 u = b - a;
    glm::vec3 w0 = a - eye;
    float aA = glm::dot(fwd, fwd);     // 1 for unit dir
    float bB = glm::dot(fwd, u);
    float cC = glm::dot(u, u);
    float dD = glm::dot(fwd, w0);
    float eE = glm::dot(u, w0);
    float denom = aA * cC - bB * bB;
    float s, t;
    const float EPS = 1e-6f;
    if (denom <= EPS) {
        s = 0.0f;
        t = (cC > EPS) ? glm::clamp(eE / cC, 0.0f, 1.0f) : 0.0f;
    } else {
        s = glm::clamp((bB * eE - cC * dD) / denom, 0.0f, 1000.0f);
        t = (bB * s - eE) / cC;
        if (t < 0.0f) { t = 0.0f; s = glm::clamp(-dD / aA, 0.0f, 1000.0f); }
        else if (t > 1.0f) { t = 1.0f; s = glm::clamp((bB - dD) / aA, 0.0f, 1000.0f); }
    }
    glm::vec3 pRay = eye + fwd * s;
    glm::vec3 pSeg = a + u * t;
    float dist = glm::length(pRay - pSeg);
    if (dist > radius) return -1.0f;
    return s >= 0.0f ? s : -1.0f;
}

// Ray vs AABB (slab method). Returns entry t>0, or -1 if miss / origin inside
// (origin-inside is treated as "not occluding ahead" → -1 so a cover the
// player is standing inside doesn't block their own aim).
float rayAABBHitT(const glm::vec3& eye, const glm::vec3& fwd,
                  const glm::vec3& boxMin, const glm::vec3& boxMax) {
    float tmin = 0.0f;
    float tmax = 1e30f;
    const float EPS = 1e-8f;
    for (int i = 0; i < 3; ++i) {
        float o = eye[i];
        float d = fwd[i];
        float lo = boxMin[i];
        float hi = boxMax[i];
        if (std::abs(d) < EPS) {
            // ray parallel to this axis slab — miss if origin outside the slab
            if (o < lo || o > hi) return -1.0f;
        } else {
            float inv = 1.0f / d;
            float t1 = (lo - o) * inv;
            float t2 = (hi - o) * inv;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return -1.0f;
        }
    }
    return tmin > 0.0f ? tmin : -1.0f;  // tmin<=0 means origin inside
}

// =============================================================================
Trainer::Trainer() {
    m_config = AppConfig();          // defaults
    m_round.roundDuration = m_config.roundDuration;
    m_round.countdownSec   = m_config.countdownSec;
    buildMode();                     // construct the default (Tracking) mode
}

void Trainer::buildMode() {
    // Construct the concrete subclass for m_modeId via the factory in Modes.cpp.
    // (Trainer only holds the abstract base via unique_ptr, so it doesn't need
    // the concrete class definitions.)
    m_mode = makeGameMode(m_modeId, m_config);
    if (m_mode) {
        m_mode->onEnter();
        m_round.phase = RoundPhase::Idle;
        m_round.phaseTimer = 0.0;
        m_round.countdownDisplay = 0;
        m_round.startRequested = false;
    }
}

void Trainer::setMode(GameModeId id, const AppConfig& config) {
    m_modeId = id;
    m_config = config;
    m_round.roundDuration = config.roundDuration;
    m_round.countdownSec   = config.countdownSec;
    buildMode();
}

void Trainer::applyConfig(const AppConfig& config) {
    m_config = config;
    m_round.roundDuration = config.roundDuration;
    m_round.countdownSec   = config.countdownSec;
    // Live-apply: update the active mode's snapshot + re-derive geometry WITHOUT
    // resetting the round/scores (buildMode would reset phase→Idle, killing any
    // in-progress round whenever a slider is dragged — that was the "参数改了
    // 不立即生效" + needing-a-mode-switch symptom). The mode's applyLiveConfig
    // keeps motion state + scores intact.
    if (m_mode) m_mode->applyLiveConfig(config);
}

void Trainer::requestRoundStart() { m_round.requestStart(); }

void Trainer::fire() {
    if (!m_mode) return;
    if (m_round.phase != RoundPhase::Playing) return;
    if (!m_mode->rtIsFire()) return;   // non-shooting modes ignore fire
    float fx, fy, fz;
    cameraForward(fx, fy, fz);
    glm::vec3 eye = m_camera.getPosition();
    m_mode->onFire(eye, glm::vec3(fx, fy, fz));
}

const ModeScore& Trainer::getScore() const {
    static const ModeScore empty{};
    return m_mode ? m_mode->score() : empty;
}

// =============================================================================
// Player movement & physics (WASD + left stick + jump + gravity)
// Called by App every frame, regardless of uiWantsInput.
// Collides with the room walls so the player can't walk through them.
// =============================================================================
// Room half-extents — must match Renderer's ROOM_W / ROOM_D.
static constexpr float ROOM_W = 10.0f;
static constexpr float ROOM_D = 15.0f;
static constexpr float PLAYER_RADIUS = 0.3f;   // collision radius

void Trainer::updatePlayer(const StickState& input, double dt) {
    float moveX = input.moveX;
    float moveY = input.moveY;

    // Left stick overrides if active (XInput deadzone already applied upstream).
    // XInput sThumbLX/LY: right=+, up=+. moveX +1 = strafe right, moveY +1 =
    // forward (W). So both map directly — NO sign flip. (The old `-leftY` was a
    // bug: it made push-up = walk backward. Don't confuse with SDL joystick
    // axes, which report up as negative.)
    if (std::abs(input.leftX) > 0.05f) moveX = input.leftX;
    if (std::abs(input.leftY) > 0.05f) moveY = input.leftY;

    // Jump: Space (keyboard) or A button (controller) — only when on ground.
    if (input.jumpRequested && m_player.grounded) {
        m_player.vy = playerJumpSpeed;
        m_player.grounded = false;
    }

    // Gravity (integrate vy each frame).
    m_player.vy += playerGravity * static_cast<float>(dt);

    // Horizontal movement is relative to current camera yaw so strafing
    // feels natural regardless of which way you're looking.
    //
    // These forward/right vectors MUST match the Renderer's view-matrix
    // convention exactly, or WASD will drift relative to where you look
    // after turning. The Renderer rotates (0,0,-1) by +yaw around +Y
    // (glm right-handed), which yields forward = (-sin yaw, 0, -cos yaw)
    // and right = cross(forward, +Y) = (cos yaw, 0, -sin yaw).
    float yawRad = glm::radians(m_camera.getState().yaw);
    float fwdX   = -std::sin(yawRad);
    float fwdZ   = -std::cos(yawRad);
    float rightX =  std::cos(yawRad);
    float rightZ = -std::sin(yawRad);

    // moveY: W=+1 (forward), S=-1 (back) ; moveX: D=+1 (right), A=-1 (left)
    m_player.vx = (fwdX * moveY + rightX * moveX) * playerMoveSpeed;
    m_player.vz = (fwdZ * moveY + rightZ * moveX) * playerMoveSpeed;

    float dtf = static_cast<float>(dt);

    // --- Integrate X, then clamp to room walls (with player radius) ---
    m_player.x += m_player.vx * dtf;
    float xLimit = ROOM_W - PLAYER_RADIUS;
    if (m_player.x >  xLimit) { m_player.x =  xLimit; m_player.vx = 0.0f; }
    if (m_player.x < -xLimit) { m_player.x = -xLimit; m_player.vx = 0.0f; }

    // --- Integrate Z, then clamp to room walls ---
    m_player.z += m_player.vz * dtf;
    float zLimit = ROOM_D - PLAYER_RADIUS;
    if (m_player.z >  zLimit) { m_player.z =  zLimit; m_player.vz = 0.0f; }
    if (m_player.z < -zLimit) { m_player.z = -zLimit; m_player.vz = 0.0f; }

    // --- Integrate Y (gravity) and collide with floor + ceiling ---
    m_player.y += m_player.vy * dtf;
    if (m_player.y <= 0.0f) {
        m_player.y = 0.0f;
        m_player.vy = 0.0f;
        m_player.grounded = true;
    }
    // Ceiling at 8m; clamp so the player's head doesn't go through it.
    constexpr float CEIL = 8.0f - 0.2f;
    if (m_player.y > CEIL) {
        m_player.y = CEIL;
        if (m_player.vy > 0.0f) m_player.vy = 0.0f;
    }

    // --- Collide with solid cover boxes (XZ-plane disc vs AABB). The active
    // mode (e.g. CoverTrackingMode) exposes its colliders via getCollisionAABBs.
    // We treat the player as a horizontal disc of PLAYER_RADIUS at (x,z) and
    // push it out of any AABB it overlaps, stopping the velocity into the box.
    if (m_mode) {
        std::vector<AABB> boxes;
        m_mode->getCollisionAABBs(boxes);
        for (const AABB& box : boxes) {
            // Closest point on the AABB (in XZ) to the player center. Y is
            // ignored: if the player's vertical span [y, y+eyeHeight] overlaps
            // the box's Y span, the box blocks horizontally.
            bool yOverlap = (m_player.y < box.max.y) &&
                            (m_player.y + eyeHeight > box.min.y);
            if (!yOverlap) continue;
            float cx = std::clamp(m_player.x, box.min.x, box.max.x);
            float cz = std::clamp(m_player.z, box.min.z, box.max.z);
            float dx = m_player.x - cx;
            float dz = m_player.z - cz;
            float d2 = dx * dx + dz * dz;
            if (d2 > PLAYER_RADIUS * PLAYER_RADIUS) continue;  // no overlap

            if (d2 > 1e-8f) {
                // Outside the box face: push out along the shortest axis.
                float d = std::sqrt(d2);
                float push = PLAYER_RADIUS - d;
                float nx = dx / d, nz = dz / d;
                m_player.x += nx * push;
                m_player.z += nz * push;
            } else {
                // Player center is inside the box: push out on the nearest face.
                float distMinX = m_player.x - box.min.x;
                float distMaxX = box.max.x - m_player.x;
                float distMinZ = m_player.z - box.min.z;
                float distMaxZ = box.max.z - m_player.z;
                float m = std::min({distMinX, distMaxX, distMinZ, distMaxZ});
                if (m == distMinX)      m_player.x = box.min.x - PLAYER_RADIUS;
                else if (m == distMaxX) m_player.x = box.max.x + PLAYER_RADIUS;
                else if (m == distMinZ) m_player.z = box.min.z - PLAYER_RADIUS;
                else                    m_player.z = box.max.z + PLAYER_RADIUS;
            }
            // Kill velocity into the box so the player doesn't stick sliding in.
            m_player.vx = 0.0f;
            m_player.vz = 0.0f;
        }
    }

    // Sync camera eye to player feet + eye height.
    m_camera.setPosition(m_player.x, m_player.y + eyeHeight, m_player.z);
}

// =============================================================================
// Mouse aim path — called when NOT interacting with UI. delta is already a
// per-frame angle (no dt multiply). Round + mode advance happens first (shared
// with the controller path) so the round/target/scoring run in mouse mode too.
// =============================================================================
void Trainer::updateWithMouse(const StickState& input, float mouseDX, float mouseDY,
                                float sensX, float sensY,
                                float adsMultiplier, double dt) {
    (void)input;  // mouse path uses SDL relative motion, not the stick state
    advanceRoundAndMode(dt);

    setCrashStep("[T-mouse] updateWithMouse");
    float yawDelta   = -mouseDX * sensX;
    float pitchDelta = -mouseDY * sensY;
    yawDelta   *= adsMultiplier;
    pitchDelta *= adsMultiplier;
    m_camera.applyDelta(yawDelta, pitchDelta);
    setCrashStep("[T-mouse] done");
}

// =============================================================================
// advanceRoundAndMode — shared by controller + mouse paths. Drives the round
// state machine and the active GameMode one frame, filling m_sceneView + setting
// m_lastAimed. Called every frame regardless of input mode so the round timer,
// target motion, hit-test and scoring all keep running in both modes.
// =============================================================================
void Trainer::advanceRoundAndMode(double dt) {
    if (!m_mode) return;
    setCrashStep("[T] round.update");
    m_round.update(dt, *m_mode);

    setCrashStep("[T] mode.update");
    float fx, fy, fz;
    cameraForward(fx, fy, fz);
    glm::vec3 eye = m_camera.getPosition();
    glm::vec3 fwd(fx, fy, fz);
    // Only advance target motion during Playing; other phases keep it static
    // (cleaner countdown/finished feel), but still produce a SceneView so the
    // scene renders (target idle).
    m_sceneView.targets.clear();
    m_sceneView.covers.clear();
    double modeDt = (m_round.phase == RoundPhase::Playing) ? dt : 0.0;
    m_lastAimed = m_mode->update(modeDt, eye, fwd, m_sceneView);
}

// =============================================================================
// Controller aim path — response curve + aim assist + camera. Round + mode
// advance happens first (shared with mouse path via advanceRoundAndMode).
// =============================================================================
void Trainer::update(const StickState& input, double dt) {
    // Advance round + mode first (also runs in mouse path).
    advanceRoundAndMode(dt);

    // ADS: tracking family uses RT>0.5 / LT>0.5 as ADS; shooting family uses RT
    // to fire (handled in fire()) so RT is NOT ADS there.
    bool tracking = (!m_mode || m_mode->isTrackingFamily());
    bool ads = tracking ? (input.RT > 0.5f) : false;
    bool ltAds = tracking ? (input.LT > 0.5f) : false;
    float adsMult = (ads || ltAds) ? getProfile().adsMultiplier : 1.0f;

    setCrashStep("[T] responseCurve");
    // XInput sThumbRX: right=+. But camera yaw+ = turn LEFT (forward.x =
    // -sin yaw), so to make stick-right = look-right we feed -rightX — the
    // same sign flip the mouse path applies (-mouseDX). sThumbRY up=+ already
    // maps to pitch+ (look up), so rightY needs no flip.
    auto vel = m_responseCurve.process(
        -input.rightX, input.rightY,
        getProfile(), adsMult, dt
    );

    // ---- Aim assist (controller path ONLY) ----
    setCrashStep("[T] aimAssist");
    float fx, fy, fz;
    cameraForward(fx, fy, fz);
    glm::vec3 eye = m_camera.getPosition();
    glm::vec3 fwd(fx, fy, fz);
    glm::vec3 tgtPos(0.0f), tgtVel(0.0f);
    bool hasAA = m_mode->getAATarget(tgtPos, tgtVel);
    float moveMag = std::clamp(
        std::sqrt(input.leftX * input.leftX + input.leftY * input.leftY
                  + input.moveX * input.moveX + input.moveY * input.moveY),
        0.0f, 1.0f);
    if (hasAA) {
        auto aa = m_aimAssist.apply(vel.yawSpeed, vel.pitchSpeed, eye,
                                    fwd, tgtPos, tgtVel,
                                    moveMag, getProfile(), dt);
        vel.yawSpeed   = aa.yawSpeed;
        vel.pitchSpeed = aa.pitchSpeed;
    }

    setCrashStep("[T] camera.update");
    m_camera.update(vel.yawSpeed, vel.pitchSpeed, dt);
    setCrashStep("[T] done");
}

// -----------------------------------------------------------------------------
// Camera forward vector (raw 3 floats to avoid GLM aligned-gentype issues).
// -----------------------------------------------------------------------------
void Trainer::cameraForward(float& fx, float& fy, float& fz) const {
    const CameraState& cam = m_camera.getState();
    float yawR   = glm::radians(cam.yaw);
    float pitchR = glm::radians(cam.pitch);
    // Must match the Renderer's view-matrix forward (rotate (0,0,-1) by
    // +yaw around +Y, then +pitch around right). Yaw-X sign is -sin so the
    // aim ray lines up with where the camera actually looks.
    glm::vec3 fwd(
       -std::sin(yawR) * std::cos(pitchR),
        std::sin(pitchR),
       -std::cos(yawR) * std::cos(pitchR)
    );
    fwd = glm::normalize(fwd);
    fx = fwd.x; fy = fwd.y; fz = fwd.z;
}

// =============================================================================
// RoundManager — Idle / Countdown / Playing / Finished state machine.
// =============================================================================
void RoundManager::requestStart() {
    if (phase == RoundPhase::Idle || phase == RoundPhase::Finished) {
        startRequested = true;
    }
}

RoundPhase RoundManager::update(double dt, GameMode& mode) {
    switch (phase) {
        case RoundPhase::Idle:
            if (startRequested) {
                startRequested = false;
                mode.resetRound();
                phase = RoundPhase::Countdown;
                phaseTimer = static_cast<double>(countdownSec);
                countdownDisplay = static_cast<int>(std::ceil(phaseTimer));
            }
            break;

        case RoundPhase::Countdown: {
            phaseTimer -= dt;
            int disp = static_cast<int>(std::ceil(phaseTimer));
            if (disp < 0) disp = 0;
            countdownDisplay = disp;
            if (phaseTimer <= 0.0) {
                phase = RoundPhase::Playing;
                phaseTimer = static_cast<double>(roundDuration);
                countdownDisplay = 0;
            }
            break;
        }

        case RoundPhase::Playing:
            phaseTimer -= dt;
            if (phaseTimer <= 0.0) {
                phaseTimer = 0.0;
                mode.onRoundEnd();   // bank best-of-rounds record
                phase = RoundPhase::Finished;
            }
            break;

        case RoundPhase::Finished:
            if (startRequested) {
                startRequested = false;
                mode.resetRound();
                phase = RoundPhase::Countdown;
                phaseTimer = static_cast<double>(countdownSec);
                countdownDisplay = static_cast<int>(std::ceil(phaseTimer));
            }
            break;
    }
    return phase;
}

double RoundManager::playingRemaining() const {
    return phase == RoundPhase::Playing ? std::max(0.0, phaseTimer) : 0.0;
}

// -----------------------------------------------------------------------------
// Full reset (player/camera/round); mode is rebuilt so its best-of-rounds
// records are cleared too. Used on a global reset (not per-round; per-round is
// handled by RoundManager via mode.resetRound()).
// -----------------------------------------------------------------------------
void Trainer::reset() {
    m_camera = Camera();
    m_responseCurve = ResponseCurve();
    m_aimAssist = AimAssist();
    m_player = {};
    m_sceneView = SceneView();
    m_lastAimed = false;
    buildMode();   // fresh mode + Idle phase (clears mode best-of-rounds)
}
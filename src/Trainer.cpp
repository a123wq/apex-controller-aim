#include "App.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <cmath>

Trainer::Trainer() {}

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
    if (std::abs(input.leftX) > 0.05f) moveX = input.leftX;
    if (std::abs(input.leftY) > 0.05f) moveY = -input.leftY;

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

    // Sync camera eye to player feet + eye height.
    m_camera.setPosition(m_player.x, m_player.y + eyeHeight, m_player.z);
}

// =============================================================================
// Mouse aim path — called when NOT interacting with UI.
// delta is already a per-frame angle (no dt multiply).
// =============================================================================
void Trainer::updateWithMouse(const StickState& input, float mouseDX, float mouseDY,
                                float sensX, float sensY,
                                float adsMultiplier, double dt) {
    setCrashStep("[T-mouse] updateWithMouse");
    float yawDelta   = -mouseDX * sensX;
    float pitchDelta = -mouseDY * sensY;
    yawDelta   *= adsMultiplier;
    pitchDelta *= adsMultiplier;
    m_camera.applyDelta(yawDelta, pitchDelta);
    setCrashStep("[T-mouse] done");
}

// =============================================================================
// Controller aim path — response curve + camera yaw/pitch only.
// Player movement and target update are handled separately by App.
// =============================================================================
void Trainer::update(const StickState& input, double dt) {
    bool ads = input.RT > 0.5f;
    float adsMult = ads ? getProfile().adsMultiplier : 1.0f;

    setCrashStep("[T] responseCurve");
    auto vel = m_responseCurve.process(
        input.rightX, input.rightY,
        getProfile(), adsMult, dt
    );

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

// -----------------------------------------------------------------------------
// Aim-hit test: branches on target model.
//   Sphere : ray-sphere — distance from ray to center <= radius.
//   Capsule: ray vs vertical line segment (cylinder axis) — distance from ray
//            to the segment <= radius. The segment runs from y-hh to y+hh,
//            where hh = (capsuleHeight - 2*radius)/2 (the straight cylinder
//            part); hemispherical caps are approximated by treating the whole
//            segment length as capsuleHeight-2*radius and clamping, which is
//            close enough for a trainer.
// -----------------------------------------------------------------------------
namespace {
// Distance from a ray (origin o, unit dir d) to a segment (a->b).
float raySegDist(const glm::vec3& o, const glm::vec3& d,
                 const glm::vec3& a, const glm::vec3& b)
{
    glm::vec3 u = b - a;            // segment dir
    glm::vec3 w0 = a - o;
    float aA = glm::dot(d, d);      // 1 for unit dir
    float bB = glm::dot(d, u);
    float cC = glm::dot(u, u);
    float dD = glm::dot(d, w0);
    float eE = glm::dot(u, w0);
    float denom = aA * cC - bB * bB;
    float s, t;
    const float EPS = 1e-6f;
    if (denom <= EPS) {
        // parallel — clamp s to 0 and project
        s = 0.0f;
        t = (cC > EPS) ? glm::clamp(eE / cC, 0.0f, 1.0f) : 0.0f;
    } else {
        s = glm::clamp((bB * eE - cC * dD) / denom, 0.0f, 1000.0f);
        t = (bB * s - eE) / cC;
        if (t < 0.0f) { t = 0.0f; s = glm::clamp(-dD / aA, 0.0f, 1000.0f); }
        else if (t > 1.0f) { t = 1.0f; s = glm::clamp((bB - dD) / aA, 0.0f, 1000.0f); }
    }
    glm::vec3 pRay = o + d * s;
    glm::vec3 pSeg = a + u * t;
    return glm::length(pRay - pSeg);
}
} // namespace

bool Trainer::isAimingAtTarget() const {
    const TargetState& tgt = m_target.getState();
    const CameraState& cam = m_camera.getState();

    float fx, fy, fz;
    cameraForward(fx, fy, fz);
    glm::vec3 fwd(fx, fy, fz);
    const glm::vec3 eye(cam.x, cam.y, cam.z);

    if (m_target.getModel() == TargetModel::Capsule) {
        // Vertical segment through the target center, length = straight part.
        float r = tgt.radius;
        float straight = std::max(0.0f, m_target.capsuleHeight - 2.0f * r);
        float hh = straight * 0.5f;
        glm::vec3 a(tgt.x, tgt.y - hh, tgt.z);
        glm::vec3 b(tgt.x, tgt.y + hh, tgt.z);
        float dist = raySegDist(eye, fwd, a, b);
        return dist <= (r + 0.05f);
    }

    // Sphere
    glm::vec3 tgtPos(tgt.x, tgt.y, tgt.z);
    glm::vec3 toTarget = tgtPos - eye;
    float t = glm::dot(toTarget, fwd);
    if (t < 0.0f) return false;  // behind camera
    glm::vec3 closest = eye + fwd * t;
    float dist = glm::length(tgtPos - closest);
    return dist <= (tgt.radius + 0.05f);
}

// -----------------------------------------------------------------------------
// Scoring: +1 point per frame while aimed at the target. If the score does not
// change for 5 seconds, the current run is banked into best-score and reset.
// -----------------------------------------------------------------------------
void Trainer::updateScore(bool aimed, double dt) {
    int prev = m_currentScore;
    if (aimed) {
        // +1 per frame while on target. dt is clamped to ~1/144..1/10s in App,
        // so this is a smooth ramp rather than a frame-rate-sensitive counter.
        m_currentScore += 1;
    }

    if (m_currentScore != prev) {
        // Score changed this frame → reset the idle timer.
        m_idleTimer = 0.0;
    } else {
        m_idleTimer += dt;
        // 5s of no change → bank current as best, then clear the run.
        if (m_idleTimer >= 5.0) {
            if (m_currentScore > m_bestScore) {
                m_bestScore = m_currentScore;
            }
            m_currentScore = 0;
            m_idleTimer = 0.0;
        }
    }
}

void Trainer::reset() {
    m_camera = Camera();
    m_target.reset();
    m_responseCurve = ResponseCurve();
    m_player = {};
    m_currentScore = 0;
    m_idleTimer = 0.0;
    // bestScore is deliberately kept across a reset — it's the high score.
}
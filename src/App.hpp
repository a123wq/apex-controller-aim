#pragma once

// =============================================================================
// Standard / Platform
// =============================================================================
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <stdexcept>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <memory>

// =============================================================================
// Lightweight logging — name must NOT conflict with glm::log()
// =============================================================================
static inline void app_err(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}
#define APP_LOG(FMT, ...) app_err("[Apex] " FMT "\n", ##__VA_ARGS__)

// Crash diagnostics — writes a line to crash_diag.txt (flushed each call).
// Defined in main.cpp. Used to bracket init/loop steps so a crash leaves a
// trail showing the last completed step.
void diag(const char* msg);
// Printf-style diagnostics into crash_diag.txt (defined in main.cpp).
void diagf(const char* fmt, ...);
// Updates the "current step" string the SEH/signal handlers will report on a
// crash. Call this around hot-path operations you want to localize.
void setCrashStep(const char* s);

// =============================================================================
// SDL2 — Windowing/Events/Input foundation (required dependency)
// =============================================================================
#include <SDL.h>

// =============================================================================
// GLM — Header-only 3D math
// =============================================================================
#define GLM_FORCE_RADIANS
// NOTE: GLM_FORCE_DEFAULT_ALIGNED_GENTYPES intentionally NOT defined. It gives
// glm::vec3 a 16-byte alignment, which breaks value-passing across function
// call boundaries under MinGW's calling convention — the classic cause of
// "crash on launch + delayed crash" memory corruption. We don't do SIMD opts
// here, so the alignment buys nothing and only adds risk.
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// =============================================================================
// AimProfile — The only source of parameters (CLAUDE.md spec)
// =============================================================================
struct AimProfile {
    std::string name            = "Classic43";
    float deadzone              = 0.08f;
    float outerDeadzone         = 0.02f;
    float curveExponent         = 2.2f;
    float microAimStrength      = 0.40f;
    float microAimThreshold     = 0.15f;
    float inputSmoothing        = 0.12f;
    float maxYawSpeed           = 420.0f;
    float maxPitchSpeed         = 360.0f;
    float adsMultiplier         = 0.65f;
    float acceleration          = 0.15f;
    float deceleration          = 0.12f;

    // ---- Aim Assist (controller path only; modeled on Apex AA) ----
    // AA acts ONLY on the trainer's own simulated targets (camera behavior).
    // No Apex interaction — see CLAUDE.md scoped exception. Two independent
    // strength axes: slowdown (stickiness) and rotational (velocity-follow),
    // plus an optional position-pull term. All hot-editable + JSON-persisted.
    bool  aaEnabled        = true;     // master toggle
    float aaBubbleAngle    = 4.0f;     // bubble half-angle (degrees)
    float aaMaxDistance     = 60.0f;    // max range for AA to engage (m)
    float aaStickiness     = 0.30f;    // slowdown 0=not sticky..1=most sticky
                                       // (maps to slowdownFactor lerp(1.0,0.4,t))
    float aaRotationalGain = 0.40f;    // PC 0.4 / console 0.6; fraction of the
                                       // target's angular velocity the camera
                                       // matches (velocity-follow)
    float aaPullGain       = 0.0f;     // pull-to-bubble-center strength (0=off)
    float aaRotMaxSpeed    = 25.0f;    // rotational term cap (deg/s)
    float aaMovementGate   = 0.15f;    // left-stick/movement magnitude gate
    float aaSmoothing      = 0.30f;    // rotational term EMA smoothing
};

// =============================================================================
// Profile — Load/Save/Reload JSON profiles
// =============================================================================
class Profile {
public:
    Profile();
    bool load(const std::string& name);
    bool save(const std::string& name);
    bool reload();

    const AimProfile& getCurrent() const { return m_current; }
    void setCurrent(const AimProfile& p) { m_current = p; }
    const std::vector<std::string>& getAvailable() const { return m_available; }
    void refreshList();
    std::string getCurrentName() const { return m_current.name; }

private:
    AimProfile m_current;
    std::vector<std::string> m_available;
    std::string m_profileDir;
    std::string m_currentFile;

    void scanDirectory();
    std::string getFilePath(const std::string& name) const;
};

// =============================================================================
// InputMode — Switch between controller and mouse
// =============================================================================
enum class InputMode { Controller, Mouse };

// =============================================================================
// TargetModel — which shape the target uses (aim + render branch on this)
// =============================================================================
enum class TargetModel { Sphere, Capsule };

// =============================================================================
// Game modes (selectable in the ESC panel). Each mode owns its own target
// motion / hit-test / scoring and produces a SceneView for the Renderer.
// =============================================================================
enum class GameModeId {
    Tracking,        // 1 target: strafe + jump, track scoring (3 scores)
    ThreeTarget,     // 10x10 grid, 3 visible, fire scoring (current+best)
    SixTarget,       // 10x10 grid, 6 visible, fire scoring
    CoverTracking,   // tracking + cover boxes that occlude the aim ray
    PillarPatrol,    // constant-speed lateral patrol, tall-thin capsule
    FreeOrbit        // 3D free motion, no gravity, 6-wall reflect
};

// Strafe speed mode for the Tracking family (mode 1's strafe feel).
enum class StrafeSpeedMode { Walk, Run, Random };

// Round state machine phases.
enum class RoundPhase { Idle, Countdown, Playing, Finished };

// =============================================================================
// Render data produced by the active GameMode each frame and consumed by the
// Renderer. Invisible targets (e.g. grid spheres not currently shown) are
// simply NOT added to `targets` — the Renderer only draws what's listed, so no
// alpha/blending is needed for "invisible" objects.
// =============================================================================
struct RenderTarget {
    glm::vec3 pos        = glm::vec3(0.0f);
    float     radius     = 0.45f;
    TargetModel model    = TargetModel::Sphere;
    float     capsuleHeight = 1.6f;
    float     color[3]   = { 1.0f, 0.65f, 0.55f };  // uBaseColor RGB (opaque)
};
struct RenderCover {
    glm::vec3 center      = glm::vec3(0.0f);
    glm::vec3 halfExtents = glm::vec3(1.0f);
};
// Axis-aligned box (min/max corners) for collision tests.
struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};
struct SceneView {
    std::vector<RenderTarget> targets;
    std::vector<RenderCover>  covers;
};

// Score state surfaced to the UI. A mode fills the fields relevant to it.
struct ModeScore {
    // Tracking family (Score1/2/3 from 练枪模式.md):
    int   trackLongestBurst = 0;  // longest single continuous aim burst (frames)
    int   trackRoundTotal   = 0;  // cumulative aimed frames this round (no clear)
    int   trackBestRound    = 0;  // max trackRoundTotal across rounds
    // Shooting family (Three/Six target):
    int   shootCurrent = 0;        // hits this round
    int   shootBest    = 0;        // best hits across rounds
};

// =============================================================================
// Config — persistent user settings in apexStickTrainer.conf next to the exe.
// Loaded at startup (auto-created if absent), saved whenever a value changes.
// =============================================================================
struct AppConfig {
    TargetModel targetModel = TargetModel::Sphere;
    float  targetRadius    = 0.45f;
    float  targetMinSpeed  = 2.0f;
    float  targetMaxSpeed  = 6.0f;
    float  aimRedirectMin  = 4.0f;
    float  aimRedirectMax  = 4.0f;
    float  mouseSensX      = 0.15f;
    float  mouseSensY      = 0.15f;

    // ---- Game mode + round settings ----
    int   gameMode        = 0;   // GameModeId as int (for JSON serialize)
    int   strafeSpeedMode = 0;   // StrafeSpeedMode as int
    float roundDuration    = 60.0f;
    float countdownSec      = 3.0f;

    // ---- Tracking family: bounce physics (shared by Tracking/Cover/Pillar) ----
    float bounceRestitution = 0.7f;
    float bounceGravity      = -9.8f;
    float bounceHeight       = 1.5f;   // hop kick velocity (m/s)
    bool  jumpEnabled        = true;    // whether the tracking target hops
    float targetBoundX       = 8.0f;   // lateral half-range
    float targetBoundZ       = 7.0f;
    float targetBaseZ        = -10.0f; // nominal center depth

    // ---- Grid (Three/Six target): 10x10 VERTICAL wall of spheres in the air,
    // facing the player at fixed depth Z. Spread across X and Y (view plane).
    float gridSpacing  = 0.9f;   // cell pitch (~7.2m span fits room height 8m)
    float gridRadius   = 0.18f;   // small enough that 0.9m spacing doesn't overlap
    float gridCenterY  = 4.0f;    // vertical center (~room mid height)
    float gridCenterZ  = -12.0f;  // depth in front of the player

    // ---- Cover (mode 4): boxes between player and target ----
    int   coverCount  = 3;
    float coverHalfX  = 1.5f;
    float coverHalfY  = 2.0f;
    float coverHalfZ  = 0.4f;

    // ---- Pillar patrol (mode 5): tall-thin capsule, constant speed ----
    float pillarHeight = 2.5f;
    float pillarRadius = 0.25f;
    float pillarSpeed   = 4.0f;

    // ---- Free orbit (mode 6): 3D velocity ----
    float freeOrbitSpeed = 5.0f;
};

class Config {
public:
    Config();
    // Load from <exeDir>/apexStickTrainer.conf. If missing, write defaults.
    bool load();
    // Persist current settings to the config file (call after any edit).
    bool save() const;
    // Mark dirty so save() runs on next App frame (avoids writing every frame).
    void markDirty() { m_dirty = true; }
    bool isDirty() const { return m_dirty; }
    void clearDirty() { m_dirty = false; }

    AppConfig&       data()       { return m_data; }
    const AppConfig& data() const { return m_data; }
private:
    AppConfig m_data;
    std::string m_filePath;
    bool m_dirty = false;
};

// True if any user-editable AppConfig field differs between a and b (used by App
// to detect UI slider edits that need a live re-apply to the active GameMode).
inline bool appConfigDiffers(const AppConfig& a, const AppConfig& b) {
    return a.targetModel    != b.targetModel    || a.targetRadius   != b.targetRadius
        || a.targetMinSpeed != b.targetMinSpeed || a.targetMaxSpeed != b.targetMaxSpeed
        || a.aimRedirectMin != b.aimRedirectMin || a.aimRedirectMax != b.aimRedirectMax
        || a.mouseSensX     != b.mouseSensX     || a.mouseSensY     != b.mouseSensY
        || a.gameMode        != b.gameMode        || a.strafeSpeedMode!= b.strafeSpeedMode
        || a.roundDuration    != b.roundDuration    || a.countdownSec   != b.countdownSec
        || a.bounceRestitution!= b.bounceRestitution|| a.bounceGravity  != b.bounceGravity
        || a.bounceHeight     != b.bounceHeight     || a.jumpEnabled     != b.jumpEnabled
        || a.targetBoundX     != b.targetBoundX     || a.targetBoundZ   != b.targetBoundZ
        || a.targetBaseZ      != b.targetBaseZ
        || a.gridSpacing  != b.gridSpacing  || a.gridRadius   != b.gridRadius
        || a.gridCenterY  != b.gridCenterY  || a.gridCenterZ  != b.gridCenterZ
        || a.coverCount   != b.coverCount   || a.coverHalfX   != b.coverHalfX
        || a.coverHalfY   != b.coverHalfY   || a.coverHalfZ   != b.coverHalfZ
        || a.pillarHeight!= b.pillarHeight || a.pillarRadius!= b.pillarRadius
        || a.pillarSpeed  != b.pillarSpeed  || a.freeOrbitSpeed!= b.freeOrbitSpeed;
}

// =============================================================================
// StickState — Raw Xbox controller input (via XInput)
// Also carries mouse delta from mouse mode
// =============================================================================
struct StickState {
    float leftX  = 0.0f, leftY  = 0.0f;
    float rightX = 0.0f, rightY = 0.0f;
    float LT = 0.0f, RT = 0.0f;
    bool  aButton     = false, bButton     = false;
    bool  xButton     = false, yButton     = false;
    bool  startButton = false, backButton  = false;
    // WASD / keyboard movement accumulator (set from pollEvents)
    float moveX = 0.0f, moveY = 0.0f;
    bool  jumpRequested = false;
    // Mouse mode: accumulated delta from last frame, in pixels
    float mouseDX     = 0.0f;
    float mouseDY     = 0.0f;
    bool  mouseGrab   = false; // true when mouse is captured (G held)
};

// =============================================================================
// Input — XInput wrapper + mouse accumulator
// =============================================================================
class Input {
public:
    Input();
    void update();
    const StickState& getState() const { return m_state; }
    bool isConnected() const { return m_connected; }

    // Called from App::pollEvents with SDL mouse deltas
    void injectMouseDelta(float dx, float dy) {
        m_state.mouseDX = dx;
        m_state.mouseDY = dy;
    }

private:
    StickState m_state;
    bool       m_connected = false;
    uint32_t   m_packetNumber = 0;
    float normalizeAxis(int16_t raw) const;
};

// =============================================================================
// PlayerState + Player Physics
// =============================================================================
struct PlayerState {
    float x = 0.0f, y = 0.0f, z = 0.0f;  // position (feet on ground)
    float vx = 0.0f, vy = 0.0f, vz = 0.0f; // velocity
    bool  grounded = true;
};

// =============================================================================
// CameraState + Camera
// =============================================================================
struct CameraState {
    float yaw = 0.0f, pitch = 0.0f;
    float x = 0.0f, y = 1.7f, z = 0.0f;  // eye position
};

class Camera {
public:
    Camera();
    void update(float yawSpeed, float pitchSpeed, double dt);
    void setPosition(float x, float y, float z);
    void setPosition(const glm::vec3& pos);
    glm::vec3 getPosition() const;
    // Apply angular deltas directly (per-frame), used by mouse input whose
    // delta is already a per-frame quantity (no dt multiply).
    void applyDelta(float yawDelta, float pitchDelta);
    const CameraState& getState() const { return m_state; }
    glm::mat4 getViewMatrix() const;
private:
    CameraState m_state;
    glm::mat4 computeViewMatrix(const CameraState& cam) const;
};

// =============================================================================
// TargetState + Target
//
// The target is a sphere. It drifts in a constant lateral direction and
// bounces with physics-based vertical motion. It re-picks direction on a
// timer (every 0.5–1.0s by default) OR the moment it is being aimed at — so
// you can't just hold the crosshair on it forever. It never rotates; only
// translates. Bounds form a box the center stays inside.
// =============================================================================
struct TargetState {
    float x = 0.0f, y = 0.0f, z = -15.0f;  // center position
    float radius = 0.45f;   // sphere radius
    bool  active = true;
};

class Target {
public:
    Target();
    void update(double dt);
    void reset();
    const TargetState& getState() const { return m_state; }
    // World-space velocity (m/s) for aim assist. Horizontal = m_dirX/m_dirZ
    // (unit dir) * m_speed; vertical = m_vy (bounce, gravity-integrated each
    // frame). By-value is safe here — GLM_FORCE_DEFAULT_ALIGNED_GENTYPES is
    // intentionally NOT defined (see header note), so glm::vec3 has no
    // 16-byte alignment that would break value-passing under MinGW.
    glm::vec3 getVelocity() const { return glm::vec3(m_dirX * m_speed, m_vy, m_dirZ * m_speed); }

    // Called by Trainer each frame with whether the crosshair is on the target
    // and the camera's forward direction (passed as 3 raw floats — NOT a
    // glm::vec3 — to avoid any GLM-aligned-gentype ABI hazard across the call).
    // An aimed-triggered redirect flees sideways, perpendicular to the aim line.
    void setAimed(bool aimed, float camFx, float camFy, float camFz, double dt);

    // Direct setters for the UI sliders (size hot-edit).
    void setRadius(float r) { m_state.radius = r; }
    void setModel(TargetModel m) { m_model = m; }
    TargetModel getModel() const { return m_model; }
    // Capsule geometry: radius is shared with the sphere; height is the
    // FULL capsule height (cylinder + 2 hemispheres). The aim test uses this.
    float capsuleHeight = 1.6f;
    // Mutable access for ImGui sliders that bind to &float.
    TargetState& stateRef() { return m_state; }

    // Movement bounds (center stays within these half-extents around origin).
    // The target must stay IN FRONT of the camera (which lives near z=0) by a
    // safe margin — if its z-range reaches the camera, the sphere gets inches
    // from the eye and looks gigantic / "overlaps" the view. baseZ - boundZ
    // is the nearest the target can approach; keep it <= -3m from the camera.
    float boundX = 8.0f;
    float boundZ = 7.0f;   // depth range around z=-10 → z in [-17, -3]
    float baseZ  = -10.0f; // nominal center depth

    float minSpeed    = 2.0f;
    float maxSpeed    = 6.0f;

    // Bounce physics
    float bounceRestitution = 0.7f;   // bounce factor (0-1)
    float bounceGravity     = -9.8f;  // m/s²
    float bounceHeight      = 0.0f;   // initial drop height (0 = starts at ground)

    // Non-aimed, periodic re-direction window (keeps motion alive even when
    // not being tracked). Default short so idle motion looks lively.
    float waitTimeMin = 0.5f;
    float waitTimeMax = 1.0f;

    // Aimed-triggered re-direction window. Once you're on target, the target
    // commits to ONE escape direction for this long before re-rolling, giving
    // it the full window to actually leave the crosshair — no jittery
    // re-rolling every fraction of a second.
    float aimRedirectMin = 4.0f;
    float aimRedirectMax = 4.0f;

    // Hysteresis grace period: if the crosshair briefly drops off the target
    // (edge jitter) for less than this long, the aim-lock is NOT released.
    // Prevents the aim-hit test's true/false flicker at the sphere silhouette
    // from re-triggering an escape re-roll every frame.
    float aimLossGraceTime = 0.25f;  // seconds

    // Hard safety margin: the target center can never get closer to the camera
    // (which lives near z=0) than this, regardless of boundZ. Stops the sphere
    // from pressing against the eye and looking gigantic / overlapping the view.
    float cameraClearDist = 3.0f;  // metres in front of the camera (z <= -this)

    // Speed boost when fleeing an aimed state (so it decisively breaks away).
    float aimSpeedBoost = 1.5f;
private:
    TargetState m_state;
    TargetModel m_model = TargetModel::Sphere;
    float m_dirX = 0.0f, m_dirZ = 0.0f;    // current horizontal unit dir (XZ, no Y)
    float m_vy = 0.0f;                      // vertical velocity
    float m_speed = 0.0f;
    float m_redirectTimer = 0.0f;  // counts down to next auto re-direction
    float m_aimCooldown   = 0.0f;  // time until next escape re-roll while aimed
    float m_aimLossGrace  = 0.0f;  // hysteresis: time left before "leaving" target
    bool  m_aimed         = false;
    float halfHeight() const;   // vertical extent from center (model-dependent)
    void  pickNewDirection();
    void  pickEscapeDirection(float camFx, float camFy, float camFz);
};

// =============================================================================
// ResponseCurve — Stick → angular velocity pipeline
// =============================================================================
class ResponseCurve {
public:
    struct AngularVelocity { float yawSpeed = 0.0f, pitchSpeed = 0.0f; };

    ResponseCurve();

    AngularVelocity process(float rawX, float rawY, const AimProfile& profile,
                            float adsMultiplier, double dt);

    static float applyDeadzone(float value, float dz, float odz);
    static float applyCurve(float normalized, float exponent);
    static float applyMicroAim(float value, float threshold, float strength);
    static float applySmoothing(float raw, float smoothing, float& smoothed);
    static float applyMaxSpeed(float value, float maxSpeed);
    static float applyAcceleration(float target, float accel, float decel,
                                    float dt, float& stored);
private:
    float m_smoothedX = 0.0f, m_smoothedY = 0.0f;
    float m_storedSpeedX = 0.0f, m_storedSpeedY = 0.0f;
};

// =============================================================================
// AimAssist — sits between ResponseCurve output and Camera.update (controller
// path ONLY). It modulates the player's angular velocity (slowdown/suction)
// and overlays a rotational tracking term that follows the target's motion.
// Camera is untouched (still just consumes angular velocity); ResponseCurve is
// untouched; Input/Target/UI are accessed only via the primitives passed in.
// Mouse path (Trainer::updateWithMouse) never calls this — no AA on mouse.
//
// Sign conventions (verified against Camera.cpp:25-26 and Trainer::cameraForward):
//   yawSpeed>0  → yaw↑   → turn LEFT  (forward.x = -sin yaw)
//   pitchSpeed>0→ pitch↑ → look UP    (forward.y =  sin pitch)
//   view-right = cross(forward, worldUp(0,1,0))
//   view-up    = cross(right, forward)
// Derived (so target-rightward motion turns the view right = yaw↓):
//   rotYaw   = -(vRight/dist) * gain   (velocity-follow)
//   rotPitch = +(vUp/dist)    * gain
// =============================================================================
class AimAssist {
public:
    struct AAOutput { float yawSpeed = 0.0f; float pitchSpeed = 0.0f; bool inBubble = false; };

    // Applies slowdown + rotational tracking to the player's angular velocity.
    // All inputs are primitives — no Camera/Target objects (M2: stay decoupled).
    AAOutput apply(float inYaw, float inPitch,
                   const glm::vec3& eye, const glm::vec3& forward,
                   const glm::vec3& tgtPos, const glm::vec3& tgtVel,
                   float moveMag, const AimProfile& p, double dt);
    bool isInBubble() const { return m_inBubble; }
private:
    float m_smoothRotYaw   = 0.0f;
    float m_smoothRotPitch = 0.0f;
    bool  m_inBubble       = false;
};

// =============================================================================
// Ray intersection utilities (slab/AABB + sphere + capsule-segment).
// Declared here so both Trainer.cpp and Modes.cpp can call them; defined in
// Trainer.cpp. `forward` must be unit length. AABB boxMin/boxMax are inclusive
// corners. Returns the ray param t>0 of the first hit surface, or -1 (no hit).
// =============================================================================
float raySphereHitT(const glm::vec3& eye, const glm::vec3& fwd,
                    const glm::vec3& center, float radius);
float rayCapsuleHitT(const glm::vec3& eye, const glm::vec3& fwd,
                     const glm::vec3& a, const glm::vec3& b, float radius);
// Slab method. Returns entry t (>0) or -1 if the ray misses / starts inside.
float rayAABBHitT(const glm::vec3& eye, const glm::vec3& fwd,
                  const glm::vec3& boxMin, const glm::vec3& boxMax);

// =============================================================================
// GameMode — abstract controller for one training mode. Owns its target(s),
// motion, hit-test and scoring. Each frame it advances its state and fills a
// SceneView for the Renderer. Camera / ResponseCurve / AimAssist / Input /
// Renderer are NOT passed in (decoupling per CLAUDE.md); only primitives and
// the SceneView/ModeScore out-params cross the boundary.
//
// The active mode is constructed with a const AppConfig& (snapshot of the
// user's mode-geometry settings); UI edits flow through Trainer::applyConfig
// which rebuilds the mode with the new config.
// =============================================================================
class GameMode {
public:
    virtual ~GameMode() = default;
    virtual GameModeId id() const = 0;
    // Called once when the mode is constructed / config changes — set up
    // initial target layout, cover layout, grid positions, etc.
    virtual void onEnter() = 0;
    // Begin a fresh round: clear scores for this round, reset target state to
    // starting positions. Called by RoundManager on Idle→Countdown transition.
    virtual void resetRound() = 0;
    // Advance one frame. eye/fwd are the camera's view (for aim/fire tests).
    // Fills `outView` with what to render this frame. Returns whether the
    // player is currently "on target" (aimed) — used by tracking-family scoring
    // and AA target selection. For shooting-family modes this returns false
    // (hits come via onFire, not per-frame tracking).
    virtual bool update(double dt, const glm::vec3& eye, const glm::vec3& fwd,
                        SceneView& outView) = 0;
    // Fire event (LMB / R2 edge). Returns whether a target was hit. Default =
    // no-op (tracking family never fires). Only shooting-family overrides this.
    virtual bool onFire(const glm::vec3& eye, const glm::vec3& fwd) {
        (void)eye; (void)fwd; return false;
    }
    // The target the aim-assist should track (controller path). {0,0,0} with
    // valid=false means "no AA target this frame" (e.g. shooting-family can
    // return the nearest visible sphere, or disable AA). velocity is m/s.
    virtual bool getAATarget(glm::vec3& outPos, glm::vec3& outVel) const {
        (void)outPos; (void)outVel; return false;
    }
    // Solid colliders the player can't walk through (e.g. cover boxes). Default
    // = none. Trainer::updatePlayer resolves player-vs-AABB collisions against
    // the list the active mode returns here. Only cover-style modes override.
    virtual void getCollisionAABBs(std::vector<AABB>& out) const { (void)out; }
    virtual const ModeScore& score() const = 0;
    // Called once by RoundManager when a round ends (Playing→Finished) so the
    // mode can bank its best-of-rounds record.
    virtual void onRoundEnd() {}
    // Reset the round-best tracking (e.g. on full reset). roundTotal is reset
    // each round by resetRound(); best* persists across rounds within a mode.
    virtual bool isTrackingFamily() const { return true; }
    // Whether R2/RT fires in this mode (shooting family). False → RT stays ADS.
    virtual bool rtIsFire() const { return false; }
    // Live-apply a config change (UI slider edit) WITHOUT resetting the round /
    // scores. Default = onEnter (which resets; subclasses override to update
    // their snapshot + re-derive geometry while keeping motion state + scores).
    virtual void applyLiveConfig(const AppConfig& config) {
        (void)config;
        onEnter();
    }
};

// Factory that constructs the concrete GameMode subclass for `id`, snapshotting
// `config`. Defined in Modes.cpp (which has the full subclass definitions); this
// keeps Trainer.cpp from needing those definitions (it only holds the abstract
// base via unique_ptr). Returns null on an invalid id.
std::unique_ptr<GameMode> makeGameMode(GameModeId id, const AppConfig& config);

// =============================================================================
// RoundManager — Idle / Countdown / Playing / Finished state machine. Owned
// by Trainer. App signals start requests (LMB/R2 edge); the manager drives
// phase transitions and the per-round timer. Defined in Trainer.cpp.
// =============================================================================
class RoundManager {
public:
    RoundPhase phase         = RoundPhase::Idle;
    double     phaseTimer    = 0.0;   // countdown remaining, or playing remaining
    int        countdownDisplay = 0;  // 3/2/1 shown to the UI (0 during non-countdown)
    bool       startRequested  = false;
    float      roundDuration = 60.0f;
    float      countdownSec   = 3.0f;

    void requestStart();
    // Advance the FSM one frame; calls mode.resetRound()/onRoundEnd() on
    // transitions. Returns the current phase.
    RoundPhase update(double dt, GameMode& mode);
    double playingRemaining() const;  // seconds left (only meaningful in Playing)
};

// =============================================================================
// Trainer — Scene orchestrator (no OpenGL/Input/UI)
// =============================================================================
class Trainer {
public:
    Trainer();
    void update(const StickState& input, double dt);
    // Mouse path: inject delta into response curve pipeline
    void updateWithMouse(const StickState& input, float mouseDX, float mouseDY,
                         float sensX, float sensY, float adsMultiplier, double dt);

    // Player physics — called by App even when UI is active
    void updatePlayer(const StickState& input, double dt);

    const CameraState& getCameraState()   const { return m_camera.getState(); }
    const AimProfile&  getProfile()        const { return m_profile.getCurrent(); }
    void               setProfile(const AimProfile& p) { m_profile.setCurrent(p); }
    Profile&           getProfileManager() { return m_profile; }
    const PlayerState& getPlayerState()    const { return m_player; }
    void reset();

    // ---- Game mode / round ----
    // setMode rebuilds the active mode subclass from `config` (the modes copy
    // the config snapshot they need at construction; call applyConfig to push
    // later edits without changing the mode). App calls these with its Config.
    void setMode(GameModeId id, const AppConfig& config);
    void applyConfig(const AppConfig& config);
    GameModeId getModeId() const { return m_modeId; }
    const RoundManager& getRound() const { return m_round; }
    void requestRoundStart();          // App: LMB/R2 edge → start/restart
    void fire();                       // App: LMB/R2 edge → shoot (if mode fires)
    const SceneView&   getSceneView()  const { return m_sceneView; }
    const ModeScore&   getScore()       const;  // delegates to active mode
    bool               lastAimed()     const { return m_lastAimed; }

    void cameraForward(float& fx, float& fy, float& fz) const;
    // Whether the aim-assist bubble is currently engaging the target (controller
    // path). UI status line reads this.
    bool isAABubbleActive() const { return m_aimAssist.isInBubble(); }

    // Advance the round FSM + active mode one frame; fills the SceneView and
    // sets lastAimed. Called by App when the params panel is open (so the round
    // timer + scene keep ticking) — the controller/mouse update() paths call it
    // internally too.
    void advanceRoundAndMode(double dt);

    // Player physics constants (tweakable)
    float playerMoveSpeed   = 6.0f;   // m/s
    float playerJumpSpeed   = 5.0f;   // m/s (upward)
    float playerGravity     = -15.0f; // m/s²
    float eyeHeight         = 1.7f;   // camera height above feet
private:
    Camera        m_camera;
    ResponseCurve m_responseCurve;
    AimAssist     m_aimAssist;
    Profile       m_profile;
    bool          m_adsActive = false;
    PlayerState   m_player;

    // Active mode + round + render state
    std::unique_ptr<GameMode> m_mode;
    RoundManager m_round;
    SceneView    m_sceneView;
    GameModeId   m_modeId = GameModeId::Tracking;
    bool         m_lastAimed = false;
    AppConfig    m_config;   // last config snapshot the mode was built with

    // Build the concrete mode subclass for `m_modeId` using `m_config`.
    void buildMode();
};

// =============================================================================
// Shader — GLSL compilation/management
// =============================================================================
class Shader {
public:
    Shader() = default;
    ~Shader();
    bool compile(const char* vertexSource, const char* fragmentSource);
    void bind() const;
    void unbind() const;
    unsigned int id() const { return m_id; }

    int  getUniformLocation(const char* name) const;
    void setUniform1i(const char* name, int value);
    void setUniform1f(const char* name, float value);
    void setUniform3f(const char* name, float x, float y, float z);
    void setUniformMatrix4fv(const char* name, const glm::mat4& mat);
private:
    unsigned int m_id = 0;
};

// =============================================================================
// Renderer — OpenGL 3.3 Core rendering pipeline
// =============================================================================
class Renderer {
public:
    Renderer();
    ~Renderer();
    bool init(int w, int h);
    void shutdown();
    // Draws the room, the targets + covers from `view`, the player feet marker,
    // and the screen-space crosshair. Target/cover colors and positions come
    // from the active GameMode's SceneView (per-object uModel + uBaseColor).
    void draw(const CameraState& camera, const PlayerState& player, const SceneView& view);
    void setWindowSize(int w, int h) { m_width = w; m_height = h; }
private:
    int m_width = 1280, m_height = 720;
    Shader m_sceneShader, m_crosshairShader;

    // Room: 6 quad VAOs (floor, ceiling, 4 walls)
    unsigned int m_floorVAO=0, m_floorVBO=0, m_floorIBO=0;
    int m_floorIndexCount = 0;
    unsigned int m_ceilVAO=0, m_ceilVBO=0, m_ceilIBO=0;
    int m_ceilIndexCount = 0;
    unsigned int m_wallNZVAO=0, m_wallNZVBO=0, m_wallNZIBO=0;
    int m_wallNZIndexCount = 0;
    unsigned int m_wallPZVAO=0, m_wallPZVBO=0, m_wallPZIBO=0;
    int m_wallPZIndexCount = 0;
    unsigned int m_wallXVAO=0, m_wallXVBO=0, m_wallXIBO=0;
    int m_wallXIndexCount = 0;
    unsigned int m_wallX2VAO=0, m_wallX2VBO=0, m_wallX2IBO=0;
    int m_wallX2IndexCount = 0;

    unsigned int m_sphereVAO=0, m_sphereVBO=0, m_sphereIBO=0;
    int m_sphereIndexCount = 0;

    // Capsule: unit cylinder (r=1, h=1) + 2 hemispheres. Scaled at runtime.
    unsigned int m_capsuleVAO=0, m_capsuleVBO=0, m_capsuleIBO=0;
    int m_capsuleIndexCount = 0;

    // Cover box: unit cube (-0.5..0.5), scaled per cover by halfExtents.
    unsigned int m_boxVAO=0, m_boxVBO=0, m_boxIBO=0;
    int m_boxIndexCount = 0;

    unsigned int m_crosshairVAO=0, m_crosshairVBO=0;

    void buildRoom();
    void buildSphere(int latSegments, int lonSegments);
    void buildCapsule(int segments, int hemiRings);
    void buildBox();
    void buildCrosshair();
    glm::mat4 computeViewMatrix(const CameraState& cam) const;
    glm::mat4 computeProjectionMatrix() const;
};

// =============================================================================
// UI — Dear ImGui real-time parameter controls
// =============================================================================
class UI {
public:
    UI();
    // Render the score overlay + parameter panel. Mode/round/score state drives
    // the overlay; `config` is bound (by ref) to per-mode parameter sliders.
    // `modeSwitch` is written by the Game Mode combo when the user picks a new
    // mode (App applies it after render). `showParams` toggles the panel (ESC).
    void render(AimProfile& profile, const std::vector<std::string>& profiles,
                int& currentIndex, Profile& profileManager, bool& quitRequested,
                InputMode inputMode, float& mouseSensX, float& mouseSensY,
                AppConfig& config,
                GameModeId modeId, RoundPhase phase, int countdown,
                double roundTimeLeft, const ModeScore& score, bool aaActive,
                GameModeId& modeSwitch, bool showParams);
    bool wantsInput() const { return m_wantsInput; }
private:
    bool m_wantsInput = false;
};

// =============================================================================
// App — SDL2 + OpenGL 3.3 + ImGui init, main loop, orchestration
// =============================================================================
class App {
public:
    App();
    ~App();
    void run();
private:
    void pollEvents();
    void renderFrame();
    void frameTiming();
    void applyConfig();      // push m_config → target/mouse sensitivity
    void syncConfigFromUI(); // pull target/mouse values → m_config + mark dirty

    SDL_Window*   m_window    = nullptr;
    SDL_GLContext m_glContext = nullptr;
    bool          m_running   = true;

    Input    m_input;
    Trainer  m_trainer;
    Renderer m_renderer;
    UI       m_ui;
    Config   m_config;
    AppConfig m_lastAppliedConfig;  // snapshot of config the active mode was built from; used to detect UI edits that need a live re-apply

    // Parameter panel visibility (ESC toggles). When visible, the mouse is
    // released for clicking UI; when hidden, the mouse is captured for aiming.
    bool      m_paramsVisible = false;
    bool      m_escWasDown = false;     // edge detect for ESC

    // Mouse aiming state
    InputMode m_inputMode = InputMode::Mouse;
    float     m_mouseSensitivityX = 0.15f;
    float     m_mouseSensitivityY = 0.15f;
    bool      m_mouseGrab = true;    // starts grabbed (mouse mode); G toggles
    bool      m_gKeyWasDown = false; // edge detect for G
    bool      m_lmbDown = false;     // LMB held: ADS in mouse mode
    bool      m_lmbWasDown = false;  // edge detect for LMB
    bool      m_controllerPrev = false; // edge detect for auto mode switch
    int       m_lastMouseX = 0, m_lastMouseY = 0;

    // Fire / round-start edge detection (LMB + R2/RT). Set in pollEvents / the
    // main loop; consumed in the loop to call Trainer::fire / requestRoundStart.
    bool      m_fireRequested  = false;
    bool      m_startRequested = false;
    bool      m_rtWasDown = false;   // edge detect for R2/RT (analog > 0.5)

    uint64_t m_lastTime   = 0;
    double   m_deltaTime  = 0.0;
    uint64_t m_frameCount = 0;
    double   m_fpsAccum   = 0.0;
    double   m_currentFPS = 0.0;

    static constexpr double  TARGET_DT = 1.0 / 144.0;   // 144 FPS cap
    static constexpr uint64_t FPS_REPORT_INTERVAL = 144;

    void initSDL();
    void initOpenGL();
    void initImGui();
    void initModules();
};
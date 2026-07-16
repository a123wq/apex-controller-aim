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
    const TargetState& getTargetState()    const { return m_target.getState(); }
    Target&            getTarget()               { return m_target; }
    const AimProfile&  getProfile()        const { return m_profile.getCurrent(); }
    void               setProfile(const AimProfile& p) { m_profile.setCurrent(p); }
    Profile&           getProfileManager() { return m_profile; }
    const PlayerState& getPlayerState()    const { return m_player; }
    void reset();

    // Aim-hit test: does the camera's forward ray pass within the sphere's radius?
    bool isAimingAtTarget() const;
    void cameraForward(float& fx, float& fy, float& fz) const;

    // ---- Scoring system ----
    // Called every frame with whether the crosshair is currently on target
    // and the frame delta. Awards 1 point/frame while aimed; if the score
    // stays unchanged for 5s the current run resets to 0 and the best is kept.
    void updateScore(bool aimed, double dt);
    int  getCurrentScore() const { return m_currentScore; }
    int  getBestScore()    const { return m_bestScore; }

    // Player physics constants (tweakable)
    float playerMoveSpeed   = 6.0f;   // m/s
    float playerJumpSpeed   = 5.0f;   // m/s (upward)
    float playerGravity     = -15.0f; // m/s²
    float eyeHeight         = 1.7f;   // camera height above feet
private:
    Camera        m_camera;
    Target        m_target;
    ResponseCurve m_responseCurve;
    Profile       m_profile;
    bool          m_adsActive = false;
    PlayerState   m_player;

    // Scoring state
    int   m_currentScore = 0;
    int   m_bestScore    = 0;
    double m_idleTimer   = 0.0;   // seconds since the score last changed
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
    void draw(const CameraState& camera, const TargetState& target, const PlayerState& player, bool aimed, TargetModel model, float capsuleHeight);
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

    unsigned int m_crosshairVAO=0, m_crosshairVBO=0;

    void buildRoom();
    void buildSphere(int latSegments, int lonSegments);
    void buildCapsule(int segments, int hemiRings);
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
    void render(AimProfile& profile, const std::vector<std::string>& profiles,
                int& currentIndex, Profile& profileManager, bool& quitRequested,
                InputMode inputMode, float& mouseSensX, float& mouseSensY,
                Target& target, bool aimedAtTarget,
                int currentScore, int bestScore, bool showParams);
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
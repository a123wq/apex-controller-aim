#include "App.hpp"
#include <glm/geometric.hpp>
#include <cmath>
#include <cstdlib>

// =============================================================================
// Modes.cpp — concrete GameMode subclasses (one per training mode).
//
// Each mode owns its target state + scoring, advances motion each frame, fills a
// SceneView for the Renderer, and answers aim/fire hit-tests via the shared ray
// utilities in Trainer.cpp (raySphereHitT / rayCapsuleHitT / rayAABBHitT).
//
// Config is snapshotted at construction (const AppConfig&). Trainer::applyConfig
// rebuilds the mode to pick up UI edits. All tunable geometry/motion comes from
// AppConfig (no magic numbers per CLAUDE.md).
// =============================================================================

namespace {
// Deterministic-ish RNG (std::rand, consistent with Target.cpp's usage).
float rndUnit() { return (std::rand() / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f; }
float rndRange(float lo, float hi) {
    return lo + (std::rand() / static_cast<float>(RAND_MAX)) * (hi - lo);
}

// Room extents — must match Renderer's ROOM_W / ROOM_D / ROOM_H.
constexpr float ROOM_W = 10.0f;
constexpr float ROOM_D = 15.0f;
constexpr float ROOM_H = 8.0f;

// Aim color convention (matches the original single-target colors):
//   on target → white, off target → peach.
constexpr float COL_AIMED[3]   = { 1.00f, 1.00f, 1.00f };
constexpr float COL_TRACKING[3] = { 1.00f, 0.65f, 0.55f };
// Grid shooting-family visible sphere color (warm orange, easy to spot).
constexpr float COL_GRID[3] = { 1.00f, 0.55f, 0.10f };
// Pillar patrol target — greyscale per the user request ("整体改成灰色"):
//   on target → near-white, off target → mid grey. Still gives aim feedback.
constexpr float COL_PILLAR_AIMED[3]   = { 0.92f, 0.92f, 0.92f };
constexpr float COL_PILLAR_TRACKING[3] = { 0.45f, 0.45f, 0.45f };

// Translate the strafe speed mode + raw [lo,hi] into the effective [lo,hi] the
// target picks from: Walk = slow half, Run = fast half, Random = full range.
void strafeRangeFor(int mode, float lo, float hi, float& outLo, float& outHi) {
    float mid = (lo + hi) * 0.5f;
    auto m = static_cast<StrafeSpeedMode>(mode);
    if (m == StrafeSpeedMode::Walk) { outLo = lo;        outHi = mid; }
    else if (m == StrafeSpeedMode::Run) { outLo = mid;   outHi = hi;  }
    else { outLo = lo; outHi = hi; }   // Random
}

// =============================================================================
// LateralTarget — a target that stays ON A FIXED LINE: standing on the ground
// at a locked Z depth, strafing left/right along X only, optionally hopping.
// This is what the spec wants for Tracking/Cover ("立在一条横线上左右横移、
// 跳跃"), NOT the old Target class which wandered the whole room on both X and
// Z (and could drift out of view / "vanish" when it wandered near/behind the
// camera).
//
// Motion model:
//  - Z is locked to `baseZ`; X moves at `vx` in ±dir, reversing at ±boundX.
//  - On each redirect window (waitTime) the speed is re-rolled from [minX,maxX]
//    (filtered by the strafe mode) and the direction re-randomized.
//  - Hop (optional): vertical velocity with gravity + restitution; re-hops on
//    landing only while `hopping` is enabled (fully controllable, unlike the
//    Target class's forced re-hop kick).
// =============================================================================
class LateralTarget {
public:
    glm::vec3 pos    = glm::vec3(0.0f);
    float     radius = 0.45f;
    TargetModel model = TargetModel::Sphere;
    float     capsuleHeight = 1.6f;

    // Config (pushed by the mode each reset).
    float minX = 2.0f, maxX = 6.0f;   // speed range
    float boundX = 8.0f;              // lateral half-range
    float baseZ = -10.0f;             // locked depth
    float hopVel = 4.0f;              // initial upward hop velocity
    float gravity = -15.0f;
    float restitution = 0.5f;
    bool  hopping = true;
    float waitMin = 0.6f, waitMax = 1.4f;

    // Live state.
    float vx = 0.0f;        // horizontal (X) velocity m/s
    float vy = 0.0f;        // vertical velocity m/s
    bool  grounded = true;
    float redirectTimer = 0.0f;

    void reset() {
        pos = glm::vec3(0.0f, halfHeight(), baseZ);
        vx = 0.0f;
        vy = 0.0f;
        grounded = true;
        rollRedirect();
    }

    glm::vec3 velocity() const { return glm::vec3(vx, vy, 0.0f); }

    void update(double dt) {
        float dtf = static_cast<float>(dt);

        // --- Lateral (X) motion: constant speed until a redirect window elapses
        //     or a boundary is hit. Z never changes (locked line). ---
        pos.x += vx * dtf;
        if (pos.x >  boundX) { pos.x =  boundX; vx = -std::abs(vx); }
        if (pos.x < -boundX) { pos.x = -boundX; vx =  std::abs(vx); }

        redirectTimer -= dtf;
        if (redirectTimer <= 0.0f) rollRedirect();

        // --- Hop physics (only when hopping enabled). ---
        if (hopping) {
            vy += gravity * dtf;
            pos.y += vy * dtf;
            float floorY = halfHeight();
            if (pos.y <= floorY) {
                pos.y = floorY;
                if (vy < 0.0f) {
                    vy = -vy * restitution;
                    // Too-weak bounce → fresh hop so motion stays lively (only
                    // while hopping is on).
                    if (vy < 1.5f) vy = hopVel;
                }
                grounded = (vy <= 0.01f);
            } else {
                grounded = false;
            }
        } else {
            pos.y = halfHeight();
            vy = 0.0f;
            grounded = true;
        }

        pos.z = baseZ;  // never drift on Z — the whole point
    }

private:
    float halfHeight() const {
        return model == TargetModel::Capsule ? capsuleHeight * 0.5f : radius;
    }
    void rollRedirect() {
        float s = rndRange(minX, maxX);
        float dir = (std::rand() & 1) ? 1.0f : -1.0f;
        vx = dir * s;
        redirectTimer = rndRange(waitMin, waitMax);
    }
};
} // namespace

// =============================================================================
// TrackingMode — one target (sphere/capsule) on a locked lateral line: stands
// on the ground, strafes left/right at random speed, optionally hops. Spec:
// "目标在正前方，立在地面上，在一条横线上左右随机速度横移、跳跃".
// Uses LateralTarget (NOT the old Target class, which wandered the whole room).
// =============================================================================
class TrackingMode : public GameMode {
public:
    explicit TrackingMode(const AppConfig& c) : m_cfg(c) { applyConfig(); m_tgt.reset(); }

    GameModeId id() const override { return GameModeId::Tracking; }
    void onEnter() override { applyConfig(); m_tgt.reset(); }
    void applyLiveConfig(const AppConfig& c) override {
        m_cfg = c;
        applyConfig();   // push new radius/speed/bounds/etc. into m_tgt
        // Do NOT reset position or scores — keep the in-progress round alive.
    }

    void resetRound() override {
        applyConfig();
        m_tgt.reset();
        m_score.trackLongestBurst = 0;
        m_score.trackRoundTotal = 0;
        m_currentBurst = 0;
    }

    bool update(double dt, const glm::vec3& eye, const glm::vec3& fwd,
                SceneView& outView) override {
        m_tgt.update(dt);

        // Aim hit-test (sphere or capsule).
        bool aimed;
        if (m_tgt.model == TargetModel::Capsule) {
            float r = m_tgt.radius;
            float straight = std::max(0.0f, m_tgt.capsuleHeight - 2.0f * r);
            float hh = straight * 0.5f;
            glm::vec3 a(m_tgt.pos.x, m_tgt.pos.y - hh, m_tgt.pos.z);
            glm::vec3 b(m_tgt.pos.x, m_tgt.pos.y + hh, m_tgt.pos.z);
            aimed = rayCapsuleHitT(eye, fwd, a, b, r + 0.05f) >= 0.0f;
        } else {
            aimed = raySphereHitT(eye, fwd, m_tgt.pos, m_tgt.radius + 0.05f) >= 0.0f;
        }

        if (aimed) {
            m_currentBurst += 1;
            m_score.trackRoundTotal += 1;
            if (m_currentBurst > m_score.trackLongestBurst)
                m_score.trackLongestBurst = m_currentBurst;
        } else {
            m_currentBurst = 0;
        }

        pushTarget(outView, aimed);
        return aimed;
    }

    bool getAATarget(glm::vec3& outPos, glm::vec3& outVel) const override {
        outPos = m_tgt.pos;
        outVel = m_tgt.velocity();
        return true;
    }
    const ModeScore& score() const override { return m_score; }
    void onRoundEnd() override {
        if (m_score.trackRoundTotal > m_score.trackBestRound)
            m_score.trackBestRound = m_score.trackRoundTotal;
    }

protected:
    AppConfig     m_cfg;
    LateralTarget m_tgt;
    ModeScore     m_score;
    int           m_currentBurst = 0;

    void applyConfig() {
        m_tgt.radius = m_cfg.targetRadius;
        m_tgt.model  = m_cfg.targetModel;
        m_tgt.capsuleHeight = 1.6f;
        float slo, shi;
        strafeRangeFor(m_cfg.strafeSpeedMode, m_cfg.targetMinSpeed, m_cfg.targetMaxSpeed, slo, shi);
        m_tgt.minX = slo;
        m_tgt.maxX = shi;
        m_tgt.boundX = m_cfg.targetBoundX;
        m_tgt.baseZ  = m_cfg.targetBaseZ;
        m_tgt.hopVel = m_cfg.bounceHeight;
        m_tgt.gravity = m_cfg.bounceGravity;
        m_tgt.restitution = m_cfg.bounceRestitution;
        m_tgt.hopping = m_cfg.jumpEnabled;
        m_tgt.waitMin = 0.6f;
        m_tgt.waitMax = 1.4f;
    }
    void pushTarget(SceneView& outView, bool aimed) const {
        RenderTarget rt;
        rt.pos = m_tgt.pos;
        rt.radius = m_tgt.radius;
        rt.model = m_tgt.model;
        rt.capsuleHeight = m_tgt.capsuleHeight;
        const float* col = aimed ? COL_AIMED : COL_TRACKING;
        rt.color[0] = col[0]; rt.color[1] = col[1]; rt.color[2] = col[2];
        outView.targets.push_back(rt);
    }
};

// =============================================================================
// GridModeBase — 10x10 array of spheres in the air; exactly K visible at once.
// Fire hits a visible sphere → it hides and a random hidden one shows. Shared
// by ThreeTarget (K=3) and SixTarget (K=6).
// =============================================================================
class GridModeBase : public GameMode {
public:
    GridModeBase(const AppConfig& c, int visibleK)
        : m_cfg(c), m_visibleK(visibleK) {
        buildGridPositions();
    }

    void onEnter() override { resetRound(); }
    void applyLiveConfig(const AppConfig& c) override {
        m_cfg = c;
        buildGridPositions();   // re-derive sphere positions from new spacing/center
        // Keep current visibility + scores; existing visible indices just move
        // to their new world positions.
    }

    void resetRound() override {
        for (int i = 0; i < GRID_N; ++i) m_visible[i] = false;
        // Reveal m_visibleK random distinct indices.
        int shown = 0;
        while (shown < m_visibleK) {
            int idx = std::rand() % GRID_N;
            if (!m_visible[idx]) { m_visible[idx] = true; ++shown; }
        }
        m_score.shootCurrent = 0;
    }

    bool update(double dt, const glm::vec3& eye, const glm::vec3& fwd,
                SceneView& outView) override {
        (void)dt; (void)eye; (void)fwd;
        // Static grid; only visible spheres render.
        for (int i = 0; i < GRID_N; ++i) {
            if (!m_visible[i]) continue;
            RenderTarget rt;
            rt.pos = m_positions[i];
            rt.radius = m_cfg.gridRadius;
            rt.model = TargetModel::Sphere;
            rt.capsuleHeight = 1.6f;
            rt.color[0] = COL_GRID[0]; rt.color[1] = COL_GRID[1]; rt.color[2] = COL_GRID[2];
            outView.targets.push_back(rt);
        }
        return false;  // shooting family: no per-frame tracking
    }

    bool onFire(const glm::vec3& eye, const glm::vec3& fwd) override {
        // Find the nearest visible sphere the ray hits; hide it + reveal a
        // random OTHER hidden one (keep exactly m_visibleK visible).
        float bestT = 1e30f;
        int bestIdx = -1;
        for (int i = 0; i < GRID_N; ++i) {
            if (!m_visible[i]) continue;
            float t = raySphereHitT(eye, fwd, m_positions[i], m_cfg.gridRadius + 0.05f);
            if (t >= 0.0f && t < bestT) { bestT = t; bestIdx = i; }
        }
        if (bestIdx < 0) return false;

        // Collect ALL currently-hidden indices EXCEPT the one we're about to
        // hide (so we never re-reveal the just-hit sphere). Picking from a
        // filled list guarantees we always find a reveal target → the visible
        // count stays exactly m_visibleK. (The old random-try loop could, in
        // pathological cases, fail to find a hidden index and let the count
        // drift down — the "球体越来越少" bug.)
        m_visible[bestIdx] = false;  // hide the hit sphere first
        int hidden[GRID_N];
        int nHidden = 0;
        for (int i = 0; i < GRID_N; ++i) {
            if (!m_visible[i]) hidden[nHidden++] = i;
        }
        if (nHidden > 0) {
            int pick = hidden[std::rand() % nHidden];
            m_visible[pick] = true;
        }

        m_score.shootCurrent += 1;
        return true;
    }

    bool getAATarget(glm::vec3& outPos, glm::vec3& outVel) const override {
        // Optional: let AA track the nearest visible sphere. For simplicity we
        // disable AA in shooting modes (returns false → no AA target).
        (void)outPos; (void)outVel; return false;
    }
    const ModeScore& score() const override { return m_score; }
    void onRoundEnd() override {
        if (m_score.shootCurrent > m_score.shootBest)
            m_score.shootBest = m_score.shootCurrent;
    }
    bool isTrackingFamily() const override { return false; }
    bool rtIsFire() const override { return true; }

protected:
    AppConfig m_cfg;
    int       m_visibleK;
    ModeScore m_score;
    static constexpr int GRID_N = 100;
    glm::vec3 m_positions[GRID_N];
    bool      m_visible[GRID_N] = {};

    void buildGridPositions() {
        // Rectangular target wall facing the player: all spheres at a FIXED
        // depth Z=gridCenterZ, spread across X and Y (the view plane). This is
        // the "前方空中的矩形阵列" from the spec. The layout is computed to FIT
        // THE CAMERA FRUSTUM at this depth (with margin), so the whole wall is
        // on-screen — no spheres clip outside the room / out of view.
        const int COLS = 20;
        const int ROWS = GRID_N / COLS;  // 5  → 20x5 = 100 (wide-and-short)
        static_assert(COLS * ROWS == GRID_N, "grid layout must total GRID_N");

        // Fit the wall to the camera FRUSTUM at depth |gridCenterZ| AND the room
        // bounds, with a margin, so the whole rectangle stays on-screen and
        // in-room. The vertical FOV is the tight constraint, so the rectangle is
        // wide-and-short (20x5). X fits the frustum (clamped to room width); Y
        // fits the room height band [0.5, H-0.5] (a wall sitting between floor
        // and ceiling, centered on eye height).
        const float depth = std::abs(m_cfg.gridCenterZ);
        const float hFovHalf = glm::radians(75.0f * 0.5f);  // horizontal half-FOV
        const float margin = 0.82f;  // fill 82% of the usable half-extent
        // X usable half-width: frustum limit, clamped to room half-width.
        float frustumHalfW = depth * std::tan(hFovHalf) * margin;
        float usableHalfW = std::min(frustumHalfW, ROOM_W - 0.5f);
        // Y usable half-height: limited by the room height band, centered on
        // eye height but clamped so the wall never crosses the floor/ceiling.
        const float eyeY = 1.7f;
        const float roomHalfH = (ROOM_H - 1.0f) * 0.5f;  // band [0.5, 7.5], half=3.5
        float usableHalfH = std::min(roomHalfH, eyeY - 0.5f); // keep above floor
        float centerY = std::clamp(eyeY, usableHalfH + 0.5f, ROOM_H - usableHalfH - 0.5f);

        // Per-axis spacing so COLS x ROWS fill the usable spans exactly.
        float effSx = (usableHalfW * 2.0f) / static_cast<float>(COLS - 1);
        float effSy = (usableHalfH * 2.0f) / static_cast<float>(ROWS - 1);

        int i = 0;
        for (int row = 0; row < ROWS; ++row) {
            for (int col = 0; col < COLS; ++col) {
                m_positions[i++] = glm::vec3(
                    -usableHalfW + col * effSx,        // X: left→right
                    centerY + usableHalfH - row * effSy, // Y: top→bottom
                    m_cfg.gridCenterZ);               // Z: locked depth
            }
        }
    }
};

class ThreeTargetMode : public GridModeBase {
public:
    explicit ThreeTargetMode(const AppConfig& c) : GridModeBase(c, 3) {}
    GameModeId id() const override { return GameModeId::ThreeTarget; }
};
class SixTargetMode : public GridModeBase {
public:
    explicit SixTargetMode(const AppConfig& c) : GridModeBase(c, 6) {}
    GameModeId id() const override { return GameModeId::SixTarget; }
};

// =============================================================================
// CoverTrackingMode — lateral-line tracking target + cover boxes of varied
// shapes between the player and the target. Covers REALLY occlude the aim ray
// (ray-AABB closer than the target → aim lost) AND are solid (the player
// collides with them, can't walk through). Cover shapes vary per spawn:
//   - standing pillar (tall, thin XZ)
//   - lying pillar  (long along X, short Y/Z — a horizontal beam)
//   - low wall       (wide along X, short Y, thin Z)
// so the layout doesn't look like a row of identical boxes.
// =============================================================================
class CoverTrackingMode : public GameMode {
public:
    explicit CoverTrackingMode(const AppConfig& c) : m_cfg(c) {
        applyConfig();
        m_tgt.reset();
        buildCovers();
    }
    GameModeId id() const override { return GameModeId::CoverTracking; }
    void onEnter() override { applyConfig(); m_tgt.reset(); buildCovers(); }
    void applyLiveConfig(const AppConfig& c) override {
        m_cfg = c;
        applyConfig();    // push target radius/speed/bounds into m_tgt
        buildCovers();    // re-derive cover shapes/positions from new config
        // Keep m_tgt motion state + scores intact (no reset).
    }

    void resetRound() override {
        applyConfig();
        m_tgt.reset();
        buildCovers();  // re-randomize cover layout each round
        m_score.trackLongestBurst = 0;
        m_score.trackRoundTotal = 0;
        m_currentBurst = 0;
    }

    bool update(double dt, const glm::vec3& eye, const glm::vec3& fwd,
                SceneView& outView) override {
        m_tgt.update(dt);

        // Base aim hit-test.
        float targetT;
        if (m_tgt.model == TargetModel::Capsule) {
            float r = m_tgt.radius;
            float straight = std::max(0.0f, m_tgt.capsuleHeight - 2.0f * r);
            float hh = straight * 0.5f;
            glm::vec3 a(m_tgt.pos.x, m_tgt.pos.y - hh, m_tgt.pos.z);
            glm::vec3 b(m_tgt.pos.x, m_tgt.pos.y + hh, m_tgt.pos.z);
            targetT = rayCapsuleHitT(eye, fwd, a, b, r + 0.05f);
        } else {
            targetT = raySphereHitT(eye, fwd, m_tgt.pos, m_tgt.radius + 0.05f);
        }
        bool aimed = (targetT >= 0.0f);

        // Occlusion: a cover box hit closer than the target blocks the aim.
        if (aimed) {
            for (const AABB& box : m_aabbs) {
                float tBox = rayAABBHitT(eye, fwd, box.min, box.max);
                if (tBox >= 0.0f && tBox < targetT) { aimed = false; break; }
            }
        }

        if (aimed) {
            m_currentBurst += 1;
            m_score.trackRoundTotal += 1;
            if (m_currentBurst > m_score.trackLongestBurst)
                m_score.trackLongestBurst = m_currentBurst;
        } else {
            m_currentBurst = 0;
        }

        // Render target (white when aimed, peach when not) + covers.
        RenderTarget rt;
        rt.pos = m_tgt.pos;
        rt.radius = m_tgt.radius;
        rt.model = m_tgt.model;
        rt.capsuleHeight = m_tgt.capsuleHeight;
        const float* col = aimed ? COL_AIMED : COL_TRACKING;
        rt.color[0] = col[0]; rt.color[1] = col[1]; rt.color[2] = col[2];
        outView.targets.push_back(rt);
        for (const RenderCover& cov : m_covers) outView.covers.push_back(cov);
        return aimed;
    }

    bool getAATarget(glm::vec3& outPos, glm::vec3& outVel) const override {
        outPos = m_tgt.pos;
        outVel = m_tgt.velocity();
        return true;
    }
    void getCollisionAABBs(std::vector<AABB>& out) const override {
        out.insert(out.end(), m_aabbs.begin(), m_aabbs.end());
    }
    const ModeScore& score() const override { return m_score; }
    void onRoundEnd() override {
        if (m_score.trackRoundTotal > m_score.trackBestRound)
            m_score.trackBestRound = m_score.trackRoundTotal;
    }

private:
    AppConfig     m_cfg;
    LateralTarget m_tgt;
    ModeScore     m_score;
    int           m_currentBurst = 0;
    std::vector<RenderCover> m_covers;
    std::vector<AABB>        m_aabbs;  // mirrors m_covers, for collision + occlusion

    void applyConfig() {
        m_tgt.radius = m_cfg.targetRadius;
        m_tgt.model  = m_cfg.targetModel;
        m_tgt.capsuleHeight = 1.6f;
        float slo, shi;
        strafeRangeFor(m_cfg.strafeSpeedMode, m_cfg.targetMinSpeed, m_cfg.targetMaxSpeed, slo, shi);
        m_tgt.minX = slo;
        m_tgt.maxX = shi;
        m_tgt.boundX = m_cfg.targetBoundX;
        m_tgt.baseZ  = m_cfg.targetBaseZ;
        m_tgt.hopVel = m_cfg.bounceHeight;
        m_tgt.gravity = m_cfg.bounceGravity;
        m_tgt.restitution = m_cfg.bounceRestitution;
        m_tgt.hopping = m_cfg.jumpEnabled;
        m_tgt.waitMin = 0.6f;
        m_tgt.waitMax = 1.4f;
    }

    void buildCovers() {
        m_covers.clear();
        m_aabbs.clear();
        int n = std::max(1, std::min(m_cfg.coverCount, 6));
        // Depth range: between just ahead of the player (z≈-2) and just in front
        // of the target (z≈baseZ+1.5). Spread the covers across it.
        float zNear = -2.0f;
        float zFar  = m_cfg.targetBaseZ + 1.5f;
        for (int i = 0; i < n; ++i) {
            RenderCover c;
            float zFrac = (n == 1) ? 0.5f : static_cast<float>(i) / static_cast<float>(n - 1);
            float cz = zNear + (zFar - zNear) * zFrac;

            // Shape variety, sized to actually occlude a target near eye height
            // (1.7m). Each base size is derived from coverHalf*, but floored to
            // a minimum so covers are big enough to block the aim line.
            int shape = i % 3;
            float hx = m_cfg.coverHalfX, hy = m_cfg.coverHalfY, hz = m_cfg.coverHalfZ;
            if (shape == 0) {
                // Standing pillar: tall (reaches above the target), modest width.
                c.halfExtents = glm::vec3(std::max(0.4f, hx * 0.8f),
                                          std::max(2.0f, hy * 1.4f),
                                          std::max(0.4f, hz));
            } else if (shape == 1) {
                // Lying pillar / horizontal beam at upper-body height: long along
                // X, blocks the aim line side-to-side.
                c.halfExtents = glm::vec3(std::max(2.5f, hx * 1.8f),
                                          std::max(0.5f, hz),
                                          std::max(0.4f, hz));
            } else {
                // Low wall / crate: a solid block wide enough to duck behind.
                c.halfExtents = glm::vec3(std::max(2.0f, hx * 1.4f),
                                          std::max(1.2f, hy * 0.7f),
                                          std::max(0.4f, hz));
            }

            // Position: depth cz, X offset so each cover partly overlaps the
            // center aim line but the set leaves gaps (alternate sides).
            float xOff = (i % 2 == 0) ? -c.halfExtents.x * 0.5f : c.halfExtents.x * 0.5f;
            xOff += rndRange(-1.0f, 1.0f);
            c.center = glm::vec3(xOff, 0.0f, cz);

            if (shape == 1) {
                // Beam: elevate so it straddles eye height (center ~1.6m).
                c.center.y = 1.6f;
            } else {
                // Pillar / wall: base on the floor (center = half height).
                c.center.y = c.halfExtents.y;
            }
            m_covers.push_back(c);
            m_aabbs.push_back(AABB{ c.center - c.halfExtents, c.center + c.halfExtents });
        }
    }
};

// =============================================================================
// PillarPatrolMode — tall-thin capsule moving at CONSTANT horizontal speed,
// reversing only at ±boundX boundaries. No random redirects, weak/no bounce.
// =============================================================================
class PillarPatrolMode : public GameMode {
public:
    explicit PillarPatrolMode(const AppConfig& c) : m_cfg(c) {
        m_pos = glm::vec3(0.0f, m_cfg.pillarHeight * 0.5f, m_cfg.targetBaseZ);
        m_dir = 1.0f;  // start moving +X
    }
    GameModeId id() const override { return GameModeId::PillarPatrol; }
    void onEnter() override { resetRound(); }
    void applyLiveConfig(const AppConfig& c) override {
        m_cfg = c;
        // Motion reads m_cfg directly each frame (pillarSpeed, pillarHeight,
        // pillarRadius, targetBaseZ, targetBoundX), so just updating the
        // snapshot is enough — keep position/dir/scores intact.
    }

    void resetRound() override {
        m_pos = glm::vec3(0.0f, m_cfg.pillarHeight * 0.5f, m_cfg.targetBaseZ);
        m_dir = 1.0f;
        m_score.trackLongestBurst = 0;
        m_score.trackRoundTotal = 0;
        m_currentBurst = 0;
    }

    bool update(double dt, const glm::vec3& eye, const glm::vec3& fwd,
                SceneView& outView) override {
        float dtf = static_cast<float>(dt);
        // Constant-speed horizontal patrol; reverse at ±boundX.
        m_pos.x += m_dir * m_cfg.pillarSpeed * dtf;
        float bx = m_cfg.targetBoundX;
        if (m_pos.x >  bx) { m_pos.x =  bx; m_dir = -1.0f; }
        if (m_pos.x < -bx) { m_pos.x = -bx; m_dir =  1.0f; }
        // Keep the capsule standing on the ground (center = half height).
        m_pos.y = m_cfg.pillarHeight * 0.5f;

        // Aim hit-test vs the capsule's vertical segment.
        float r = m_cfg.pillarRadius;
        float straight = std::max(0.0f, m_cfg.pillarHeight - 2.0f * r);
        float hh = straight * 0.5f;
        glm::vec3 a(m_pos.x, m_pos.y - hh, m_pos.z);
        glm::vec3 b(m_pos.x, m_pos.y + hh, m_pos.z);
        bool aimed = rayCapsuleHitT(eye, fwd, a, b, r + 0.05f) >= 0.0f;

        if (aimed) {
            m_currentBurst += 1;
            m_score.trackRoundTotal += 1;
            if (m_currentBurst > m_score.trackLongestBurst)
                m_score.trackLongestBurst = m_currentBurst;
        } else {
            m_currentBurst = 0;
        }

        RenderTarget rt;
        rt.pos = m_pos;
        rt.radius = r;
        rt.model = TargetModel::Capsule;
        rt.capsuleHeight = m_cfg.pillarHeight;
        // Pillar uses a greyscale palette (per the user request "整体改成灰色"):
        // near-white when on target, mid grey when off. Still gives aim feedback
        // without the colored look of the tracking spheres.
        const float* col = aimed ? COL_PILLAR_AIMED : COL_PILLAR_TRACKING;
        rt.color[0] = col[0]; rt.color[1] = col[1]; rt.color[2] = col[2];
        outView.targets.push_back(rt);
        return aimed;
    }

    bool getAATarget(glm::vec3& outPos, glm::vec3& outVel) const override {
        outPos = m_pos;
        outVel = glm::vec3(m_dir * m_cfg.pillarSpeed, 0.0f, 0.0f);
        return true;
    }
    const ModeScore& score() const override { return m_score; }
    void onRoundEnd() override {
        if (m_score.trackRoundTotal > m_score.trackBestRound)
            m_score.trackBestRound = m_score.trackRoundTotal;
    }

private:
    AppConfig m_cfg;
    glm::vec3 m_pos;
    float     m_dir;          // +1 / -1
    ModeScore m_score;
    int       m_currentBurst = 0;
};

// =============================================================================
// FreeOrbitMode — one sphere moving freely in 3D, no gravity, reflecting off all
// six room walls. Periodic random re-direction keeps motion varied.
// =============================================================================
class FreeOrbitMode : public GameMode {
public:
    explicit FreeOrbitMode(const AppConfig& c) : m_cfg(c) {
        m_pos = glm::vec3(0.0f, 3.0f, m_cfg.targetBaseZ);
        rollVelocity();
    }
    GameModeId id() const override { return GameModeId::FreeOrbit; }
    void onEnter() override { resetRound(); }
    void applyLiveConfig(const AppConfig& c) override {
        m_cfg = c;
        // Radius/bounds read live; speed takes effect on next rollVelocity.
        // Keep position/velocity/scores intact.
    }

    void resetRound() override {
        m_pos = glm::vec3(0.0f, 3.0f, m_cfg.targetBaseZ);
        rollVelocity();
        m_score.trackLongestBurst = 0;
        m_score.trackRoundTotal = 0;
        m_currentBurst = 0;
        m_redirectTimer = rndRange(2.0f, 4.0f);
    }

    bool update(double dt, const glm::vec3& eye, const glm::vec3& fwd,
                SceneView& outView) override {
        float dtf = static_cast<float>(dt);
        // Integrate position.
        m_pos += m_vel * dtf;
        // Reflect off the six walls (leave a radius margin).
        float r = m_cfg.targetRadius;
        float bx = ROOM_W - r;
        float bz = ROOM_D - r;
        float byLo = r, byHi = ROOM_H - r;
        if (m_pos.x >  bx) { m_pos.x =  bx; m_vel.x = -std::abs(m_vel.x); }
        if (m_pos.x < -bx) { m_pos.x = -bx; m_vel.x =  std::abs(m_vel.x); }
        if (m_pos.z >  bz) { m_pos.z =  bz; m_vel.z = -std::abs(m_vel.z); }
        if (m_pos.z < -bz) { m_pos.z = -bz; m_vel.z =  std::abs(m_vel.z); }
        if (m_pos.y >  byHi) { m_pos.y =  byHi; m_vel.y = -std::abs(m_vel.y); }
        if (m_pos.y <  byLo) { m_pos.y =  byLo; m_vel.y =  std::abs(m_vel.y); }
        // Keep the target in the front half (z < 0) so it stays in view.
        if (m_pos.z > -1.0f) { m_pos.z = -1.0f; if (m_vel.z > 0.0f) m_vel.z = -m_vel.z; }

        // Periodic random re-direction.
        m_redirectTimer -= dtf;
        if (m_redirectTimer <= 0.0f) {
            rollVelocity();
            m_redirectTimer = rndRange(2.0f, 4.0f);
        }

        // Aim hit-test (sphere).
        bool aimed = raySphereHitT(eye, fwd, m_pos, r + 0.05f) >= 0.0f;

        if (aimed) {
            m_currentBurst += 1;
            m_score.trackRoundTotal += 1;
            if (m_currentBurst > m_score.trackLongestBurst)
                m_score.trackLongestBurst = m_currentBurst;
        } else {
            m_currentBurst = 0;
        }

        RenderTarget rt;
        rt.pos = m_pos;
        rt.radius = r;
        rt.model = TargetModel::Sphere;
        rt.capsuleHeight = 1.6f;
        const float* col = aimed ? COL_AIMED : COL_TRACKING;
        rt.color[0] = col[0]; rt.color[1] = col[1]; rt.color[2] = col[2];
        outView.targets.push_back(rt);
        return aimed;
    }

    bool getAATarget(glm::vec3& outPos, glm::vec3& outVel) const override {
        outPos = m_pos;
        outVel = m_vel;
        return true;
    }
    const ModeScore& score() const override { return m_score; }
    void onRoundEnd() override {
        if (m_score.trackRoundTotal > m_score.trackBestRound)
            m_score.trackBestRound = m_score.trackRoundTotal;
    }

private:
    AppConfig m_cfg;
    glm::vec3 m_pos;
    glm::vec3 m_vel;
    ModeScore m_score;
    int       m_currentBurst = 0;
    float     m_redirectTimer = 3.0f;

    void rollVelocity() {
        // Random unit direction * freeOrbitSpeed, biased toward lateral/forward.
        for (int attempt = 0; attempt < 8; ++attempt) {
            float x = rndUnit(), y = rndUnit() * 0.5f, z = rndUnit();
            float len = std::sqrt(x * x + y * y + z * z);
            if (len > 0.01f) {
                m_vel = glm::vec3(x, y, z) / len * m_cfg.freeOrbitSpeed;
                // Bias to move away from the camera (−Z) so it starts in view.
                if (m_vel.z > 0.0f) m_vel.z = -m_vel.z;
                return;
            }
        }
        m_vel = glm::vec3(0.0f, 0.0f, -m_cfg.freeOrbitSpeed);
    }
};

// =============================================================================
// Factory — constructs the concrete GameMode subclass for `id`. Defined here
// (where the subclass definitions are visible) so Trainer.cpp only needs the
// abstract base + this declaration.
// =============================================================================
std::unique_ptr<GameMode> makeGameMode(GameModeId id, const AppConfig& config) {
    switch (id) {
        case GameModeId::Tracking:      return std::make_unique<TrackingMode>(config);
        case GameModeId::ThreeTarget:   return std::make_unique<ThreeTargetMode>(config);
        case GameModeId::SixTarget:     return std::make_unique<SixTargetMode>(config);
        case GameModeId::CoverTracking: return std::make_unique<CoverTrackingMode>(config);
        case GameModeId::PillarPatrol:  return std::make_unique<PillarPatrolMode>(config);
        case GameModeId::FreeOrbit:     return std::make_unique<FreeOrbitMode>(config);
    }
    return nullptr;
}

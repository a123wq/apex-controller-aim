// =============================================================================
// Renderer.cpp — Indoor training room (closed white box with directional light)
//
// Room: 20m wide (X: -10..10) × 8m tall (Y: 0..8) × 30m deep (Z: -15..15).
// Camera starts at (0, 1.7, 0) facing +Z.
//
// Draw order:
//   1. Room surfaces: floor + 4 walls + ceiling (white, procedural grid)
//   2. Target sphere (lit)
//   3. Player feet marker
//   4. Crosshair (screen-space overlay)
//
// NOTE: Room surfaces are drawn with GL_CULL_FACE DISABLED because the
// back-faces of the room walls are what we see from inside. The sphere
// keeps culling enabled for proper 3D shading.
// =============================================================================
#include "App.hpp"
#include "gl33.h"
#include <vector>

// =============================================================================
// Scene shader
//   attributes: 0 = position (3f), 1 = normal (3f), 2 = UV (2f)
//   uDrawID: 0 = room surface, 1 = lit object, 2 = player marker
// =============================================================================
static const char* SCENE_VERTEX = R"(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;

uniform mat4 uModel;
uniform mat4 uViewProj;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vWorldPos = world.xyz;
    vNormal   = mat3(uModel) * aNormal;
    vUV       = aUV;
    gl_Position = uViewProj * world;
}
)";

static const char* SCENE_FRAGMENT = R"(
#version 330 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;

uniform int   uDrawID;
uniform vec3  uBaseColor;
uniform vec3  uCamPos;
uniform vec3  uLightDir;     // normalized, points TOWARD the light
uniform vec3  uLightColor;

out vec4 FragColor;

// Procedural grid on any surface given its local UV.
// cellCount = number of cells along the longest UV edge.
// Returns 0 (no line) to 1 (solid line).
float gridLines(vec2 uv, float cellCount) {
    vec2 uvScaled = uv * cellCount;
    vec2 d = fwidth(uvScaled);
    vec2 f = fract(uvScaled - 0.5);
    vec2 g = abs(f - 0.5) / max(d, vec2(0.0001));
    return 1.0 - min(min(g.x, g.y), 1.0);
}

void main() {
    if (uDrawID == 0) {
        // ---- Room surface: white with procedural grid lines ----
        vec3 baseCol = vec3(0.92, 0.92, 0.92);

        // Two grid scales: fine (0.5m) and coarse (2m)
        // UV 0..1 corresponds to the face dimensions,
        // so 20 cells = 0.5m spacing on a 10m-wide face.
        float g1 = gridLines(vUV, 20.0);
        float g2 = gridLines(vUV, 5.0);

        baseCol = mix(baseCol, vec3(0.55, 0.55, 0.55), g1 * 0.35);
        baseCol = mix(baseCol, vec3(0.30, 0.30, 0.30), g2 * 0.60);

        // Lambertian shading
        vec3 N = normalize(vNormal);
        vec3 L = normalize(uLightDir);
        float diff = max(dot(N, L), 0.0);

        // Bright ambient so all surfaces are clearly visible
        vec3 lit = baseCol * (0.40 + diff * 0.60);
        lit *= uLightColor;

        FragColor = vec4(lit, 1.0);
    } else if (uDrawID == 1) {
        // ---- Lit sphere: Lambertian diffuse + ambient + rim ----
        vec3 N = normalize(vNormal);
        vec3 L = normalize(uLightDir);
        float diff = max(dot(N, L), 0.0);

        vec3 V = normalize(uCamPos - vWorldPos);
        float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0);

        vec3 col = uBaseColor * (0.25 + diff * 0.75);
        col += vec3(0.5, 0.6, 1.0) * rim * 0.35;

        FragColor = vec4(col, 1.0);
    } else {
        // ---- Player marker: bright cyan ----
        FragColor = vec4(0.20, 0.75, 1.0, 1.0);
    }
}
)";

// Crosshair shader
static const char* CROSSHAIR_VERTEX = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

static const char* CROSSHAIR_FRAGMENT = R"(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);
}
)";

// =============================================================================
// Room dimensions (world units = metres)
// =============================================================================
static constexpr float ROOM_W = 10.0f;  // half-width   (X: -10..10)
static constexpr float ROOM_H =  8.0f;  // height       (Y:  0.. 8)
static constexpr float ROOM_D = 15.0f;  // half-depth   (Z: -15..15)

// Directional light: from upper-right of room, pointing toward scene center.
// This is the direction FROM the surface TOWARD the light source.
// Normalized: sqrt(0.35² + 0.80² + 0.48²) ≈ 1.0
static const float LIGHT_DIR[3] = { 0.35f, 0.80f, 0.48f };

// =============================================================================
// Upload a quad (4 verts) into a VAO.
// Vertex layout: x,y,z, nx,ny,nz, u,v  (8 floats per vertex)
// =============================================================================
struct QuadVerts { float d[32]; };

static QuadVerts makeQuad(
    float ax, float ay, float az,
    float bx, float by, float bz,
    float cx, float cy, float cz,
    float dx, float dy, float dz,
    float nx, float ny, float nz,
    float u0, float v0, float u1, float v1)
{
    QuadVerts q;
    // BL
    q.d[ 0] = ax; q.d[ 1] = ay; q.d[ 2] = az; q.d[ 3] = nx; q.d[ 4] = ny; q.d[ 5] = nz; q.d[ 6] = u0; q.d[ 7] = v0;
    // BR
    q.d[ 8] = bx; q.d[ 9] = by; q.d[10] = bz; q.d[11] = nx; q.d[12] = ny; q.d[13] = nz; q.d[14] = u1; q.d[15] = v0;
    // TR
    q.d[16] = cx; q.d[17] = cy; q.d[18] = cz; q.d[19] = nx; q.d[20] = ny; q.d[21] = nz; q.d[22] = u1; q.d[23] = v1;
    // TL
    q.d[24] = dx; q.d[25] = dy; q.d[26] = dz; q.d[27] = nx; q.d[28] = ny; q.d[29] = nz; q.d[30] = u0; q.d[31] = v1;
    return q;
}

static const unsigned int QUAD_IDX[6] = { 0, 1, 2, 0, 2, 3 };

static void uploadQuad(unsigned int& vao, unsigned int& vbo, unsigned int& ibo,
                       int& outIndexCount,
                       const float* vertsData, int vertCount)
{
    struct Vertex { float x, y, z, nx, ny, nz, u, v; };
    static_assert(sizeof(Vertex) == 32, "Vertex must be 32 bytes");

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertCount * sizeof(Vertex)), vertsData, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(6 * sizeof(unsigned int)), QUAD_IDX, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);

    outIndexCount = 6;
}

// =============================================================================
Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(int windowWidth, int windowHeight) {
    m_width  = windowWidth;
    m_height = windowHeight;

    if (!m_sceneShader.compile(SCENE_VERTEX, SCENE_FRAGMENT)) return false;
    if (!m_crosshairShader.compile(CROSSHAIR_VERTEX, CROSSHAIR_FRAGMENT)) return false;

    buildRoom();
    buildSphere(16, 24);
    buildCapsule(20, 12);
    buildCrosshair();

    return true;
}

void Renderer::shutdown() {
    auto del = [](unsigned int& id) { if (id) { glDeleteBuffers(1, &id); id = 0; } };
    auto delVAO = [](unsigned int& id) { if (id) { glDeleteVertexArrays(1, &id); id = 0; } };

    delVAO(m_floorVAO);    del(m_floorVBO);    del(m_floorIBO);
    delVAO(m_ceilVAO);     del(m_ceilVBO);     del(m_ceilIBO);
    delVAO(m_wallNZVAO);   del(m_wallNZVBO);   del(m_wallNZIBO);
    delVAO(m_wallPZVAO);   del(m_wallPZVBO);   del(m_wallPZIBO);
    delVAO(m_wallXVAO);    del(m_wallXVBO);    del(m_wallXIBO);
    delVAO(m_wallX2VAO);   del(m_wallX2VBO);   del(m_wallX2IBO);
    delVAO(m_sphereVAO);   del(m_sphereVBO);   del(m_sphereIBO);
    delVAO(m_capsuleVAO);  del(m_capsuleVBO);  del(m_capsuleIBO);
    delVAO(m_crosshairVAO); del(m_crosshairVBO);
}

// =============================================================================
// Room faces. Each face stores world-space position and normal oriented INTO
// the room, with UV in [0,1] x [0,1] for the grid shader.
// =============================================================================
void Renderer::buildRoom() {
    QuadVerts q;

    // ---- Floor: Y=0, normal +Y (points UP into room), UV: X→U, Z→V ----
    q = makeQuad(
        -ROOM_W, 0, -ROOM_D,    // BL
         ROOM_W, 0, -ROOM_D,    // BR
         ROOM_W, 0,  ROOM_D,    // TR
        -ROOM_W, 0,  ROOM_D,    // TL
        0, 1, 0,  0, 0, 1, 1);
    uploadQuad(m_floorVAO, m_floorVBO, m_floorIBO, m_floorIndexCount, q.d, 4);

    // ---- Ceiling: Y=H, normal -Y (points DOWN into room), UV: X→U, Z→V ----
    q = makeQuad(
        -ROOM_W, ROOM_H,  ROOM_D,  // BL (from below, +Z is bottom-left)
         ROOM_W, ROOM_H,  ROOM_D,  // BR
         ROOM_W, ROOM_H, -ROOM_D,  // TR
        -ROOM_W, ROOM_H, -ROOM_D,  // TL
        0, -1, 0,  0, 0, 1, 1);
    uploadQuad(m_ceilVAO, m_ceilVBO, m_ceilIBO, m_ceilIndexCount, q.d, 4);

    // ---- Far wall: Z=-D, normal +Z, UV: X→U, Y→V ----
    q = makeQuad(
        -ROOM_W, 0, -ROOM_D,     // BL
         ROOM_W, 0, -ROOM_D,     // BR
         ROOM_W, ROOM_H, -ROOM_D, // TR
        -ROOM_W, ROOM_H, -ROOM_D, // TL
        0, 0, 1,  0, 0, 1, 1);
    uploadQuad(m_wallNZVAO, m_wallNZVBO, m_wallNZIBO, m_wallNZIndexCount, q.d, 4);

    // ---- Near wall: Z=+D, normal -Z, UV: X→U, Y→V ----
    q = makeQuad(
        -ROOM_W, 0, ROOM_D,     // BL
         ROOM_W, 0, ROOM_D,     // BR
         ROOM_W, ROOM_H, ROOM_D, // TR
        -ROOM_W, ROOM_H, ROOM_D, // TL
        0, 0, -1,  0, 0, 1, 1);
    uploadQuad(m_wallPZVAO, m_wallPZVBO, m_wallPZIBO, m_wallPZIndexCount, q.d, 4);

    // ---- Right wall: X=+W, normal -X, UV: Z→U, Y→V ----
    q = makeQuad(
         ROOM_W, 0, -ROOM_D,     // BL
         ROOM_W, 0,  ROOM_D,     // BR
         ROOM_W, ROOM_H,  ROOM_D, // TR
         ROOM_W, ROOM_H, -ROOM_D, // TL
        -1, 0, 0,  0, 0, 1, 1);
    uploadQuad(m_wallXVAO, m_wallXVBO, m_wallXIBO, m_wallXIndexCount, q.d, 4);

    // ---- Left wall: X=-W, normal +X, UV: Z→U, Y→V ----
    q = makeQuad(
        -ROOM_W, 0, -ROOM_D,     // BL
        -ROOM_W, 0,  ROOM_D,     // BR
        -ROOM_W, ROOM_H,  ROOM_D, // TR
        -ROOM_W, ROOM_H, -ROOM_D, // TL
        1, 0, 0,  0, 0, 1, 1);
    uploadQuad(m_wallX2VAO, m_wallX2VBO, m_wallX2IBO, m_wallX2IndexCount, q.d, 4);
}

// =============================================================================
void Renderer::buildSphere(int latSeg, int lonSeg) {
    struct Vertex { float x, y, z, nx, ny, nz, u, v; };
    const int vc = (latSeg + 1) * (lonSeg + 1);
    const int ic = latSeg * lonSeg * 6;

    auto* verts = new Vertex[vc];
    auto* idxs  = new unsigned int[ic];

    const float PI = 3.14159265f, PI2 = 2.0f * PI;
    int vi = 0;
    for (int lat = 0; lat <= latSeg; ++lat) {
        float theta = static_cast<float>(lat) / static_cast<float>(latSeg) * PI;
        float st = std::sin(theta), ct = std::cos(theta);
        for (int lon = 0; lon <= lonSeg; ++lon) {
            float phi = static_cast<float>(lon) / static_cast<float>(lonSeg) * PI2;
            float x = st * std::cos(phi), y = ct, z = st * std::sin(phi);
            float u = static_cast<float>(lon) / static_cast<float>(lonSeg);
            float v = static_cast<float>(lat) / static_cast<float>(latSeg);
            verts[vi++] = { x, y, z, x, y, z, u, v };
        }
    }
    int ii = 0;
    for (int lat = 0; lat < latSeg; ++lat) {
        for (int lon = 0; lon < lonSeg; ++lon) {
            int a = lat * (lonSeg + 1) + lon;
            int b = a + lonSeg + 1;
            idxs[ii++] = a;     idxs[ii++] = b;     idxs[ii++] = a + 1;
            idxs[ii++] = b;     idxs[ii++] = b + 1; idxs[ii++] = a + 1;
        }
    }

    glGenVertexArrays(1, &m_sphereVAO);
    glGenBuffers(1, &m_sphereVBO);
    glGenBuffers(1, &m_sphereIBO);
    glBindVertexArray(m_sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vc * sizeof(Vertex)), verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphereIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(ic * sizeof(unsigned int)), idxs, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    m_sphereIndexCount = ic;

    delete[] verts;
    delete[] idxs;
}

// =============================================================================
// Capsule: a vertical cylinder (radius 1, straight length 1, centered at
// origin from y=-0.5..+0.5) capped by two hemispheres of radius 1. Scaled at
// runtime by (radius, straightLen, radius) where straightLen = capsuleHeight-2r,
// so the total rendered height is capsuleHeight.
// =============================================================================
void Renderer::buildCapsule(int seg, int hemiRings) {
    struct Vertex { float x, y, z, nx, ny, nz, u, v; };
    std::vector<Vertex> verts;
    std::vector<unsigned int> idxs;

    const float PI = 3.14159265f, PI2 = 2.0f * PI;
    const float halfStraight = 0.5f;  // straight cylinder half-length (unit)

    // ---- Cylinder side ring ----  (seg+1 columns × 2 rows: top/bot of straight part)
    int cylTopStart = static_cast<int>(verts.size());
    for (int i = 0; i <= seg; ++i) {
        float phi = static_cast<float>(i) / static_cast<float>(seg) * PI2;
        float cx = std::cos(phi), cz = std::sin(phi);
        verts.push_back({ cx, -halfStraight, cz, cx, 0.0f, cz, 0.0f, 0.0f }); // bottom of straight
        verts.push_back({ cx,  halfStraight, cz, cx, 0.0f, cz, 1.0f, 0.0f }); // top of straight
    }
    for (int i = 0; i < seg; ++i) {
        int b0 = cylTopStart + i * 2, t0 = b0 + 1;
        int b1 = cylTopStart + (i + 1) * 2, t1 = b1 + 1;
        idxs.push_back(b0); idxs.push_back(t0); idxs.push_back(t1);
        idxs.push_back(b0); idxs.push_back(t1); idxs.push_back(b1);
    }

    // ---- Bottom hemisphere (cap at y = -halfStraight, pointing -Y) ----
    // hemiRings latitude bands from equator (theta=PI/2) to pole (theta=PI).
    int hemiBottomStart = static_cast<int>(verts.size());
    for (int r = 0; r <= hemiRings; ++r) {
        float theta = (PI * 0.5f) + (static_cast<float>(r) / static_cast<float>(hemiRings)) * (PI * 0.5f);
        float st = std::sin(theta), ct = std::cos(theta);  // ct: 0 → -1
        float y = -halfStraight + ct;  // -0.5 → -1.5
        for (int i = 0; i <= seg; ++i) {
            float phi = static_cast<float>(i) / static_cast<float>(seg) * PI2;
            float x = st * std::cos(phi), z = st * std::sin(phi);
            verts.push_back({ x, y, z, x, ct, z, 0.0f, 0.0f });
        }
    }
    for (int r = 0; r < hemiRings; ++r) {
        for (int i = 0; i < seg; ++i) {
            int a = hemiBottomStart + r * (seg + 1) + i;
            int b = a + (seg + 1);
            idxs.push_back(a); idxs.push_back(b); idxs.push_back(a + 1);
            idxs.push_back(b); idxs.push_back(b + 1); idxs.push_back(a + 1);
        }
    }

    // ---- Top hemisphere (cap at y = +halfStraight, pointing +Y) ----
    int hemiTopStart = static_cast<int>(verts.size());
    for (int r = 0; r <= hemiRings; ++r) {
        float theta = (PI * 0.5f) - (static_cast<float>(r) / static_cast<float>(hemiRings)) * (PI * 0.5f);
        float st = std::sin(theta), ct = std::cos(theta);  // ct: 0 → 1
        float y = halfStraight + ct;  // +0.5 → +1.5
        for (int i = 0; i <= seg; ++i) {
            float phi = static_cast<float>(i) / static_cast<float>(seg) * PI2;
            float x = st * std::cos(phi), z = st * std::sin(phi);
            verts.push_back({ x, y, z, x, ct, z, 0.0f, 0.0f });
        }
    }
    for (int r = 0; r < hemiRings; ++r) {
        for (int i = 0; i < seg; ++i) {
            int a = hemiTopStart + r * (seg + 1) + i;
            int b = a + (seg + 1);
            idxs.push_back(a); idxs.push_back(b); idxs.push_back(a + 1);
            idxs.push_back(b); idxs.push_back(b + 1); idxs.push_back(a + 1);
        }
    }

    glGenVertexArrays(1, &m_capsuleVAO);
    glGenBuffers(1, &m_capsuleVBO);
    glGenBuffers(1, &m_capsuleIBO);
    glBindVertexArray(m_capsuleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_capsuleVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_capsuleIBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(idxs.size() * sizeof(unsigned int)), idxs.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    m_capsuleIndexCount = static_cast<int>(idxs.size());
}

// =============================================================================
void Renderer::buildCrosshair() {
    constexpr float GAP = 0.015f, LEN = 0.04f;
    struct V { float x, y; };
    V verts[8] = {
        {-LEN, 0.0f}, {-GAP, 0.0f},
        { GAP, 0.0f}, { LEN, 0.0f},
        {0.0f,-LEN}, {0.0f,-GAP},
        {0.0f, GAP}, {0.0f, LEN},
    };
    glGenVertexArrays(1, &m_crosshairVAO);
    glGenBuffers(1, &m_crosshairVBO);
    glBindVertexArray(m_crosshairVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_crosshairVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// =============================================================================
glm::mat4 Renderer::computeViewMatrix(const CameraState& cam) const {
    float yawRad   = glm::radians(cam.yaw);
    float pitchRad = glm::radians(cam.pitch);
    const glm::vec3 eye(cam.x, cam.y, cam.z);
    glm::vec3 fwd(0.0f, 0.0f, -1.0f);

    glm::mat4 yawMat = glm::rotate(glm::mat4(1.0f), yawRad, glm::vec3(0, 1, 0));
    fwd = glm::vec3(yawMat * glm::vec4(fwd, 0.0f));

    glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3(0, 1, 0)));
    glm::mat4 pitchMat = glm::rotate(glm::mat4(1.0f), pitchRad, right);
    fwd = glm::vec3(pitchMat * glm::vec4(fwd, 0.0f));

    return glm::lookAt(eye, eye + fwd, glm::vec3(0, 1, 0));
}

glm::mat4 Renderer::computeProjectionMatrix() const {
    float aspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    return glm::perspective(glm::radians(75.0f), aspect, 0.1f, 80.0f);
}

// =============================================================================
void Renderer::draw(const CameraState& camera, const TargetState& target, const PlayerState& player, bool aimed, TargetModel model, float capsuleHeight) {
    glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view     = computeViewMatrix(camera);
    glm::mat4 proj     = computeProjectionMatrix();
    glm::mat4 viewProj = proj * view;

    const glm::vec3 camPos(camera.x, camera.y, camera.z);
    const glm::vec3 lightDir = glm::normalize(glm::vec3(LIGHT_DIR[0], LIGHT_DIR[1], LIGHT_DIR[2]));
    const glm::vec3 lightCol(1.0f, 0.96f, 0.88f);  // warm daylight

    m_sceneShader.bind();
    m_sceneShader.setUniformMatrix4fv("uViewProj", viewProj);
    m_sceneShader.setUniform3f("uCamPos",    camPos.x, camPos.y, camPos.z);
    m_sceneShader.setUniform3f("uLightDir",   lightDir.x, lightDir.y, lightDir.z);
    m_sceneShader.setUniform3f("uLightColor", lightCol.x, lightCol.y, lightCol.z);

    // ---- Room surfaces (NO culling — we're inside the box) ----
    glDisable(GL_CULL_FACE);
    m_sceneShader.setUniform1i("uDrawID", 0);
    glm::mat4 identity = glm::mat4(1.0f);

    auto drawQuad = [&](unsigned int vao, int count) {
        m_sceneShader.setUniformMatrix4fv("uModel", identity);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_INT, 0);
    };

    drawQuad(m_floorVAO, m_floorIndexCount);
    drawQuad(m_ceilVAO,  m_ceilIndexCount);
    drawQuad(m_wallNZVAO, m_wallNZIndexCount);
    drawQuad(m_wallPZVAO, m_wallPZIndexCount);
    drawQuad(m_wallXVAO,  m_wallXIndexCount);
    drawQuad(m_wallX2VAO, m_wallX2IndexCount);
    glEnable(GL_CULL_FACE);  // re-enable for 3D objects

    // ---- Target (sphere or capsule) ----
    // Color reflects aim state: white when on target, peach when not.
    m_sceneShader.setUniform1i("uDrawID", 1);
    if (aimed) {
        m_sceneShader.setUniform3f("uBaseColor", 1.00f, 1.00f, 1.00f);  // white
    } else {
        m_sceneShader.setUniform3f("uBaseColor", 1.00f, 0.65f, 0.55f);  // peach
    }
    glm::mat4 tgtModel = glm::translate(glm::mat4(1.0f), glm::vec3(target.x, target.y, target.z));
    if (model == TargetModel::Capsule) {
        // Capsule unit geometry: r=1, straight length=1, hemisphere caps r=1
        // (total unit height = 3). Scale so rendered total = capsuleHeight:
        //   X,Z = radius,  Y = (capsuleHeight - 2*radius)  (straight part).
        //   hemispheres come from the radius scaling, so total = straight+2r = capsuleHeight.
        float r = target.radius;
        float straightLen = std::max(0.01f, capsuleHeight - 2.0f * r);
        tgtModel = glm::scale(tgtModel, glm::vec3(r, straightLen, r));
        m_sceneShader.setUniformMatrix4fv("uModel", tgtModel);
        glBindVertexArray(m_capsuleVAO);
        glDrawElements(GL_TRIANGLES, m_capsuleIndexCount, GL_UNSIGNED_INT, 0);
    } else {
        tgtModel = glm::scale(tgtModel, glm::vec3(target.radius));
        m_sceneShader.setUniformMatrix4fv("uModel", tgtModel);
        glBindVertexArray(m_sphereVAO);
        glDrawElements(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT, 0);
    }

    // ---- Player feet marker ----
    m_sceneShader.setUniform1i("uDrawID", 2);
    m_sceneShader.setUniform3f("uBaseColor", 0.20f, 0.75f, 1.0f);
    tgtModel = glm::translate(glm::mat4(1.0f), glm::vec3(player.x, 0.01f, player.z));
    tgtModel = glm::scale(tgtModel, glm::vec3(0.25f, 0.03f, 0.25f));
    m_sceneShader.setUniformMatrix4fv("uModel", tgtModel);
    glBindVertexArray(m_sphereVAO);
    glDrawElements(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT, 0);

    m_sceneShader.unbind();

    // ---- Crosshair ----
    glDisable(GL_DEPTH_TEST);
    m_crosshairShader.bind();
    m_crosshairShader.setUniform3f("uColor", 1.0f, 1.0f, 1.0f);
    glBindVertexArray(m_crosshairVAO);
    glDrawArrays(GL_LINES, 0, 8);
    m_crosshairShader.unbind();
    glEnable(GL_DEPTH_TEST);
}
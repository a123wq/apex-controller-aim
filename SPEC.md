# Apex Stick Trainer V1.0 — 实现规格

> **核心定位**：Xbox 手柄瞄准手感训练器（3D 相机空间）。**不是** 2D 点击靶场。
> 手柄输入经可配置流水线（Deadzone → 曲线 → 微瞄 → 平滑 → 最高速度）驱动相机旋转（Yaw/Pitch），间接控制准星在 3D 场景中的指向。

---

## 一、需求回顾（对照 CLAUDE.md）

| CLAUDE.md 关键字 | 实现规格对应 |
|---|---|
| 单帧循环，DeltaTime | `App::run()` 每帧计算 `dt`，传 `Trainer::update()` |
| 输入 → 响应曲线 → 角速度 → 相机 | `Input` → `ResponseCurve::process()` → `Camera::update()` |
| 8 个可配置参数 | `AimProfile` 的 11 个字段 |
| 真实 3D 场景（地/天/准星/移动靶球） | `Renderer::draw()` 渲染 ground + sky + target + crosshair |
| 所有模块 SRP 且解耦 | 9 个独立模块，无跨模块直接状态访问 |
| 无阴影/HDR/PBR/粒子/后处理 | 纯 flat shading，OpenGL 3.3 Core |
| 240 FPS，<100ms 启动，<3% CPU，<30MB RAM | 性能预算内设计 |
| Classic43 / Linear43 两套配置 | `assets/profiles/classic43.json` + `linear43.json` |
| 热插拔手柄 | `Input::update()` 每次轮询 `XInputGetState`，错误码自动恢复 |
| ImGui 实时调参，立即生效 | `UI::render()` → `Trainer` → `ResponseCurve` → `Camera`，立即响应 |

---

## 二、项目结构

```
D:\claude_project\controller_aim\
├── SPEC.md                                  # 本文件
├── CLAUDE.md                                # 需求规格（已就绪）
├── Makefile                                 # 主构建配置（MinGW + 直接依赖）
├── scripts/
│   └── fetch_deps.sh                       # 下载 SDL2/glm/nlohmann/ImGui/glad
├── assets/
│   └── profiles/
│       ├── classic43.json                  # 默认：幂函数响应曲线 + 加速减速
│       └── linear43.json                   # 默认：线性响应，无加速减速
├── src/
│   ├── main.cpp                            # 入口点，#define SDL_MAIN_HANDLED
│   ├── App.cpp / .hpp                      # SDL 初始化 / OpenGL 3.3 上下文 / ImGui / 主循环
│   ├── Input.cpp / .hpp                   # XInput 手柄，输出原始 StickState
│   ├── Camera.cpp / .hpp                   # Yaw/Pitch 积分，更新 view matrix
│   ├── ResponseCurve.cpp / .hpp           # 响应曲线流水线（6 步）
│   ├── Trainer.cpp / .hpp                 # 场景编排器：Camera + Target + ResponseCurve
│   ├── Renderer.cpp / .hpp                # OpenGL 3.3 渲染：地/天/靶球/准星
│   ├── Target.cpp / .hpp                  # 随机左右移动的球体（左右/圆形/随机/V2 贝塞尔）
│   ├── UI.cpp / .hpp                      # ImGui 所有参数实时可调
│   ├── Profile.cpp / .hpp                  # JSON 加载/保存/重载 profile
│   └── Shader.cpp / .hpp                   # GLSL 编译工具函数
├── deps/                                   # 第三方库（fetch_deps.sh 自动下载）
│   ├── SDL2-x.x.x/                         # SDL2 库（Win32 MinGW）
│   ├── glm/                                # 头文件数学库
│   ├── nlohmann/                           # JSON 单头文件
│   ├── imgui/                              # Dear ImGui 源码 + SDL2+GL3 绑定
│   └── glad/                               # OpenGL 扩展加载器
└── .vscode/
    ├── c_cpp_properties.json               # MinGW include 路径
    └── launch.json                          # GDB 调试配置
```

---

## 三、技术选型

| 组件 | 选型 | 理由 |
|------|------|------|
| 编译器 | MinGW GCC 14.2.0（C++20） | Windows 原生，支持 C++20 全部特性 |
| 构建系统 | **Makefile**（不用 CMake） | CMake 未安装；MinGW 自带 `mingw32-make`；手工 Makefile 足够 |
| 窗口/手柄 | **SDL2 2.x**（从源码构建） | 跨平台事件循环；SDL3 暂不可用；`#define SDL_MAIN_HANDLED` 防止链接冲突 |
| OpenGL | **OpenGL 3.3 Core** + GLSL #version 330 | 最小通用版本；VAO/VBO/IBO 全支持；无 legacy API |
| OpenGL 加载器 | **glad**（单头文件） | 自动生成 GL 扩展函数指针，兼容 MinGW |
| 数学 | **glm 0.9.9+**（头文件） | `glm::vec3`, `glm::mat4`, `glm::radians`, `glm::lookAt`, `glm::perspective` |
| 配置 | **nlohmann/json 3.x**（单头文件） | 现代 C++17/20 API，无需安装 |
| UI | **Dear ImGui 1.9+**（源码 + `imgui_impl_sdl2_gl3`） | 实时调参无需重启 |
| 手柄 | **XInput**（MinGW 内置 `<xinput.h>`） | Xbox 控制器原生驱动，无需额外 SDK |
| 音频 | **无**（V1 不含） | CLAUDE.md 未要求音频 |

---

## 四、模块接口定义

### 4.1 StickState（Input 输出）

```cpp
struct StickState {
    float leftX  = 0.0f;   // 归一化 [-1, 1]
    float leftY  = 0.0f;
    float rightX = 0.0f;
    float rightY = 0.0f;
    float LT     = 0.0f;    // 左扳机 [0, 1]
    float RT     = 0.0f;    // 右扳机 [0, 1]
    bool  buttons[16] = {}; // A/B/X/Y/LB/RB/Start/Back/Up/Down/Left/Right + 4 扩展
};
```

### 4.2 AimProfile（配置核心）

```cpp
struct AimProfile {
    std::string name            = "Default";
    float deadzone              = 0.08f;   // 内死区半径（摇杆归一化值）
    float outerDeadzone         = 0.02f;   // 外死区（超近边缘的归一化值）
    float curveExponent         = 2.2f;    // 幂函数指数（1.0 = 线性）
    float microAimStrength      = 0.40f;   // 微瞄强度
    float microAimThreshold     = 0.15f;   // 微瞄激活阈值
    float inputSmoothing        = 0.12f;   // 指数平滑系数 [0,1]，越大越迟钝
    float maxYawSpeed           = 420.0f;  // 最大偏航速度（度/秒）
    float maxPitchSpeed         = 360.0f;  // 最大俯仰速度（度/秒）
    float ADSMultiplier         = 0.65f;   // 按住 RT 时的灵敏度倍率
    float acceleration          = 0.15f;  // 角速度变化速率上限（加速）
    float deceleration          = 0.12f;  // 角速度变化速率上限（减速）
};
```

### 4.3 ResponseCurve（响应曲线流水线）

```
原始摇杆 → Deadzone → 归一化 + Curve → Micro Aim → Input Smoothing → Max Speed → Acceleration → 角速度(度/秒)
```

每一步均为独立纯函数：

```cpp
float applyDeadzone(float value, float deadzone, float outerDeadzone);
// |value| ≤ deadzone → 0
// |value| ≥ (1 - outerDeadzone) → copysign(1, value)
// 其他：线性映射 [deadzone, 1-outerDeadzone] → [0, 1]

float applyCurve(float normalized, float exponent);
// pow(normalized, exponent) —— exponent=1.0 为线性，2.2 为经典幂函数

float applyMicroAim(float value, float threshold, float strength);
// |value| < threshold 时，乘以 (1 + strength * (1 - |value|/threshold))

float applySmoothing(float raw, float smoothing, float& smoothed, float dt);
// 指数移动平均：smoothed += (1-smoothing) * (raw - smoothed)

float applyMaxSpeed(float value, float maxSpeed);
// value ∈ [-1,1] → 角速度 ∈ [-maxSpeed, maxSpeed]

float applyAcceleration(float currentSpeed, float accel, float decel, float dt, float& storedSpeed);
// 每帧限速：|currentSpeed - stored| ≤ (currentSpeed > stored ? accel : decel) * dt
```

### 4.4 Camera（相机状态）

```cpp
struct CameraState {
    float yaw   = 0.0f;    // 水平旋转（度）
    float pitch = 0.0f;    // 垂直旋转（度），钳位到 [-89, 89]
};

void Camera::update(float yawSpeed, float pitchSpeed, double dt);
// yaw   += yawSpeed   * dt
// pitch += pitchSpeed * dt
// pitch = clamp(pitch, -89.0f, 89.0f)

glm::mat4 Camera::getViewMatrix() const;
// 基于 yaw/pitch 的 lookAt 矩阵
```

### 4.5 Target（移动靶球）

```cpp
struct TargetState {
    float x = 0.0f;        // 世界空间 X 坐标
    float y = 0.0f;        // Y 坐标（固定 0）
    float z = -15.0f;      // Z 坐标（固定，在相机前方 15 单位）
    float radius = 0.5f;   // 球体半径（世界空间）
    bool  active = true;
};

void Target::update(double dt);
// 1. 移动到随机目标 X（speed ∈ [minSpeed, maxSpeed]）
// 2. 到达后等待 random(waitTimeMin, waitTimeMax) 秒
// 3. 重复
```

### 4.6 Renderer（3D 渲染）

```cpp
void Renderer::draw(const CameraState& cam, const TargetState& target);
// 1. glClear → 天空色
// 2. glEnable(GL_DEPTH_TEST)
// 3. 计算 View + Projection → viewProj
// 4. 绘制 Ground（灰色大平面，y=0，范围 [-50, 50]）
// 5. 绘制 Target 球体（红色 UV 球）
// 6. glDisable(GL_DEPTH_TEST)
// 7. 绘制 Crosshair（屏幕空间十字丝，NDC 坐标，中心留空）
```

### 4.7 Trainer（场景编排）

```cpp
void Trainer::update(const StickState& input, double dt) {
    // ADS 检测
    bool ads = input.RT > 0.5f;
    float adsMult = ads ? m_profile.ADSMultiplier : 1.0f;

    // 响应曲线
    auto vel = m_response.process(input.rightX, input.rightY, m_profile, adsMult);

    // 相机
    m_camera.update(vel.yawSpeed, vel.pitchSpeed, dt);

    // 靶标
    m_target.update(dt);
}
```

---

## 五、主循环

```
App::run():

while (!m_quit) {
    1. pollEvents()                          // SDL_PollEvent + Input::update()
    2. calculateDeltaTime()                   // dt = currentTime - lastTime
    3. ImGui_ImplOpenGL3_NewFrame()
       ImGui_ImplSDL2_NewFrame()
       ImGui::NewFrame()
    4. Trainer::update(stickState, dt)        // 核心逻辑（无 OpenGL/UI）
    5. UI::render(profile)                    // ImGui 调参窗口
    6. ImGui::Render()
    7. Renderer::draw(cameraState, target)    // 3D 渲染
    8. ImGui_ImplOpenGL3_RenderDrawData()
    9. SDL_GL_SwapWindow()
   10. frameTiming(dt)                        // dt < 4.17ms 时 SDL_Delay 补齐
}
```

---

## 六、渲染管线

### 6.1 坐标系统

- 右手系，Y-up
- 相机位置固定在 `(0, 1.7, 0)`（眼高），朝向 `(0, 1.7, -1)`
- 视锥体：FOV=90°（竖直），near=0.1，far=100

### 6.2 几何体

| 对象 | 生成方式 | 颜色 |
|------|----------|------|
| Ground | 2 三角形大平面 y=0，范围 [-50,50] × [-50,50] | #4D4D4D |
| Target | UV 球 16 segments × 12 rings（约 288 三角形） | #FF3333 |
| Crosshair | 4 条 GL_LINES，屏幕空间，中心留 8px 缺口 | #FFFFFF |
| Sky | `glClearColor(0.05, 0.05, 0.15, 1.0)` | 深蓝黑 |

### 6.3 着色器

**scene.vert**（顶点着色器）：
```glsl
#version 330 core
in vec3 aPos;
in vec3 aNormal;
uniform mat4 uViewProj;
uniform mat4 uModel;
void main() {
    gl_Position = uViewProj * uModel * vec4(aPos, 1.0);
}
```

**scene.frag**（片段着色器）：
```glsl
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(uColor, 1.0);  // 无光照，flat color
}
```

---

## 七、ImGui 布局（对应 CLAUDE.md 规范）

```
┌─────────────────────────────────────────┐
│ Aim Trainer V1.0                         │
├─────────────────────────────────────────┤
│ Profile: [Classic43     ▼]               │
├─────────────────────────────────────────┤
│ Deadzone          0.080  ────●──────    │
│ Outer Deadzone    0.020  ──●────────    │
│ Curve Exponent    2.200  ──────●────    │
│ Micro Aim         0.400  ───────●───    │
│ Micro Threshold   0.150  ────●──────    │
│ Input Smooth      0.120  ────●──────    │
│ Yaw Speed         420.0  ───────●───    │
│ Pitch Speed       360.0  ───────●───    │
│ ADS               0.650  ────────●──    │
│ Acceleration      0.150  ────●──────    │
│ Deceleration      0.120  ────●──────    │
├─────────────────────────────────────────┤
│ [Save] [Reload] [Reset]  [Quit]         │
└─────────────────────────────────────────┘
```

- 所有 Slider 改动**立即**通过 Trainer → ResponseCurve 生效，无需重启
- Save/Reload/Reset 操作 Profile JSON 文件

---

## 八、配置文件 JSON 格式

```json
// assets/profiles/classic43.json
{
    "name": "Classic43",
    "deadzone": 0.08,
    "outerDeadzone": 0.02,
    "curveExponent": 2.2,
    "microAimStrength": 0.40,
    "microAimThreshold": 0.15,
    "inputSmoothing": 0.12,
    "maxYawSpeed": 420.0,
    "maxPitchSpeed": 360.0,
    "ADSMultiplier": 0.65,
    "acceleration": 0.15,
    "deceleration": 0.12
}
```

---

## 九、实现里程碑

| 阶段 | 关键产出 | 验证 |
|------|----------|------|
| **M1** 骨架 + 构建 | `Makefile` + `scripts/fetch_deps.sh`；`main.cpp` + `App` 空循环；SDL2 窗口成功打开 | 可运行窗口，关闭无崩溃 |
| **M2** OpenGL 3.3 + ImGui | `Renderer` 绘制纯色背景；ImGui demo 窗口渲染 | 240 FPS 空循环 |
| **M3** Profile 系统 | `Profile` 加载/保存/重载；classic43.json + linear43.json | 切换 profile JSON 内容正确生效 |
| **M4** XInput 输入 | `Input::update()` 读取手柄；`StickState` 输出归一化值 | 在 ImGui 文本框显示摇杆/扳机数值 |
| **M5** 响应曲线 | `ResponseCurve` 6 步流水线 | 注入已知输入，验证每步输出 |
| **M6** 相机 | `Camera::update(yawSpeed, pitchSpeed, dt)` | 摇杆推动准星旋转 |
| **M7** 靶球 | `Target` 随机左右移动 | 球体在场景中移动并等待 |
| **M8** 3D 场景 | Ground + Target + Crosshair 渲染 | 可见地面、靶球、准星 |
| **M9** Trainer 集成 | 完整流水线：输入 → 曲线 → 相机 → 渲染 | 手柄控制准星指向靶球 |
| **M10** UI 调参 | 所有 11 个 Slider + Save/Reload/Reset | 滑条改动立即影响瞄准手感 |
| **M11** ADS 模式 | 按住 RT → 灵敏度 × ADSMultiplier | 扳机触发灵敏度下降 |
| **M12** 性能调优 | 测量 FPS/CPU/RAM；消除性能问题 | 稳定 240 FPS，<3% CPU，<30MB |
| **M13** 单元测试 | `ScoreTracker` 核心指标测试（如有） | GoogleTest 通过 |

---

## 十、编码规范

| 规则 | 规范 |
|------|------|
| 命名 | CamelCase（类/函数），snake_case（变量/文件） |
| 头文件 | 每个 `.hpp` 含 `#pragma once`，前置声明代替互相包含 |
| 线程安全 | 无多线程（单线程主循环） |
| 错误处理 | 启动失败 → `std::runtime_error` + 顶层 `catch`；运行时不崩溃 |
| 无魔法数 | 所有数值定义为 `constexpr` 或配置参数 |
| 无 if-else 链 | 响应曲线每步为独立纯函数 |
| 日志 | `fmt::print(stderr, ...)`，release 构建禁用 |

---

## 十一、关键注意事项

1. **SDL2 + MinGW 链接**：在 `main.cpp` 第一行前定义 `#define SDL_MAIN_HANDLED`，使用 `main()` 而非 `SDL_main()`；链接 `-lSDL2main -lSDL2 -lopengl32`
2. **XInput 链接**：MinGW 自带 `xinput.h`，链接 `-lxinput`（或 `libxinput.a`）
3. **OpenGL 3.3 Core Profile**：`SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE)`
4. **glad**：使用 `glad` 单头文件注入 OpenGL 函数指针，通过 `SDL_GL_GetProcAddress` 加载
5. **响应曲线公式**：幂函数使用 `std::pow(normalized, exponent)`，对 2 个轴在 240 FPS 下 CPU 占用可忽略
6. **ADS 检测**：RT ∈ [0, 255]，归一化为 [0, 1]，阈值 0.5（大约 50% 按下）
7. **热插拔**：每次 `XInputGetState` 调用后检查返回错误码；连接恢复后自动继续

---

*实现规格 V1.0 — 基于 CLAUDE.md 需求驱动设计*


# Apex Stick Trainer V1.0 - 开发方案

## 一、项目目标

开发一个**完全独立运行**的离线瞄准训练程序。

本项目**不是 Apex Mod，不与 Apex 交互，不读取任何游戏数据，不 Hook，不注入 DLL，不修改任何游戏行为。**

目标仅为：

> 使用 Xbox 手柄，提供与 Apex Legends 接近的瞄准体验（Classic 4-3、Linear 4-3），用于离线训练。

---

# 二、开发原则（必须遵守）

## 必须遵守

- 独立运行

- 不依赖 Steam

- 不依赖 Unity

- 不依赖 Unreal

- 不依赖 Apex

- 不联网

- 低延迟

- 模块化

- 参数化

- 所有参数支持实时修改

- 所有参数支持保存

---

## 严禁实现

禁止实现任何：

- Apex Memory Read

- ReadProcessMemory

- WriteProcessMemory

- DLL Injection

- Hook

- Overlay

- Driver

- Cheat

- Macro

- Auto Aim

- Aim Assist

- 自动压枪

- 自动瞄准

- 修改 Apex 配置文件

本项目仅模拟训练环境。

> **例外（scoped exception）**：本项目作为离线训练器，允许在**手柄瞄准路径**中对**自身模拟目标**实现 Apex 风格的辅助瞄准模拟（Slowdown + Rotational）。此 AA 仅作用于训练器内部的镜头朝向行为，不读取、不修改、不注入任何外部游戏（包括 Apex）。前述 Memory Read / Hook / 注入 / Overlay 等针对真实游戏的禁令不受影响，原样保留。

---

# 三、技术栈

推荐：

```
C++20

SDL3

OpenGL

Dear ImGui

glm
```

原因：

- 编译后二进制小

- 启动快

- XInput 支持成熟

- 延迟低

- 后续容易扩展

---

# 四、项目目录

```
ApexStickTrainer/

src/

    App/

    Input/

    Camera/

    Renderer/

    Trainer/

    Target/

    UI/

    Profile/

    Math/

assets/

profiles/

    classic43.json

    linear43.json

config/

    config.json
```

---

# 五、模块设计

---

## App

负责：

- 初始化 SDL

- 初始化 OpenGL

- 初始化 ImGui

- 主循环

- DeltaTime

- FPS 控制

不包含业务逻辑。

---

## Input

负责：

Xbox 手柄输入。

要求：

支持：

- 热插拔

- 自动识别

- Dead Connection Recovery

输出：

```cpp
struct StickState
{
    float leftX;
    float leftY;

    float rightX;
    float rightY;

    float LT;
    float RT;

    bool buttons[16];
};
```

Input 模块禁止处理任何 Response Curve。

只输出原始输入。

---

## Camera

负责：

Camera Rotation。

输入：

```
Raw Stick
```

输出：

```
Yaw

Pitch
```

Camera 不知道：

Classic

Linear

Profile

Camera 只接受：

```
Angular Velocity
```

例如：

```
Yaw += yawSpeed * dt;

Pitch += pitchSpeed * dt;
```

---

## ResponseCurve

这是整个项目最重要的模块。

不要使用：

```
pow()

if else

大量魔法数字
```

设计：

```
Raw Stick

↓

Deadzone

↓

Normalization

↓

Curve

↓

Micro Aim

↓

Input Smooth

↓

Max Speed

↓

Angular Velocity
```

每一步必须独立。

每一步必须可配置。

---

## Trainer

负责：

更新：

- Camera

- Target

- Scene

Trainer 不包含：

OpenGL

Input

UI

---

## Renderer

负责：

绘制。

仅负责：

```
Ground

Sky

Crosshair

Target
```

不要加入：

阴影

HDR

PBR

粒子

后处理

Bloom

保证：

240FPS。

---

## Target

实现：

一个球体。

支持：

随机左右移动。

参数：

```
速度最小值

速度最大值

移动范围

等待时间
```

支持：

以后增加：

```
圆形

随机

Bezier

Spline
```

---

## UI

采用：

Dear ImGui。

所有参数实时可调。

修改立即生效。

无需重启。

---

## Profile

负责：

保存：

```
classic43.json

linear43.json
```

支持：

```
Load

Save

Save As

Reload
```

---

# 六、参数驱动架构（核心）

项目不得使用固定算法模拟 Apex。

所有行为由 Profile 决定。

定义：

```cpp
struct AimProfile
{
    float deadzone;

    float outerDeadzone;

    float curveExponent;

    float microAimStrength;

    float microAimThreshold;

    float inputSmoothing;

    float maxYawSpeed;

    float maxPitchSpeed;

    float ADSMultiplier;

    float acceleration;

    float deceleration;
};
```

ResponseCurve：

仅负责：

```
Input

↓

Profile

↓

Angular Velocity
```

以后：

Classic43

Linear43

仅仅是：

Profile 不同。

---

# 七、Profile 文件

采用：

JSON。

例如：

classic43.json

```json
{
    "name":"Classic43",

    "deadzone":0.08,

    "outerDeadzone":0.02,

    "curveExponent":2.2,

    "microAimStrength":0.40,

    "microAimThreshold":0.15,

    "inputSmoothing":0.12,

    "maxYawSpeed":420,

    "maxPitchSpeed":360,

    "ADSMultiplier":0.65,

    "acceleration":0.15,

    "deceleration":0.12
}
```

Linear43：

同理。

Agent 可提供一套初始值。

这些值不是 Apex 官方数据，仅作为默认配置，后续通过实际体验逐步调整。

---

# 八、实时调参（必须实现）

UI：

```
Current Profile

Classic43

Linear43

--------------------------------

Deadzone

Outer Deadzone

Curve Exponent

Micro Aim

Micro Threshold

Input Smooth

Yaw Speed

Pitch Speed

ADS

Acceleration

Deceleration

--------------------------------

Save

Reload

Reset
```

所有 Slider：

实时更新。

无需重新编译。

无需重启。

---

# 九、Camera 更新流程

```
Raw Stick

↓

Deadzone

↓

Normalize

↓

Curve

↓

Micro Aim

↓

Smoothing

↓

Angular Velocity

↓

Camera
```

所有步骤必须拆分成独立函数。

禁止写成一个大函数。

---

# 十、Target

支持：

```
左右随机移动
```

参数：

```
随机速度

随机停顿

随机方向

最大范围
```

不要：

AI

导航

寻路

攻击

碰撞

---

# 十一、性能目标

启动：

```
<100ms
```

CPU：

```
<3%
```

内存：

```
<30MB
```

FPS：

```
240FPS
```

Input Latency：

尽可能低。

避免：

```
Sleep()

Busy Wait

阻塞
```

---

# 十二、后续扩展（V2，不实现）

架构需预留扩展能力，但 V1 不实现：

- 更多 Aim Profile（如 ALC、5-4、6-3）

- 更多 Target 运动模式

- 命中检测与统计

- Session 数据分析

- 自定义 Profile 管理

- Profile 导入/导出

- WebAssembly 前端版本

---

# 十三、代码规范

必须遵守：

- 单一职责原则（SRP）

- 每个模块只负责一个功能

- 不允许跨模块直接访问内部状态

- Profile 为唯一参数来源

- 所有魔法数字必须配置化

- 不允许硬编码 Classic43 或 Linear43 的逻辑

- ResponseCurve、Camera、Input、UI、Target 完全解耦

- 所有可调参数必须支持热更新和持久化

---

# 十四、V1 验收标准

项目达到以下标准即可视为完成 V1：

1. Xbox 手柄可即插即用，输入稳定。

2. 程序可在 240 FPS 下稳定运行，启动快速、资源占用低。

3. 提供一个简单训练场，包含准星和可左右随机移动的目标。

4. 可在运行时一键切换 **Classic43** 与 **Linear43** 配置。

5. 所有瞄准参数均可通过 ImGui 实时调节，并支持保存、加载和热重载。

6. 整个瞄准系统完全采用**参数驱动**，Profile 是唯一的行为配置来源。

7. 程序不依赖 Apex，不与任何游戏进程交互，不包含任何 Hook、注入、内存访问或自动化功能。

> **开发重点不是“逆向 Apex 算法”，而是构建一个低延迟、可调试、可扩展的瞄准框架，并提供一套默认的 Classic43 / Linear43 参数作为起点，使后续调校只需要修改 Profile，而无需修改代码。**

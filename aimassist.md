# 辅助瞄准 (Aim Assist) 实现方案

> 本文件是 ApexStickTrainer 为手柄瞄准路径加入仿 Apex 辅助瞄准的完整设计与执行方案。
> 依据：用户提供的 `辅助瞄准原理.md`。

---

## Context

为 `ApexStickTrainer.exe` 的**手柄瞄准路径**加入仿 Apex 的辅助瞄准。`辅助瞄准原理.md` 把 Apex AA 拆成三个机制：

1. **Reticle Slowdown（减速辅助）** — 准星进入目标周围的不可见 "AA Bubble"（比 hitbox 略大）后，**降低右摇杆灵敏度**（摇杆 100% → 镜头旋转 40~60%）。镜头不被直接移动，只是旋转速度被降低，产生"粘住"感。
2. **Rotational Aim Assist（旋转辅助）** — 目标移动 + 玩家在 Bubble 内时，**额外施加一个小的 yaw/pitch** 跟随目标运动方向。
3. **Movement Gate（左摇杆门）** — Rotational 的触发条件之一是**存在移动输入**（左摇杆 / WASD）。这就是"左摇杆 AA"，也是玩家故意保留轻微 stick drift 的原因。

关键约束：**PC 系数 ~0.4，主机 ~0.6**；**离开 Bubble 立即停止**；**仅手柄路径**（鼠标路径不加 AA）。

### 与 CLAUDE.md 的冲突（H1，必须先处理）
CLAUDE.md 第二章「严禁实现」列出了 `Aim Assist / Auto Aim`，与本次需求直接冲突。本次 AA **只作用于训练器自身的模拟目标**（镜头朝向行为），不涉及任何 Apex 交互（Memory Read / Hook / 注入 / Overlay 仍禁止，原样保留）。**建议在 CLAUDE.md 增加一条 scoped exception**：训练器内对模拟目标的 AA 模拟允许；对 Apex 或任何外部游戏的 AA / Hook / 注入仍禁止。这一步作为执行的第 0 步，用户若不认可可 veto。

---

## 设计要点

**模块位置**：新增 `AimAssist` 类，夹在 `ResponseCurve` 输出与 `Camera.update` 之间 —— 它调制玩家的角速度（减速）并叠加旋转跟踪项。`Camera` 只接受角速度，**完全不改动**；`ResponseCurve` 完全不改动；`Input`/`Target`/`UI` 仅通过新增的公开 getter / 参数解耦访问。仅 `Trainer::update`（手柄路径）接线，`Trainer::updateWithMouse`（鼠标路径）**保持无 AA**。

**参数位置**：AA 参数放进 `AimProfile`（响应手感层，走 Profile 的 Save/Reload/Reset + JSON 持久化），**不进** `AppConfig`。符合项目两级参数体系（AimProfile=响应手感，Profile 是唯一行为参数来源；AppConfig=目标/鼠标、脏写自动保存）。`Profile.cpp` 的 `j.value(...)` 默认值让旧 JSON 文件无 AA 键也能正常加载。

### 两个强度轴，分别量化（重要，避免概念混淆）

Apex 的「AA 强度」其实是**减速（粘住）+ 旋转（跟随）两个独立轴叠加**。Respawn 公开的 0.4(PC)/0.6(主机) 只精确对应**旋转**那个轴 —— 它是个未公开内部公式的聚合系数，没有官方公式把它换算成具体角加速度，但能干净地映射到我们的旋转模型。

- **轴 1 — 旋转 `aaRotationalGain`（对应 0.4/0.6）**：旋转项最终化简为 `相机自动旋转 = gain × ω_target`（ω_target = 目标相对相机的角速度，`vRight/dist`，rad/s，小角近似）。所以 **gain 的物理含义 = 相机自动匹配目标角速度的比例**：0.4=跟 40%、0.6=跟 60%、1.0=完全锁定。方向与「主机 0.6 > PC 0.4 = 更强」一致。**默认 0.4（PC），量化值可解释，非拍脑袋。**
- **轴 2 — 减速 `aaStickiness`（用户「粘住感/吸引力」直觉 = 这个）**：摇杆推 100% 但镜头只转一部分，准星难被推离目标 → 感觉"被吸住"。文档原文「右摇杆100% → 游戏旋转40~60%」给的是减速**区间** 0.4–0.6，**独立于** 0.4/0.6。按用户口径「**0=不粘, 1=最粘**」归一化，**默认 0.3**。内部映射：`slowdownFactor = lerp(1.0, 0.4, aaStickiness)` —— 即 stickiness=0 时不减速（1.0），stickiness=1 时摇杆→40%旋转（文档区间下限，最强粘），stickiness=0.3 时→0.88×（轻度粘）。这样 UI 滑块「0→1」方向与手感一致。

### 旋转模型 = 速度跟随 + 可选位置 pull（用户选定）

- 主项：速度跟随（`gain × ω_target`，朝目标运动方向漂）。稳定、不与玩家抢控制权。
- 附加项：向 bubble 中心的位置 pull（`pull ∝ angle`，瞄越偏拽越强，到中心归零），独立 `aaPullGain`，**默认 0（关闭）**，想试硬拽感再调大。这两项都乘 `bubbleT`，离开 bubble 同样立即清零。
- 公式（deg/s，clamp 前）：
  - `rotYaw = -(vRight/dist) × p.aaRotationalGain × bubbleT × moveF × (180/π)`  ← 速度跟随
  - `rotYaw += sign(angle) × clamp(|angle| - hitMargin, 0, bubbleRad) × p.aaPullGain × bubbleT × moveF`  ← 位置 pull（指向 bubble 中心方向）
  - pitch 同理（用 vUp 与 pitch 偏角）。
  - `moveF = smoothstep(p.aaMovementGate, p.aaMovementGate+0.2, moveMag)`；无移动时旋转项归零（减速仍在）。

### 旋转符号表（关键，已由 Plan 子代理对照源码验证）

对照 `Camera.cpp:25-26`（`yaw += yawSpeed*dt; pitch += pitchSpeed*dt`）与 `Trainer.cpp:130-134`（`forward = (-sin yaw·cos pitch, sin pitch, -cos yaw·cos pitch)`）：

| 量 | 约定 | 验证 |
|---|---|---|
| yawSpeed>0 → yaw↑ → 视角左转 | forward.x = −sin(yaw) | ✅ |
| pitchSpeed>0 → pitch↑ → 向上看 | forward.y = sin(pitch) | ✅ |
| view-right = cross(forward, worldUp(0,1,0)) | yaw=0,pitch=0 → (+1,0,0)=右 | ✅ |
| view-up = cross(right, forward) | yaw=0,pitch=0 → (0,+1,0)=上 | ✅ |

由此推导出（**已验证正确**）：
- 目标向视角右移（`vRight>0`）→ 需向右转 → yaw 需减小 → `rotYaw = -(vRight/dist)·gain`
- 目标向上移（`vUp>0`）→ 需向上看 → pitch 需增大 → `rotPitch = +(vUp/dist)·gain`

> 注：`Target::pickEscapeDirection` 用的是相反叉乘序 `cross(worldUp, fwd)` 得到「左」，但它随后乘随机 `±sideSign` 抵消，无害。AA 这里用 `cross(forward, worldUp)` 是正确的「右」。

### 已采纳的修正
- **M1**：旋转项 EMA 平滑在 `!inBubble` 时**直接清零** `m_smoothRotYaw/m_smoothRotPitch`（否则留下 ~30–60ms 旋转尾巴，违反「离开 Bubble 立即停止」）。减速因子用连续 `bubbleT` lerp，本就即时 snap，合规。
- **M2**：`AimAssist::apply` 签名只传**基元**（eye/forward/tgtPos/tgtVel/moveMag），不传 Camera/Target 对象，避免跨模块耦合（CLAUDE.md「完全解耦」）。
- **L2**：`cross(forward, worldUp)` 在极端 pitch 下退化 → `if(length<1e-4) right=(1,0,0)`，镜像现有 `pickEscapeDirection` 的保护。
- **L3**：`aaBubbleAngle` 存为度，比较时显式 `glm::radians()` 转换并注释（避免 57× 量级错误）。

> 放弃 L1 的 Bubble 边缘 hysteresis：减速 `bubbleT` 连续、旋转项被 `bubbleT`（边缘→0）自阻尼，边缘进入/离开的跳变已很小，无需额外 grace，且严格符合「立即停止」。

---

## 要改动的文件

### 1. `src/App.hpp`（全量重编译，Makefile 的 `-MMD -MP` 已正确处理）
- **`AimProfile` 增加 AA 参数**（默认值，Classic43/Linear43 均用 PC 系数）：
  ```
  bool  aaEnabled        = true;
  float aaBubbleAngle    = 4.0f;    // Bubble 半角（度）
  float aaMaxDistance    = 60.0f;   // 最远生效距离（m）
  float aaStickiness     = 0.30f;   // 减速粘性 0=不粘..1=最粘（映射到 slowdownFactor∈[0.4,1.0]）
  float aaRotationalGain = 0.40f;   // 旋转跟随比例（PC 0.4 / 主机 0.6）= 相机匹配目标角速度的占比
  float aaPullGain       = 0.0f;    // 向 bubble 中心的位置 pull 强度（0=关，默认关）
  float aaRotMaxSpeed    = 25.0f;   // 旋转项上限（deg/s）
  float aaMovementGate   = 0.15f;   // 左摇杆/移动幅度门限
  float aaSmoothing      = 0.30f;   // 旋转项 EMA 平滑
  ```
- **新增 `class AimAssist`**：私有成员 `float m_smoothRotYaw=0, m_smoothRotPitch=0; bool m_inBubble=false;`；公开：
  ```cpp
  struct AAOutput { float yawSpeed; float pitchSpeed; bool inBubble; };
  AAOutput apply(float inYaw, float inPitch,
                 const glm::vec3& eye, const glm::vec3& forward,
                 const glm::vec3& tgtPos, const glm::vec3& tgtVel,
                 float moveMag, const AimProfile& p, double dt);
  bool isInBubble() const { return m_inBubble; }
  ```
- **`Target`** 增加公开 `glm::vec3 getVelocity() const` 返回 `(m_dirX*m_speed, m_vy, m_dirZ*m_speed)`（by-value 安全，GLM_FORCE_DEFAULT_ALIGNED_GENTYPES 未定义）。
- **`Trainer`** 增加 `AimAssist m_aimAssist;` 成员 + `bool isAABubbleActive() const { return m_aimAssist.isInBubble(); }`。
- **`UI::render`** 签名增加 `bool aaActive`（放在 `aimedAtTarget` 旁）。

### 2. `src/AimAssist.cpp`（新文件，Makefile 的 `wildcard` 自动纳入）
实现 `apply()`，算法（含 M1/M2/L2/L3 修正 + pull 项）：
1. `dirToTarget = tgtPos - eye; dist = length(dirToTarget);` 若 `dist < 0.5` → 返回 `{inYaw, inPitch, false}`；归一化 dir。
2. `cosA = clamp(dot(forward, dirToTarget), -1, 1); inFront = cosA > 0; angle = acos(cosA);`
3. `bubbleRad = glm::radians(p.aaBubbleAngle); inBubble = p.aaEnabled && inFront && angle <= bubbleRad && dist <= p.aaMaxDistance;`
4. `bubbleT = inBubble ? clamp(1 - angle/bubbleRad, 0, 1) : 0;`
5. **减速**（`aaStickiness` 0=不粘..1=最粘 → slowdownFactor 1.0..0.4）：`slowdownFactor = lerp(1.0f, 0.4f, p.aaStickiness); factor = lerp(1.0f, slowdownFactor, bubbleT); outYaw = inYaw*factor; outPitch = inPitch*factor;`
6. **旋转**（仅 inBubble 时计算）：
   - `right = cross(forward, worldUp); if(length(right)<1e-4) right=(1,0,0); normalize;` `up = cross(right, forward);`
   - `vRight = dot(tgtVel, right); vUp = dot(tgtVel, up);` `moveF = smoothstep(p.aaMovementGate, p.aaMovementGate+0.2f, moveMag);`
   - **速度跟随**：`rotYaw = -(vRight/dist) * p.aaRotationalGain * bubbleT * moveF * (180/PI);` `rotPitch = +(vUp/dist) * p.aaRotationalGain * bubbleT * moveF * (180/PI);`
   - **位置 pull**（向 bubble 中心，`aaPullGain` 默认 0）：`pullDeg = clamp(angle, 0, bubbleRad) * p.aaPullGain * bubbleT * moveF;` 用 `dirToTarget` 在 `right`/`up` 上的投影符号决定拉向：`side = dot(dirToTarget, right);` `rotYaw += (side>0 ? -pullDeg : +pullDeg);` pitch 同理用 `dot(dirToTarget, up)` 符号。（符号方向与速度跟随一致：右偏目标→拉回中心需向右转→yaw↓）
   - 各自 clamp 到 `±p.aaRotMaxSpeed`。
7. **EMA + M1 退出清零**：`if(!inBubble){ m_smoothRotYaw = m_smoothRotPitch = 0; }` 否则 `alpha=1-p.aaSmoothing; m_smoothRotYaw += alpha*(rotYaw - m_smoothRotYaw);`（pitch 同理）。
8. `outYaw += m_smoothRotYaw; outPitch += m_smoothRotPitch;` `m_inBubble = inBubble;` 返回 `{outYaw, outPitch, inBubble};`

### 3. `src/Trainer.cpp`（仅改 `update()`，`updateWithMouse` 不动）
在 `m_responseCurve.process(...)` 得到 `vel` 后、`m_camera.update(...)` 前，插入：
```cpp
float fx, fy, fz; cameraForward(fx, fy, fz);
glm::vec3 eye = m_camera.getPosition();
const TargetState& ts = m_target.getState();
glm::vec3 tgtPos(ts.x, ts.y, ts.z);
glm::vec3 tgtVel = m_target.getVelocity();
float moveMag = clamp(sqrt(leftX*leftX + leftY*leftY + moveX*moveX + moveY*moveY), 0, 1);
auto aa = m_aimAssist.apply(vel.yawSpeed, vel.pitchSpeed, eye,
                            glm::vec3(fx,fy,fz), tgtPos, tgtVel,
                            moveMag, getProfile(), dt);
m_camera.update(aa.yawSpeed, aa.pitchSpeed, dt);
```
> `input` 已含 leftX/leftY/moveX/moveY（App 在 WASD 块写入 moveX/moveY，Input::update 写入 leftX/leftY）。eye 来自 `updatePlayer` 同步，forward 与 `isAimingAtTarget` 用同一份（一致的一帧滞后，~7ms@144FPS，可忽略）。

### 4. `src/Target.cpp`
增加 `glm::vec3 Target::getVelocity() const { return glm::vec3(m_dirX*m_speed, m_vy, m_dirZ*m_speed); }`。`m_vy` 是弹跳垂直速度（含重力，逐帧变），纳入 vUp 表示弹跳中段 pitch-AA 最强 —— 合理。

### 5. `src/Profile.cpp`
- `defaultClassic43()` / `defaultLinear43()` 末尾设置 AA 默认值：`aaEnabled=true, aaBubbleAngle=4.0, aaMaxDistance=60.0, aaStickiness=0.30, aaRotationalGain=0.40, aaPullGain=0.0, aaRotMaxSpeed=25.0, aaMovementGate=0.15, aaSmoothing=0.30`。
- `load()` 增加 9 个 `j.value("aaXxx", ...)`（仿现有 `ADSMultiplier` 模式，line 101）。
- `save()` 增加 9 个 `j["aaXxx"] = ...`（仿 line 121）。
- `reload()` 同 load。
> 旧 `classic43.json`/`linear43.json` 无 AA 键也能加载（默认值填充）；运行一次 Save 即持久化新键，**无需手动编辑 JSON**。

### 6. `src/UI.cpp`
在 response curve 滑块区后加 `ImGui::CollapsingHeader("Aim Assist")`：
- `Checkbox("Enable AA", &profile.aaEnabled)`
- `SliderFloat("Bubble Angle (deg)", &profile.aaBubbleAngle, 0.5f, 20.0f)`
- `SliderFloat("Max Distance", &profile.aaMaxDistance, 10.0f, 120.0f)`
- `SliderFloat("Stickiness", &profile.aaStickiness, 0.0f, 1.0f, "%.2f")` （tooltip: 0=不粘, 1=最粘；映射到减速倍率 1.0→0.4）
- `SliderFloat("Rotational Gain", &profile.aaRotationalGain, 0.0f, 0.8f)` （tooltip: PC~0.4 / Console~0.6；= 相机匹配目标角速度的占比）
- `SliderFloat("Pull Gain", &profile.aaPullGain, 0.0f, 2.0f)` （tooltip: 向 bubble 中心的硬拽强度，0=关）
- `SliderFloat("Rot Max Speed", &profile.aaRotMaxSpeed, 0.0f, 60.0f)`
- `SliderFloat("Movement Gate", &profile.aaMovementGate, 0.0f, 0.5f)`
- `SliderFloat("Smoothing", &profile.aaSmoothing, 0.0f, 0.9f)`
- 状态行：`aaActive ? "AA: ON (in bubble)" : "AA: idle"`（颜色区分）

签名新增的 `bool aaActive` 参数接 `m_trainer.isAABubbleActive()`。

### 7. `src/App.cpp`
- `m_ui.render(...)` 调用增加 `m_trainer.isAABubbleActive()` 实参（对应新 `aaActive` 形参）。
- `applyConfig` / `syncConfigFromUI` **不改**（AA 走 Profile，不走 Config）。

### 8. `Makefile`
**无需改动** —— `SRC_FILES := $(wildcard $(SRC_DIR)/*.cpp)` 自动纳入 `AimAssist.cpp`；`-MMD -MP` 自动生成其依赖。

### 9. `CLAUDE.md`（第 0 步，spec 对账，建议执行）
在第二章「严禁实现」列表后加一条 scoped exception：
> 例外：本项目作为离线训练器，允许在**手柄瞄准路径**中对**自身模拟目标**实现 Apex 风格的辅助瞄准模拟（Slowdown + Rotational）。此 AA 仅作用于训练器内部镜头行为，不读取、不修改、不注入任何外部游戏（包括 Apex）。前述 Memory Read / Hook / 注入 / Overlay 等针对真实游戏的禁令不受影响。

---

## 验证

1. **构建**：`mingw32-make clean && mingw32-make` —— 期望 0 错误（仅既存 Wconversion/imgui 无害警告），生成 `ApexStickTrainer.exe`。
2. **手柄路径 AA**（需接 Xbox 手柄，Tab 切到 Controller）：
   - 准星靠近目标但不直接命中 → 触感明显变"粘"（减速/stickiness 生效）。`aaStickiness` 调到 1.0 时摇杆几乎推不动准星离目标，调到 0 时无粘感。
   - 目标横向移动 + 推左摇杆 → 镜头自动小幅跟随（旋转/速度跟随生效）；松开左摇杆静止 → 旋转消失但减速仍在（验证 movement gate）。
   - 把 `aaPullGain` 从 0 调大 → 准星偏在 bubble 边缘时会被硬拽向目标中心（位置 pull 生效）；调回 0 → 无拽感。
   - 准星移出 Bubble → AA **立即**停止（M1 验证：无旋转尾巴）。
   - UI「Aim Assist」折叠头里 `aaActive` 状态行实时变化。
3. **鼠标路径无 AA**：Tab 切 Mouse，同样靠近目标 → 无任何粘滞/自动跟随（确认 `updateWithMouse` 未被波及）。
4. **参数持久化**：ESC 调参（含 stickiness/pullGain）→ Save → 重启 → Reload，AA 参数恢复；旧 profile JSON 无 AA 键也能正常加载。
5. **符号自检**：目标向右移时镜头向右跟、向上跳时镜头向上跟、pull 项把偏左的准星向右拽回中心（若方向反了则相应符号需翻 —— 但子代理已对照源码验证速度跟随符号正确，pull 项符号按同套约定推导）。

> 运行时验证需 GUI，本环境无法驱动；以上为构建通过后用户侧手测清单。

---

## 执行顺序（建议）

1. **CLAUDE.md** 加 scoped exception（第 0 步，用户确认后再做）。
2. `src/App.hpp` — 加 AimProfile AA 参数、AimAssist 类声明、Target::getVelocity、Trainer::m_aimAssist/isAABubbleActive、UI::render 签名。
3. `src/AimAssist.cpp`（新）— 实现 apply()。
4. `src/Target.cpp` — 加 getVelocity。
5. `src/Trainer.cpp` — update() 接线。
6. `src/Profile.cpp` — 9 个 AA 参数 load/save/reload + 两个 default 函数。
7. `src/UI.cpp` — Aim Assist 折叠头 + 签名。
8. `src/App.cpp` — m_ui.render 实参。
9. `mingw32-make clean && mingw32-make` 构建验证。
10. 用户侧手测清单（见上「验证」）。

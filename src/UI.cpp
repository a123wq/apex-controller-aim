#include "App.hpp"
#include <imgui.h>
#include <cstdio>
#include <cmath>

UI::UI() : m_wantsInput(false) {}

// Draw text that reads as bold by stamping it at several small offsets
// (a poor-man's bold / outline) on top of a drop shadow. No font file needed.
// `scale` enlarges the text for the countdown number.
static void drawBoldText(ImDrawList* dl, const ImVec2& base, ImU32 color,
                         ImU32 shadow, const char* text, float scale = 1.0f)
{
    auto off = [](const ImVec2& p, float dx, float dy) {
        return ImVec2(p.x + dx, p.y + dy);
    };
    if (scale != 1.0f) dl->ChannelsSetCurrent(0); // no-op guard; keeps state tidy
    // Drop shadow for legibility.
    dl->AddText(nullptr, std::round(ImGui::GetFontSize() * scale),
                off(base, 2, 2), shadow, text);
    // Outline stamp → bold weight.
    dl->AddText(nullptr, std::round(ImGui::GetFontSize() * scale), off(base, -1, 0), color, text);
    dl->AddText(nullptr, std::round(ImGui::GetFontSize() * scale), off(base,  1, 0), color, text);
    dl->AddText(nullptr, std::round(ImGui::GetFontSize() * scale), off(base,  0, -1), color, text);
    dl->AddText(nullptr, std::round(ImGui::GetFontSize() * scale), off(base,  0,  1), color, text);
    dl->AddText(nullptr, std::round(ImGui::GetFontSize() * scale), base, color, text);
}

static const char* modeName(GameModeId id) {
    switch (id) {
        case GameModeId::Tracking:      return "Tracking";
        case GameModeId::ThreeTarget:   return "Three Target";
        case GameModeId::SixTarget:      return "Six Target";
        case GameModeId::CoverTracking: return "Cover Tracking";
        case GameModeId::PillarPatrol:  return "Pillar Patrol";
        case GameModeId::FreeOrbit:     return "360 Free Orbit";
    }
    return "?";
}

void UI::render(AimProfile& profile,
                const std::vector<std::string>& profiles,
                int& currentIndex,
                Profile& profileManager,
                bool& quitRequested,
                InputMode inputMode,
                float& mouseSensX,
                float& mouseSensY,
                AppConfig& config,
                GameModeId modeId,
                RoundPhase phase,
                int countdown,
                double roundTimeLeft,
                const ModeScore& score,
                bool aaActive,
                GameModeId& modeSwitch,
                bool showParams) {
    quitRequested = false;
    m_wantsInput = false;
    modeSwitch = modeId;  // default: no change

    // ── HUD overlay (top center) — score / countdown / round timer / prompts ─
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float winW = 420.0f;
        const float winH = 92.0f;
        ImVec2 pos(vp->Pos.x + (vp->Size.x - winW) * 0.5f, vp->Pos.y + 8.0f);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                               | ImGuiWindowFlags_NoInputs
                               | ImGuiWindowFlags_NoBackground
                               | ImGuiWindowFlags_NoFocusOnAppearing
                               | ImGuiWindowFlags_NoNav;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
        ImGui::Begin("##ScoreOverlay", nullptr, flags);

        const ImU32 white  = IM_COL32(255, 255, 255, 255);
        const ImU32 shadow = IM_COL32(0, 0, 0, 200);
        const ImU32 accent = IM_COL32(255, 210, 80, 255);   // countdown / timer
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();

        // Line 1: mode + round state.
        char line1[128];
        const char* phaseStr = (phase == RoundPhase::Idle)      ? "Press LMB/R2 to start"
                             : (phase == RoundPhase::Countdown) ? "Get ready..."
                             : (phase == RoundPhase::Playing)   ? "Round running"
                             :                                    "Round over";
        std::snprintf(line1, sizeof(line1), "%s — %s", modeName(modeId), phaseStr);
        drawBoldText(dl, ImVec2(p0.x, p0.y + 4.0f), white, shadow, line1);

        // Line 2: scores / timer depending on mode + phase.
        char line2[128];
        if (phase == RoundPhase::Countdown && countdown > 0) {
            // Big number is drawn separately (below); line2 shows the countdown label.
            std::snprintf(line2, sizeof(line2), "Starting in %d...", countdown);
        } else if (phase == RoundPhase::Playing) {
            std::snprintf(line2, sizeof(line2), "%.0fs left", std::ceil(roundTimeLeft));
        } else {
            // Idle / Finished → show the scores.
            bool shooting = (modeId == GameModeId::ThreeTarget || modeId == GameModeId::SixTarget);
            if (shooting) {
                std::snprintf(line2, sizeof(line2), "Score: %d     Best: %d",
                              score.shootCurrent, score.shootBest);
            } else {
                std::snprintf(line2, sizeof(line2), "Burst: %d   Round: %d   Best: %d",
                              score.trackLongestBurst, score.trackRoundTotal,
                              score.trackBestRound);
            }
        }
        ImVec2 base2(p0.x, p0.y + 30.0f);
        drawBoldText(dl, base2, (phase == RoundPhase::Playing) ? accent : white, shadow, line2);

        // Big countdown number centered, during Countdown.
        if (phase == RoundPhase::Countdown && countdown > 0) {
            char num[8];
            std::snprintf(num, sizeof(num), "%d", countdown);
            ImVec2 ts = ImGui::CalcTextSize(num);
            float sx = ts.x * 4.0f, sy = ts.y * 4.0f;  // scaled text size
            ImVec2 base(vp->Pos.x + (vp->Size.x - sx) * 0.5f,
                        vp->Pos.y + vp->Size.y - sy - 12.0f);
            drawBoldText(dl, base, accent, shadow, num, 4.0f);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    // ── Parameter panel — only when toggled on by ESC ────────────
    if (!showParams) {
        // m_wantsInput stays false → game keeps full control of input.
        return;
    }

    ImGui::Begin("Aim Trainer V1.0");

    // ── Input Mode ────────────────────────────────────────────
    if (ImGui::BeginCombo("Input Mode",
            inputMode == InputMode::Mouse ? "Mouse" : "Controller")) {
        if (ImGui::Selectable("Mouse", inputMode == InputMode::Mouse))
            {} // App owns mode switch; this is display only
        if (ImGui::Selectable("Controller", inputMode == InputMode::Controller))
            {} // App owns mode switch
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "[Tab] to switch");

    // ── Mouse sensitivity (only shown in mouse mode) ────────────
    if (inputMode == InputMode::Mouse) {
        ImGui::Text("Sensitivity (degrees/pixel)");
        ImGui::PushID("sens");
        ImGui::SliderFloat("Yaw  ##s",   &mouseSensX, 0.01f, 2.00f, "%.3f");
        ImGui::SliderFloat("Pitch ##s", &mouseSensY, 0.01f, 2.00f, "%.3f");
        ImGui::PopID();
        ImGui::Separator();
    }

    // ── Game Mode selector ───────────────────────────────────────
    if (ImGui::BeginCombo("Game Mode", modeName(modeId))) {
        for (int i = 0; i <= static_cast<int>(GameModeId::FreeOrbit); ++i) {
            GameModeId gid = static_cast<GameModeId>(i);
            bool selected = (gid == modeId);
            if (ImGui::Selectable(modeName(gid), selected)) {
                modeSwitch = gid;  // App applies after render
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Separator();

    // ── Profile selector ───────────────────────────────────────
    if (ImGui::BeginCombo("Profile", profiles.empty() ? "None" : profiles[currentIndex].c_str())) {
        for (int i = 0; i < static_cast<int>(profiles.size()); i++) {
            bool selected = (i == currentIndex);
            if (ImGui::Selectable(profiles[i].c_str(), selected)) {
                currentIndex = i;
                profileManager.load(profiles[i]);
                profile = profileManager.getCurrent();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // ── Response curve parameters ─────────────────────────────
    ImGui::SliderFloat("Deadzone",        &profile.deadzone,          0.000f, 0.300f, "%.3f");
    ImGui::SliderFloat("Outer Deadzone",  &profile.outerDeadzone,    0.000f, 0.100f, "%.3f");
    ImGui::SliderFloat("Curve Exponent",  &profile.curveExponent,     1.000f, 3.000f, "%.2f");
    ImGui::SliderFloat("Micro Aim",       &profile.microAimStrength,  0.000f, 1.000f, "%.2f");
    ImGui::SliderFloat("Micro Threshold", &profile.microAimThreshold, 0.000f, 0.500f, "%.3f");
    ImGui::SliderFloat("Input Smooth",    &profile.inputSmoothing,    0.000f, 0.950f, "%.2f");
    ImGui::SliderFloat("Yaw Speed",       &profile.maxYawSpeed,       60.0f,  720.0f, "%.0f");
    ImGui::SliderFloat("Pitch Speed",     &profile.maxPitchSpeed,     60.0f,  600.0f, "%.0f");
    ImGui::SliderFloat("ADS",             &profile.adsMultiplier,     0.100f, 1.000f, "%.2f");
    ImGui::SliderFloat("Acceleration",    &profile.acceleration,    0.000f, 1.000f, "%.2f");
    ImGui::SliderFloat("Deceleration",    &profile.deceleration,     0.000f, 1.000f, "%.2f");

    ImGui::Separator();

    // ── Aim Assist (controller path only) ─────────────────────
    if (ImGui::CollapsingHeader("Aim Assist")) {
        ImGui::Checkbox("Enable AA", &profile.aaEnabled);
        ImGui::SliderFloat("Bubble Angle (deg)", &profile.aaBubbleAngle, 0.5f, 20.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Half-angle of the invisible AA bubble around the target. Bigger = engages sooner.");
        ImGui::SliderFloat("Max Distance", &profile.aaMaxDistance, 10.0f, 120.0f, "%.0f m");
        ImGui::SliderFloat("Stickiness", &profile.aaStickiness, 0.0f, 1.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Slowdown axis: 0 = not sticky (no slowdown), 1 = most sticky (stick 100%% -> camera 40%%). Default 0.30.");
        ImGui::SliderFloat("Rotational Gain", &profile.aaRotationalGain, 0.0f, 0.8f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rotational axis: fraction of the target's angular velocity the camera auto-follows. PC ~0.4, console ~0.6.");
        ImGui::SliderFloat("Pull Gain", &profile.aaPullGain, 0.0f, 2.0f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Hard pull toward the bubble center (stronger the further off-center). 0 = off (default). Try ~0.5 for a sticky-lock feel.");
        ImGui::SliderFloat("Rot Max Speed", &profile.aaRotMaxSpeed, 0.0f, 60.0f, "%.1f deg/s");
        ImGui::SliderFloat("Movement Gate", &profile.aaMovementGate, 0.0f, 0.5f, "%.2f");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Left-stick/WASD magnitude below which rotational AA fades to zero (Apex movement gate). Slowdown is NOT gated.");
        ImGui::SliderFloat("Smoothing", &profile.aaSmoothing, 0.0f, 0.9f, "%.2f");
        ImGui::TextColored(aaActive ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
                                    : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            aaActive ? "AA: ON (in bubble)" : "AA: idle");
        ImGui::TextDisabled("Controller path only. Mouse aim is unaffected.");
    }

    ImGui::Separator();

    // ── Per-mode parameters (bound to AppConfig) ───────────────
    if (ImGui::CollapsingHeader("Mode Parameters")) {
        // Round settings (all modes).
        ImGui::SliderFloat("Round Duration", &config.roundDuration, 10.0f, 180.0f, "%.0f s");
        ImGui::SliderFloat("Countdown",       &config.countdownSec,   1.0f, 10.0f, "%.0f s");
        ImGui::Separator();

        const bool trackingLike = (modeId == GameModeId::Tracking
                                || modeId == GameModeId::CoverTracking
                                || modeId == GameModeId::PillarPatrol
                                || modeId == GameModeId::FreeOrbit);

        if (trackingLike) {
            ImGui::Text("Target (tracking family)");
            ImGui::Combo("Target Model", reinterpret_cast<int*>(&config.targetModel),
                         "Sphere\0Capsule\0");
            ImGui::SliderFloat("Target Radius",   &config.targetRadius,   0.15f, 1.5f, "%.2f");
            ImGui::SliderFloat("Min Speed",       &config.targetMinSpeed,  0.5f,  12.0f, "%.1f");
            ImGui::SliderFloat("Max Speed",       &config.targetMaxSpeed, 1.0f,  20.0f, "%.1f");
            ImGui::SliderFloat("Bound X",          &config.targetBoundX,   2.0f,  10.0f, "%.1f");
            ImGui::SliderFloat("Bound Z",          &config.targetBoundZ,   1.0f,  14.0f, "%.1f");
            ImGui::SliderFloat("Base Depth (Z)",   &config.targetBaseZ,   -20.0f, -3.0f, "%.1f");
            ImGui::Combo("Strafe Speed", &config.strafeSpeedMode,
                         "Walk\0Run\0Random\0");
            ImGui::Checkbox("Hopping", &config.jumpEnabled);
            ImGui::Separator();
            ImGui::Text("Bounce Physics");
            ImGui::SliderFloat("Restitution",  &config.bounceRestitution, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Hop Velocity", &config.bounceHeight,     1.0f, 8.0f, "%.1f m/s");
            ImGui::SliderFloat("Gravity",       &config.bounceGravity,    -20.0f, -2.0f, "%.1f");
        }

        if (modeId == GameModeId::ThreeTarget || modeId == GameModeId::SixTarget) {
            ImGui::Text("Grid (shooting family — auto-fit to screen)");
            ImGui::SliderFloat("Grid Radius",  &config.gridRadius,  0.08f, 0.40f, "%.2f");
            ImGui::SliderFloat("Grid Depth Z", &config.gridCenterZ, -20.0f, -5.0f, "%.1f");
            ImGui::TextDisabled("Layout (20x5) auto-fits the screen at this depth.");
        }

        if (modeId == GameModeId::CoverTracking) {
            ImGui::Separator();
            ImGui::Text("Cover Boxes");
            ImGui::SliderInt("Cover Count", &config.coverCount, 1, 6);
            ImGui::SliderFloat("Cover Half X", &config.coverHalfX, 0.2f, 4.0f, "%.2f");
            ImGui::SliderFloat("Cover Half Y", &config.coverHalfY, 0.2f, 4.0f, "%.2f");
            ImGui::SliderFloat("Cover Half Z", &config.coverHalfZ, 0.1f, 2.0f, "%.2f");
        }

        if (modeId == GameModeId::PillarPatrol) {
            ImGui::Separator();
            ImGui::Text("Pillar");
            ImGui::SliderFloat("Pillar Height", &config.pillarHeight, 1.0f, 4.0f, "%.2f");
            ImGui::SliderFloat("Pillar Radius", &config.pillarRadius, 0.10f, 0.60f, "%.2f");
            ImGui::SliderFloat("Pillar Speed",  &config.pillarSpeed,  1.0f, 12.0f, "%.1f");
        }

        if (modeId == GameModeId::FreeOrbit) {
            ImGui::Separator();
            ImGui::Text("Free Orbit");
            ImGui::SliderFloat("Orbit Speed", &config.freeOrbitSpeed, 1.0f, 15.0f, "%.1f");
        }

        ImGui::Separator();
        ImGui::TextDisabled("Mode geometry edits apply on next round / mode switch.");
    }

    ImGui::Separator();

    // ── Profile actions ────────────────────────────────────────
    if (ImGui::Button("Save")) {
        profileManager.setCurrent(profile);
        profileManager.save(profile.name);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        profileManager.reload();
        profile = profileManager.getCurrent();
        for (int i = 0; i < static_cast<int>(profiles.size()); i++) {
            if (profiles[i] == profileManager.getCurrentName()) {
                currentIndex = i; break;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        profile = profileManager.getCurrent();
    }
    ImGui::SameLine();
    if (ImGui::Button("Quit")) {
        quitRequested = true;
    }

    ImGui::Separator();

    // ── Control hints ─────────────────────────────────────────
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "ESC: Close panel (resume)");
    const bool shooting = (modeId == GameModeId::ThreeTarget || modeId == GameModeId::SixTarget);
    if (inputMode == InputMode::Mouse) {
        ImGui::Text("Mouse: Aim (Yaw/Pitch)");
        ImGui::Text("WASD or Arrows: Move");
        ImGui::Text("Space: Jump");
        ImGui::Text("Tab: Switch to Controller");
        ImGui::Text("G: Toggle mouse grab");
        if (shooting) ImGui::Text("LMB: Fire (start round)");
        else          ImGui::Text("LMB: ADS / start round (sens x%.2f)", profile.adsMultiplier);
    } else {
        ImGui::Text("Right Stick: Aim (Yaw/Pitch)");
        ImGui::Text("Left Stick: Move");
        ImGui::Text("A: Jump");
        if (shooting) {
            ImGui::Text("RT/R2: Fire (start round)");
            ImGui::Text("(ADS disabled in shooting modes)");
        } else {
            ImGui::Text("LT/RT: ADS / start round");
        }
        ImGui::Text("Tab: Switch to Mouse");
    }

    // Only steal input when the user is actively interacting with a widget
    // (dragging a slider, typing, etc.) — NOT merely when the window is focused.
    m_wantsInput = ImGui::IsAnyItemActive();
    ImGui::End();
}

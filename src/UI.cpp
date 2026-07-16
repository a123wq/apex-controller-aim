#include "App.hpp"
#include <imgui.h>
#include <cstdio>

UI::UI() : m_wantsInput(false) {}

// Draw text that reads as bold by stamping it at several small offsets
// (a poor-man's bold / outline) on top of a drop shadow. No font file needed.
static void drawBoldText(ImDrawList* dl, const ImVec2& base, ImU32 color,
                         ImU32 shadow, const char* text)
{
    auto off = [](const ImVec2& p, float dx, float dy) {
        return ImVec2(p.x + dx, p.y + dy);
    };
    // Drop shadow for legibility.
    dl->AddText(off(base, 2, 2), shadow, text);
    // Outline stamp → bold weight.
    dl->AddText(off(base, -1, 0), color, text);
    dl->AddText(off(base,  1, 0), color, text);
    dl->AddText(off(base,  0, -1), color, text);
    dl->AddText(off(base,  0,  1), color, text);
    dl->AddText(base, color, text);
}

void UI::render(AimProfile& profile,
                const std::vector<std::string>& profiles,
                int& currentIndex,
                Profile& profileManager,
                bool& quitRequested,
                InputMode inputMode,
                float& mouseSensX,
                float& mouseSensY,
                Target& target,
                bool aimedAtTarget,
                int currentScore,
                int bestScore,
                bool showParams) {
    quitRequested = false;
    m_wantsInput = false;

    // ── Score overlay (top center, white bold) ───────────────────
    // A borderless, click-through window pinned to the top of the viewport.
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        const float winW = 380.0f;
        const float winH = 70.0f;
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

        char scoreText[96];
        std::snprintf(scoreText, sizeof(scoreText),
                      "Score: %d     Best: %d", currentScore, bestScore);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 textSize = ImGui::CalcTextSize(scoreText);
        ImVec2 base(p0.x, p0.y + 6.0f);

        drawBoldText(dl, base, white, shadow, scoreText);

        // Aim status line (green when on target).
        const char* aimStr = aimedAtTarget ? "ON TARGET" : "tracking...";
        ImU32 aimCol = aimedAtTarget ? IM_COL32(120, 255, 120, 255)
                                     : IM_COL32(190, 190, 190, 255);
        ImVec2 aimPos(p0.x, base.y + textSize.y + 6.0f);
        drawBoldText(dl, aimPos, aimCol, shadow, aimStr);

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
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f),
        inputMode == InputMode::Mouse ? "[Tab] to switch" : "[Tab] to switch");

    // ── Mouse sensitivity (only shown in mouse mode) ────────────
    if (inputMode == InputMode::Mouse) {
        ImGui::Text("Sensitivity (degrees/pixel)");
        ImGui::PushID("sens");
        ImGui::SliderFloat("Yaw  ##s",   &mouseSensX, 0.01f, 2.00f, "%.3f");
        ImGui::SliderFloat("Pitch ##s", &mouseSensY, 0.01f, 2.00f, "%.3f");
        ImGui::PopID();
        ImGui::Separator();
    }

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
    ImGui::SliderFloat("Curve Exponent",  &profile.curveExponent,    1.000f, 3.000f, "%.2f");
    ImGui::SliderFloat("Micro Aim",       &profile.microAimStrength, 0.000f, 1.000f, "%.2f");
    ImGui::SliderFloat("Micro Threshold", &profile.microAimThreshold,0.000f, 0.500f, "%.3f");
    ImGui::SliderFloat("Input Smooth",    &profile.inputSmoothing,   0.000f, 0.950f, "%.2f");
    ImGui::SliderFloat("Yaw Speed",       &profile.maxYawSpeed,      60.0f,  720.0f, "%.0f");
    ImGui::SliderFloat("Pitch Speed",     &profile.maxPitchSpeed,    60.0f,  600.0f, "%.0f");
    ImGui::SliderFloat("ADS",             &profile.adsMultiplier,    0.100f, 1.000f, "%.2f");
    ImGui::SliderFloat("Acceleration",    &profile.acceleration,   0.000f, 1.000f, "%.2f");
    ImGui::SliderFloat("Deceleration",    &profile.deceleration,    0.000f, 1.000f, "%.2f");

    ImGui::Separator();

    // ── Target (sphere) parameters ────────────────────────────
    if (ImGui::CollapsingHeader("Target")) {
        ImGui::TextColored(aimedAtTarget ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f)
                                         : ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
            aimedAtTarget ? "ON TARGET (target will break away)" : "not aimed at target");

        // Target model selector (Sphere / Capsule)
        const char* curModel = (target.getModel() == TargetModel::Capsule) ? "Capsule" : "Sphere";
        if (ImGui::BeginCombo("Target Model", curModel)) {
            if (ImGui::Selectable("Sphere", target.getModel() == TargetModel::Sphere)) {
                target.setModel(TargetModel::Sphere);
                target.reset();
            }
            if (ImGui::Selectable("Capsule", target.getModel() == TargetModel::Capsule)) {
                target.setModel(TargetModel::Capsule);
                target.reset();
            }
            ImGui::EndCombo();
        }

        if (target.getModel() == TargetModel::Capsule) {
            ImGui::SliderFloat("Capsule Height", &target.capsuleHeight, 0.6f, 3.0f, "%.2f");
            ImGui::SliderFloat("Capsule Radius", &target.stateRef().radius, 0.15f, 1.0f, "%.2f");
        } else {
            ImGui::SliderFloat("Sphere Radius", &target.stateRef().radius, 0.15f, 1.5f, "%.2f");
        }
        ImGui::Separator();
        ImGui::SliderFloat("Min Speed",  &target.minSpeed, 0.5f, 12.0f, "%.1f");
        ImGui::SliderFloat("Max Speed",  &target.maxSpeed, 1.0f, 20.0f, "%.1f");
        ImGui::SliderFloat("Bound X",    &target.boundX,   2.0f, 30.0f, "%.1f");
        ImGui::SliderFloat("Bound Z",    &target.boundZ,   1.0f, 15.0f, "%.1f");
        ImGui::Separator();
        ImGui::Text("Bounce Physics");
        ImGui::SliderFloat("Restitution",   &target.bounceRestitution, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Bounce Height", &target.bounceHeight, 0.5f, 5.0f, "%.1f m");
        ImGui::Separator();
        ImGui::Text("Idle re-direction (random every, when NOT aimed)");
        ImGui::SliderFloat("Idle Min", &target.waitTimeMin, 0.1f, 4.0f, "%.2f s");
        ImGui::SliderFloat("Idle Max", &target.waitTimeMax, 0.2f, 6.0f, "%.2f s");
        ImGui::Text("Aimed escape window (holds ONE direction this long)");
        ImGui::SliderFloat("Aim Redirect Min", &target.aimRedirectMin, 0.5f, 8.0f, "%.2f s");
        ImGui::SliderFloat("Aim Redirect Max", &target.aimRedirectMax, 1.0f, 10.0f, "%.2f s");
        ImGui::SliderFloat("Aim Loss Grace",  &target.aimLossGraceTime, 0.0f, 1.0f, "%.2f s");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sticky aim-lock: brief crosshair drop-off shorter than this does not release the target. Stops edge-jitter from re-rolling the escape direction.");
        ImGui::SliderFloat("Aim Speed Boost",  &target.aimSpeedBoost, 1.0f, 3.0f, "%.2fx");
        if (ImGui::Button("Respawn Target")) {
            target.reset();
        }
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
    if (inputMode == InputMode::Mouse) {
        ImGui::Text("Mouse: Aim (Yaw/Pitch)");
        ImGui::Text("WASD or Arrows: Move");
        ImGui::Text("Space: Jump");
        ImGui::Text("Tab: Switch to Controller");
        ImGui::Text("G: Toggle mouse grab");
        ImGui::Text("LMB: ADS (sensitivity x%.2f)", profile.adsMultiplier);
    } else {
        ImGui::Text("Right Stick: Aim (Yaw/Pitch)");
        ImGui::Text("Left Stick: Move");
        ImGui::Text("A: Jump");
        ImGui::Text("LT/RT: Movement / ADS");
        ImGui::Text("Tab: Switch to Mouse");
    }

    // Only steal input when the user is actively interacting with a widget
    // (dragging a slider, typing, etc.) — NOT merely when the window is focused.
    // Otherwise a focused ImGui window permanently blocks mouse-look.
    m_wantsInput = ImGui::IsAnyItemActive();
    ImGui::End();
}
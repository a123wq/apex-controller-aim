// =============================================================================
// App.cpp — SDL2 + OpenGL 3.3 + ImGui init, main loop, module orchestration
// Supports both Xbox controller (XInput) and mouse aiming
// =============================================================================
#define SDL_MAIN_HANDLED

#include "App.hpp"
#include "gl33.h"
#include <SDL.h>
#include <imgui.h>

#if __has_include("imgui_impl_sdl2.h")
    #include "imgui_impl_sdl2.h"
    #include "imgui_impl_opengl3.h"
    #define HAS_IMGUI_BACKENDS 1
#else
    #define HAS_IMGUI_BACKENDS 0
#endif

// =============================================================================
static void sdlDie(const char* msg) {
    fprintf(stderr, "[App] %s: %s\n", msg, SDL_GetError());
    throw std::runtime_error(msg);
}

// =============================================================================
App::App() {
    diag("[App] ctor: initSDL");
    initSDL();
    diag("[App] ctor: initOpenGL");
    initOpenGL();
    diag("[App] ctor: initImGui");
    initImGui();
    diag("[App] ctor: initModules");
    initModules();
    // Load persisted user settings (creates the file if absent), then apply
    // them to the trainer target + mouse sensitivity.
    m_config.load();
    applyConfig();
    diag("[App] ctor: mouse capture");
    if (m_window) {
        SDL_SetWindowGrab(m_window, SDL_TRUE);
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_ShowCursor(SDL_DISABLE);
    }
    m_lastTime = SDL_GetPerformanceCounter();
    fprintf(stderr, "[App] Startup complete (mouse mode enabled)\n");
    diag("[App] ctor: done");
}

// =============================================================================
App::~App() {
    if (m_window) {
        SDL_SetWindowGrab(m_window, SDL_FALSE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_ShowCursor(SDL_ENABLE);
    }
    #if HAS_IMGUI_BACKENDS
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    #endif
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(m_glContext);
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

// =============================================================================
void App::initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        sdlDie("SDL_Init failed");
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    m_window = SDL_CreateWindow(
        "Apex Stick Trainer V1.0",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
    );
    if (!m_window) sdlDie("SDL_CreateWindow failed");

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext) sdlDie("SDL_GL_CreateContext failed");

    if (SDL_GL_SetSwapInterval(-1) != 0) {
        SDL_GL_SetSwapInterval(0);
    }

    fprintf(stderr, "[App] SDL2 initialized\n");
}

// =============================================================================
void App::initOpenGL() {
    if (!gGL.load()) {
        throw std::runtime_error("Failed to load OpenGL 3.3 function pointers");
    }

    const char* vendor   = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version  = (const char*)glGetString(GL_VERSION);
    fprintf(stderr, "[App] GL Vendor:   %s\n",   vendor   ? vendor   : "null");
    fprintf(stderr, "[App] GL Renderer: %s\n", renderer ? renderer : "null");
    fprintf(stderr, "[App] GL Version:  %s\n",  version  ? version  : "null");

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    int w = 0, h = 0;
    SDL_GetWindowSize(m_window, &w, &h);
    glViewport(0, 0, w, h);

    fprintf(stderr, "[App] OpenGL 3.3 initialized\n");
}

// =============================================================================
void App::initImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    #if HAS_IMGUI_BACKENDS
    ImGui_ImplSDL2_InitForOpenGL(m_window, m_glContext);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    #endif

    fprintf(stderr, "[App] Dear ImGui initialized\n");
}

// =============================================================================
void App::initModules() {
    int w = 0, h = 0;
    SDL_GetWindowSize(m_window, &w, &h);

    if (!m_renderer.init(w, h)) {
        throw std::runtime_error("Renderer::init failed (shader compilation error)");
    }

    fprintf(stderr, "[App] Modules initialized\n");
}

// =============================================================================
// Push persisted config → live state (game mode + mode geometry + mouse sens).
// The trainer rebuilds its active GameMode with the new config snapshot.
// =============================================================================
void App::applyConfig() {
    AppConfig& c = m_config.data();
    // Push mode + geometry into the trainer (rebuilds the active mode).
    m_trainer.setMode(static_cast<GameModeId>(c.gameMode), c);
    m_lastAppliedConfig = c;   // remember what the live mode was built from
    m_mouseSensitivityX = c.mouseSensX;
    m_mouseSensitivityY = c.mouseSensY;
    m_config.clearDirty();
}

// =============================================================================
// Pull live UI-editable values → config, mark dirty so it persists next frame.
// Call after UI::render so slider edits are captured. If the UI changed any
// mode-geometry field (sliders bind directly to m_config.data() by ref), rebuild
// the active GameMode so the edit takes effect IMMEDIATELY — not only on the
// next mode switch.
// =============================================================================
void App::syncConfigFromUI() {
    AppConfig& c = m_config.data();
    c.gameMode = static_cast<int>(m_trainer.getModeId());
    if (c.mouseSensX != m_mouseSensitivityX) { c.mouseSensX = m_mouseSensitivityX; }
    if (c.mouseSensY != m_mouseSensitivityY) { c.mouseSensY = m_mouseSensitivityY; }

    // Live-apply mode-geometry edits: if the user dragged a slider that changed
    // a field the active mode snapshotted at construction, rebuild the mode now.
    if (appConfigDiffers(c, m_lastAppliedConfig)) {
        m_trainer.applyConfig(c);          // rebuilds the active mode with new values
        m_lastAppliedConfig = c;
        m_config.markDirty();              // persist next frame
    }
}

// =============================================================================
void App::run() {
    diag("[App] run: enter loop");
    fprintf(stderr, "[App] Entering main loop\n");

    int loopCount = 0;
    while (m_running) {
        // 1. Event polling
        pollEvents();

        // 2. Frame timing
        uint64_t now = SDL_GetPerformanceCounter();
        double elapsed = static_cast<double>(now - m_lastTime) /
                         static_cast<double>(SDL_GetPerformanceFrequency());
        m_lastTime = now;
        m_deltaTime = std::clamp(elapsed, 0.001, 0.1);

        // 3. Controller update (XInput)
        m_input.update();

        // 3b. Auto switch input mode on hotplug.
        {
            bool conn = m_input.isConnected();
            if (conn && !m_controllerPrev) {
                m_inputMode = InputMode::Controller;
                SDL_SetWindowGrab(m_window, SDL_FALSE);
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
                diagf("[App] controller connected → CONTROLLER mode");
            } else if (!conn && m_controllerPrev && m_inputMode == InputMode::Controller) {
                m_inputMode = InputMode::Mouse;
                SDL_SetWindowGrab(m_window, SDL_TRUE);
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_ShowCursor(SDL_DISABLE);
                diagf("[App] controller disconnected → MOUSE mode");
            }
            m_controllerPrev = conn;
        }

        // 3c. R2/RT (controller fire/shoulder) edge detection. RT is analog, so
        // detect a rising edge (>0.5) when the panel is closed and route it to
        // start-round + fire (the mode decides which applies). Mirrors the LMB
        // path so either input starts/fires.
        if (!m_paramsVisible && m_inputMode == InputMode::Controller) {
            bool rt = m_input.getState().RT > 0.5f;
            if (rt && !m_rtWasDown) {
                m_fireRequested = true;
                m_startRequested = true;
            }
            m_rtWasDown = rt;
        } else {
            m_rtWasDown = false;  // reset so re-entering controller doesn't double-fire
        }

        // 4. ImGui NewFrame
        #if HAS_IMGUI_BACKENDS
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        #endif
        ImGui::NewFrame();

        // 5. WASD movement — ALWAYS runs, never blocked by UI.
        {
            const Uint8* keys = SDL_GetKeyboardState(nullptr);
            StickState& st = const_cast<StickState&>(m_input.getState());
            st.moveX = 0.0f;
            st.moveY = 0.0f;
            if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    st.moveY += 1.0f;
            if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])   st.moveY -= 1.0f;
            if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])   st.moveX -= 1.0f;
            if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT])  st.moveX += 1.0f;
            st.jumpRequested = (keys[SDL_SCANCODE_SPACE] != 0) || st.aButton;

            setCrashStep("[loop] updatePlayer");
            m_trainer.updatePlayer(st, m_deltaTime);
        }

        // 6. Target physics + scoring now live inside the active GameMode, driven
        // by Trainer::update / updateWithMouse (below). No per-target work here.

        // 7. Camera aim rotation — runs unless the parameter panel is open.
        // When the panel is visible the mouse is free (pointer shown) for
        // clicking UI, so we must NOT consume its motion as aim rotation.
        if (m_paramsVisible) {
            // Drain any pending relative motion so the next resume frame
            // doesn't jump the view. Still advance the round/mode so the scene
            // (and round timer) keep ticking behind the panel.
            if (m_inputMode == InputMode::Mouse) {
                SDL_GetRelativeMouseState(nullptr, nullptr);
            }
            m_trainer.advanceRoundAndMode(m_deltaTime);
        } else if (m_inputMode == InputMode::Mouse) {
            int mx = 0, my = 0;
            SDL_GetRelativeMouseState(&mx, &my);
            // ADS in mouse mode: LMB held = ADS (tracking family only). In
            // shooting family LMB fires instead (edge-detected in pollEvents).
            bool tracking = m_trainer.getModeId() != GameModeId::ThreeTarget
                          && m_trainer.getModeId() != GameModeId::SixTarget;
            float adsMult = (tracking && m_lmbDown) ? m_trainer.getProfile().adsMultiplier : 1.0f;
            setCrashStep("[loop] updateWithMouse");
            m_trainer.updateWithMouse(
                m_input.getState(),
                static_cast<float>(mx), static_cast<float>(my),
                m_mouseSensitivityX, m_mouseSensitivityY,
                adsMult, m_deltaTime);
        } else {
            setCrashStep("[loop] update(controller)");
            m_trainer.update(m_input.getState(), m_deltaTime);
        }

        // 7b. Fire / round-start edges (LMB + R2) — handled in pollEvents by
        // setting m_fireRequested / m_startRequested; apply them here so the
        // round/mode state is current.
        if (m_startRequested) {
            m_trainer.requestRoundStart();
            m_startRequested = false;
        }
        if (m_fireRequested) {
            m_trainer.fire();
            m_fireRequested = false;
        }

        // 8. UI render
        setCrashStep("[loop] UI render");
        AimProfile& profile = const_cast<AimProfile&>(m_trainer.getProfileManager().getCurrent());
        const auto& available = m_trainer.getProfileManager().getAvailable();
        std::string currentName = m_trainer.getProfileManager().getCurrentName();

        int currentIndex = 0;
        for (int i = 0; i < static_cast<int>(available.size()); ++i) {
            if (available[i] == currentName) {
                currentIndex = i; break;
            }
        }

        bool quitRequested = false;
        GameModeId modeSwitch = m_trainer.getModeId();
        const RoundManager& rm = m_trainer.getRound();
        m_ui.render(profile, available, currentIndex,
                    m_trainer.getProfileManager(), quitRequested,
                    m_inputMode, m_mouseSensitivityX, m_mouseSensitivityY,
                    m_config.data(),
                    m_trainer.getModeId(), rm.phase, rm.countdownDisplay,
                    rm.playingRemaining(), m_trainer.getScore(),
                    m_trainer.isAABubbleActive(),
                    modeSwitch, m_paramsVisible);

        // 8a. Apply a mode switch requested by the UI Game Mode combo.
        if (modeSwitch != m_trainer.getModeId()) {
            m_config.data().gameMode = static_cast<int>(modeSwitch);
            m_trainer.setMode(modeSwitch, m_config.data());
            m_config.markDirty();
        }

        if (quitRequested) {
            m_running = false; break;
        }

        // 8b. Persist any config edits made via the UI this frame.
        syncConfigFromUI();
        if (m_config.isDirty()) {
            m_config.save();
            m_config.clearDirty();
        }

        // 9. Render scene
        setCrashStep("[loop] ImGui::Render");
        ImGui::Render();
        setCrashStep("[loop] renderFrame");
        renderFrame();

        // 10. Swap buffers
        setCrashStep("[loop] SwapWindow");
        SDL_GL_SwapWindow(m_window);
        setCrashStep("[loop] frame done");
        loopCount++;

        // 11. Frame timing (cap to ~144 FPS)
        frameTiming();

        // 12. FPS reporting every FPS_REPORT_INTERVAL frames
        m_frameCount++;
        m_fpsAccum += 1.0 / m_deltaTime;
        if (m_frameCount % FPS_REPORT_INTERVAL == 0) {
            m_currentFPS = m_fpsAccum / static_cast<double>(FPS_REPORT_INTERVAL);
            fprintf(stderr, "\r[App] FPS: %.0f | dt: %.3fms  ",
                       m_currentFPS, m_deltaTime * 1000.0);
            m_fpsAccum = 0.0;
        }
    }
    fprintf(stderr, "\n[App] Exiting\n");
}

// =============================================================================
void App::pollEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        #if HAS_IMGUI_BACKENDS
        ImGui_ImplSDL2_ProcessEvent(&ev);
        #endif

        switch (ev.type) {
            case SDL_QUIT:
                m_running = false;
                break;

            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_RESIZED) {
                    int w = ev.window.data1, h = ev.window.data2;
                    m_renderer.setWindowSize(w, h);
                    glViewport(0, 0, w, h);
                }
                if (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    if (m_inputMode == InputMode::Mouse) {
                        SDL_SetWindowGrab(m_window, SDL_TRUE);
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                    }
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    m_lmbDown = true;
                    if (m_mouseGrab) {
                        SDL_SetWindowGrab(m_window, SDL_TRUE);
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_ShowCursor(SDL_DISABLE);
                    }
                    // LMB = start round / fire (when the panel is closed). The
                    // current mode decides which: fire() is a no-op unless the
                    // mode's rtIsFire(); requestRoundStart() handles the rest.
                    if (!m_paramsVisible) {
                        m_fireRequested = true;
                        m_startRequested = true;
                    }
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    m_lmbDown = false;
                }
                break;

            case SDL_KEYDOWN: {
                if (ev.key.keysym.sym == SDLK_g && !m_paramsVisible) {
                    // G only toggles grab while the parameter panel is hidden;
                    // when the panel is open the mouse is intentionally free.
                    if (!m_gKeyWasDown) {
                        m_mouseGrab = !m_mouseGrab;
                        if (m_mouseGrab && m_inputMode == InputMode::Mouse) {
                            SDL_SetWindowGrab(m_window, SDL_TRUE);
                            SDL_SetRelativeMouseMode(SDL_TRUE);
                            SDL_ShowCursor(SDL_DISABLE);
                            fprintf(stderr, "\n[App] Mouse GRABBED (aim mode)\n");
                        } else {
                            SDL_SetWindowGrab(m_window, SDL_FALSE);
                            SDL_SetRelativeMouseMode(SDL_FALSE);
                            SDL_ShowCursor(SDL_ENABLE);
                            fprintf(stderr, "\n[App] Mouse RELEASED (pointer visible)\n");
                        }
                    }
                    m_gKeyWasDown = true;
                }
                // ESC: toggle the parameter panel. Opening it releases the mouse
                // (so the pointer appears for clicking UI); closing it re-captures
                // the mouse for aiming.
                if (ev.key.keysym.sym == SDLK_ESCAPE && !ev.key.repeat) {
                    if (!m_escWasDown) {
                        m_paramsVisible = !m_paramsVisible;
                        if (m_paramsVisible) {
                            SDL_SetWindowGrab(m_window, SDL_FALSE);
                            SDL_SetRelativeMouseMode(SDL_FALSE);
                            SDL_ShowCursor(SDL_ENABLE);
                            fprintf(stderr, "\n[App] Params panel OPEN (mouse released)\n");
                        } else {
                            // Resuming play: re-capture mouse if in mouse mode.
                            if (m_inputMode == InputMode::Mouse && m_mouseGrab) {
                                SDL_SetWindowGrab(m_window, SDL_TRUE);
                                SDL_SetRelativeMouseMode(SDL_TRUE);
                                SDL_ShowCursor(SDL_DISABLE);
                            }
                            fprintf(stderr, "\n[App] Params panel CLOSED (resume)\n");
                        }
                    }
                    m_escWasDown = true;
                }
                if (ev.key.keysym.sym == SDLK_TAB && !ev.key.repeat && !m_paramsVisible) {
                    if (m_inputMode == InputMode::Mouse) {
                        m_inputMode = InputMode::Controller;
                        SDL_SetWindowGrab(m_window, SDL_FALSE);
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        SDL_ShowCursor(SDL_ENABLE);
                        fprintf(stderr, "\n[App] Switched to CONTROLLER mode\n");
                    } else {
                        m_inputMode = InputMode::Mouse;
                        SDL_SetWindowGrab(m_window, SDL_TRUE);
                        SDL_SetRelativeMouseMode(SDL_TRUE);
                        SDL_ShowCursor(SDL_DISABLE);
                        fprintf(stderr, "\n[App] Switched to MOUSE mode\n");
                    }
                }
                break;
            }

            case SDL_KEYUP:
                if (ev.key.keysym.sym == SDLK_g) {
                    m_gKeyWasDown = false;
                }
                if (ev.key.keysym.sym == SDLK_ESCAPE) {
                    m_escWasDown = false;
                }
                break;

            default:
                break;
        }
    }
}

// =============================================================================
void App::renderFrame() {
    m_renderer.draw(m_trainer.getCameraState(), m_trainer.getPlayerState(),
                    m_trainer.getSceneView());
    #if HAS_IMGUI_BACKENDS
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    #endif
}

// =============================================================================
void App::frameTiming() {
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t frameTarget = static_cast<uint64_t>(static_cast<double>(freq) * TARGET_DT);
    uint64_t elapsed = SDL_GetPerformanceCounter() - m_lastTime;

    while (elapsed < frameTarget) {
        uint64_t remaining = frameTarget - elapsed;
        uint64_t sleepMs = remaining * 1000 / freq;
        if (sleepMs > 2) {
            SDL_Delay(static_cast<Uint32>(sleepMs - 1));
        } else {
            SDL_Delay(1);
        }
        elapsed = SDL_GetPerformanceCounter() - m_lastTime;
    }
}
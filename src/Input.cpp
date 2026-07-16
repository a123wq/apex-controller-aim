#include "App.hpp"
#include <cmath>

// Windows headers for LoadLibraryA / GetProcAddress / HMODULE / WINAPI.
// SDL.h (included transitively via App.hpp) typically pulls these in, but we
// include <windows.h> explicitly so this file is self-contained. WIN32_LEAN_AND_MEAN
// keeps it light. We do NOT include <Xinput.h> — XInput is loaded dynamically.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// =============================================================================
// XInput — loaded dynamically so the exe has ZERO non-system DLL dependencies.
// We do NOT #include <Xinput.h> and do NOT link -lxinput. Instead we define the
// minimal struct layout ourselves and resolve XInputGetState at runtime via
// LoadLibrary/GetProcAddress. If XINPUT1_3.dll (or XINPUT9_1_0.dll) is absent
// on a clean Windows install, the controller simply reports "not connected"
// and the app keeps running with the mouse — no crash, no missing-DLL dialog.
// =============================================================================

static constexpr int XINPUT_DEADZONE = 8639;

// XInput button bitmask constants (from the XInput API; reproduced here so we
// don't need the header). Layout matches XINPUT_GAMEPAD.wButtons.
static constexpr unsigned short XINPUT_GAMEPAD_DPAD_UP         = 0x0001;
static constexpr unsigned short XINPUT_GAMEPAD_DPAD_DOWN       = 0x0002;
static constexpr unsigned short XINPUT_GAMEPAD_DPAD_LEFT       = 0x0004;
static constexpr unsigned short XINPUT_GAMEPAD_DPAD_RIGHT      = 0x0008;
static constexpr unsigned short XINPUT_GAMEPAD_START           = 0x0010;
static constexpr unsigned short XINPUT_GAMEPAD_BACK            = 0x0020;
static constexpr unsigned short XINPUT_GAMEPAD_LEFT_THUMB      = 0x0040;
static constexpr unsigned short XINPUT_GAMEPAD_RIGHT_THUMB     = 0x0080;
static constexpr unsigned short XINPUT_GAMEPAD_A               = 0x1000;
static constexpr unsigned short XINPUT_GAMEPAD_B               = 0x2000;
static constexpr unsigned short XINPUT_GAMEPAD_X               = 0x4000;
static constexpr unsigned short XINPUT_GAMEPAD_Y               = 0x8000;

// Reproduced struct layouts (identical binary layout to <Xinput.h>).
struct XINPUT_GAMEPAD {
    unsigned short wButtons;
    unsigned char  bLeftTrigger;
    unsigned char  bRightTrigger;
    short          sThumbLX;
    short          sThumbLY;
    short          sThumbRX;
    short          sThumbRY;
};

struct XINPUT_STATE {
    unsigned int   dwPacketNumber;
    XINPUT_GAMEPAD Gamepad;
};

#define XINPUT_ERROR_SUCCESS               0L
#define XINPUT_ERROR_DEVICE_NOT_CONNECTED  1167L

typedef unsigned long (WINAPI *PFN_XInputGetState)(unsigned long dwUserIndex, XINPUT_STATE* pState);

// -----------------------------------------------------------------------------
// Global XInput function pointer, resolved once at Input construction.
// -----------------------------------------------------------------------------
static PFN_XInputGetState g_XInputGetState = nullptr;
static bool g_xinputResolved = false;

static PFN_XInputGetState resolveXInput() {
    // Try several XInput DLL names in order of preference. XINPUT9_1_0.dll is
    // shipped inbox on Windows Vista+ (always present), but only exposes the
    // 1.0 subset — still has XInputGetState, which is all we need.
    const char* candidates[] = {
        "XINPUT1_4.dll",
        "XINPUT1_3.dll",
        "XINPUT9_1_0.dll",
    };
    for (const char* name : candidates) {
        HMODULE h = LoadLibraryA(name);
        if (h) {
            auto fn = reinterpret_cast<PFN_XInputGetState>(
                reinterpret_cast<void*>(GetProcAddress(h, "XInputGetState")));
            if (fn) return fn;
        }
    }
    return nullptr;  // no XInput at all — controller disabled, mouse still works
}

// =============================================================================
Input::Input() : m_connected(false), m_packetNumber(0) {
    g_XInputGetState = resolveXInput();
    g_xinputResolved = (g_XInputGetState != nullptr);
    if (!g_xinputResolved) {
        diagf("[Input] XInput DLL not found — controller disabled (mouse still works)");
    } else {
        diagf("[Input] XInput loaded OK (dynamic, %s)", "resolved");
    }
}

void Input::update() {
    if (!g_xinputResolved || g_XInputGetState == nullptr) {
        m_connected = false;
        m_state = {};
        m_packetNumber = 0;
        return;
    }

    XINPUT_STATE state = {};
    unsigned long result = XINPUT_ERROR_DEVICE_NOT_CONNECTED;
    // Diagnostic: log per-index return values ONCE on the first update.
    static bool diagOnce = false;
    unsigned long foundIndex = 0xFFFFFFFF;
    for (unsigned long i = 0; i < 4; i++) {
        unsigned long r = g_XInputGetState(i, &state);
        if (!diagOnce) {
            diagf("[Input] XInputGetState(%lu) returned %lu", i, r);
        }
        if (r == XINPUT_ERROR_SUCCESS) {
            result = r;
            foundIndex = i;
            break;
        }
    }
    if (!diagOnce) {
        diagOnce = true;
        diagf("[Input] first scan done; connected index = %lu (0xFFFFFFFF=none)",
              foundIndex);
    }

    if (result == XINPUT_ERROR_SUCCESS) {
        m_connected = true;
        if (state.dwPacketNumber != m_packetNumber) {
            m_packetNumber = state.dwPacketNumber;
            const XINPUT_GAMEPAD& gp = state.Gamepad;
            m_state.leftX  = normalizeAxis(gp.sThumbLX);
            m_state.leftY  = normalizeAxis(gp.sThumbLY);
            m_state.rightX = normalizeAxis(gp.sThumbRX);
            m_state.rightY = normalizeAxis(gp.sThumbRY);
            m_state.LT = gp.bLeftTrigger  / 255.0f;
            m_state.RT = gp.bRightTrigger / 255.0f;
            m_state.aButton     = (gp.wButtons & XINPUT_GAMEPAD_A)      != 0;
            m_state.bButton     = (gp.wButtons & XINPUT_GAMEPAD_B)      != 0;
            m_state.xButton     = (gp.wButtons & XINPUT_GAMEPAD_X)      != 0;
            m_state.yButton     = (gp.wButtons & XINPUT_GAMEPAD_Y)      != 0;
            m_state.startButton = (gp.wButtons & XINPUT_GAMEPAD_START)  != 0;
            m_state.backButton  = (gp.wButtons & XINPUT_GAMEPAD_BACK)   != 0;
        }
    } else {
        m_connected = false;
        m_state = {};
        m_packetNumber = 0;
    }
}

float Input::normalizeAxis(int16_t raw) const {
    int absRaw = std::abs(static_cast<int>(raw));
    if (absRaw <= XINPUT_DEADZONE) return 0.0f;
    float sign = (raw > 0) ? 1.0f : -1.0f;
    float magnitude = static_cast<float>(absRaw - XINPUT_DEADZONE) /
                      static_cast<float>(32767 - XINPUT_DEADZONE);
    return sign * magnitude;
}

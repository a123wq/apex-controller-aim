#include "App.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <windows.h>
#include <cstdlib>

static AimProfile defaultClassic43() {
    AimProfile p;
    p.name             = "Classic43";
    p.deadzone          = 0.08f;
    p.outerDeadzone    = 0.02f;
    p.curveExponent    = 2.2f;
    p.microAimStrength  = 0.40f;
    p.microAimThreshold = 0.15f;
    p.inputSmoothing    = 0.12f;
    p.maxYawSpeed      = 420.0f;
    p.maxPitchSpeed    = 360.0f;
    p.adsMultiplier    = 0.65f;
    p.acceleration     = 0.15f;
    p.deceleration     = 0.12f;
    // Aim assist defaults (PC coefficient; see AimProfile in App.hpp).
    p.aaEnabled        = true;
    p.aaBubbleAngle    = 4.0f;
    p.aaMaxDistance    = 60.0f;
    p.aaStickiness     = 0.30f;
    p.aaRotationalGain = 0.40f;
    p.aaPullGain       = 0.0f;
    p.aaRotMaxSpeed    = 25.0f;
    p.aaMovementGate   = 0.15f;
    p.aaSmoothing      = 0.30f;
    return p;
}

static AimProfile defaultLinear43() {
    AimProfile p;
    p.name             = "Linear43";
    p.deadzone          = 0.06f;
    p.outerDeadzone    = 0.01f;
    p.curveExponent    = 1.0f;
    p.microAimStrength  = 0.20f;
    p.microAimThreshold = 0.10f;
    p.inputSmoothing    = 0.05f;
    p.maxYawSpeed      = 420.0f;
    p.maxPitchSpeed    = 360.0f;
    p.adsMultiplier    = 0.65f;
    p.acceleration     = 0.00f;
    p.deceleration     = 0.00f;
    // Aim assist defaults (PC coefficient; see AimProfile in App.hpp).
    p.aaEnabled        = true;
    p.aaBubbleAngle    = 4.0f;
    p.aaMaxDistance    = 60.0f;
    p.aaStickiness     = 0.30f;
    p.aaRotationalGain = 0.40f;
    p.aaPullGain       = 0.0f;
    p.aaRotMaxSpeed    = 25.0f;
    p.aaMovementGate   = 0.15f;
    p.aaSmoothing      = 0.30f;
    return p;
}

Profile::Profile() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash);
    m_profileDir = exeDir + "\\assets\\profiles";
    scanDirectory();
    if (!m_available.empty()) {
        load(m_available[0]);
    } else {
        m_current = defaultClassic43();
    }
}

void Profile::scanDirectory() {
    m_available.clear();
    std::string pattern = m_profileDir + "\\*.json";
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string name = fd.cFileName;
            if (name.size() > 5) {
                name = name.substr(0, name.size() - 5);
                m_available.push_back(name);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
}

std::string Profile::getFilePath(const std::string& name) const {
    return m_profileDir + "\\" + name + ".json";
}

bool Profile::load(const std::string& name) {
    std::ifstream f(getFilePath(name));
    if (!f.is_open()) {
        if (name == "Classic43" || name == "classic43") m_current = defaultClassic43();
        else if (name == "Linear43" || name == "linear43") m_current = defaultLinear43();
        else { m_current = defaultClassic43(); m_current.name = name; }
        m_current.name = name;
        m_currentFile = getFilePath(name);
        save(name);
        m_available.push_back(name);
        return true;
    }
    try {
        std::string jsonStr((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto j = nlohmann::json::parse(jsonStr);
        m_current.name              = j.value("name", name);
        m_current.deadzone          = j.value("deadzone", 0.08f);
        m_current.outerDeadzone    = j.value("outerDeadzone", 0.02f);
        m_current.curveExponent    = j.value("curveExponent", 2.2f);
        m_current.microAimStrength = j.value("microAimStrength", 0.40f);
        m_current.microAimThreshold= j.value("microAimThreshold", 0.15f);
        m_current.inputSmoothing   = j.value("inputSmoothing", 0.12f);
        m_current.maxYawSpeed      = j.value("maxYawSpeed", 420.0f);
        m_current.maxPitchSpeed    = j.value("maxPitchSpeed", 360.0f);
        m_current.adsMultiplier    = j.value("ADSMultiplier", 0.65f);
        m_current.acceleration    = j.value("acceleration", 0.15f);
        m_current.deceleration     = j.value("deceleration", 0.12f);
        // Aim assist — j.value defaults let old profile files without these
        // keys load fine (filled with PC-coefficient defaults). Saving once
        // persists them; no manual JSON editing required.
        m_current.aaEnabled        = j.value("aaEnabled", true);
        m_current.aaBubbleAngle    = j.value("aaBubbleAngle", 4.0f);
        m_current.aaMaxDistance    = j.value("aaMaxDistance", 60.0f);
        m_current.aaStickiness     = j.value("aaStickiness", 0.30f);
        m_current.aaRotationalGain = j.value("aaRotationalGain", 0.40f);
        m_current.aaPullGain       = j.value("aaPullGain", 0.0f);
        m_current.aaRotMaxSpeed    = j.value("aaRotMaxSpeed", 25.0f);
        m_current.aaMovementGate   = j.value("aaMovementGate", 0.15f);
        m_current.aaSmoothing      = j.value("aaSmoothing", 0.30f);
        m_currentFile = getFilePath(name);
        return true;
    } catch (...) { return false; }
}

bool Profile::save(const std::string& name) {
    CreateDirectoryA(m_profileDir.c_str(), nullptr);
    nlohmann::json j;
    j["name"]             = m_current.name;
    j["deadzone"]          = m_current.deadzone;
    j["outerDeadzone"]     = m_current.outerDeadzone;
    j["curveExponent"]     = m_current.curveExponent;
    j["microAimStrength"]  = m_current.microAimStrength;
    j["microAimThreshold"] = m_current.microAimThreshold;
    j["inputSmoothing"]   = m_current.inputSmoothing;
    j["maxYawSpeed"]       = m_current.maxYawSpeed;
    j["maxPitchSpeed"]     = m_current.maxPitchSpeed;
    j["ADSMultiplier"]     = m_current.adsMultiplier;
    j["acceleration"]      = m_current.acceleration;
    j["deceleration"]      = m_current.deceleration;
    j["aaEnabled"]         = m_current.aaEnabled;
    j["aaBubbleAngle"]     = m_current.aaBubbleAngle;
    j["aaMaxDistance"]     = m_current.aaMaxDistance;
    j["aaStickiness"]      = m_current.aaStickiness;
    j["aaRotationalGain"] = m_current.aaRotationalGain;
    j["aaPullGain"]        = m_current.aaPullGain;
    j["aaRotMaxSpeed"]     = m_current.aaRotMaxSpeed;
    j["aaMovementGate"]   = m_current.aaMovementGate;
    j["aaSmoothing"]      = m_current.aaSmoothing;
    std::string path = getFilePath(name.empty() ? m_current.name : name);
    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << j.dump(4);
    m_currentFile = path;
    return true;
}

bool Profile::reload() {
    if (m_currentFile.empty()) return false;
    std::ifstream f(m_currentFile);
    if (!f.is_open()) return false;
    try {
        std::string jsonStr((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto j = nlohmann::json::parse(jsonStr);
        m_current.name              = j.value("name", m_current.name);
        m_current.deadzone          = j.value("deadzone", m_current.deadzone);
        m_current.outerDeadzone    = j.value("outerDeadzone", m_current.outerDeadzone);
        m_current.curveExponent    = j.value("curveExponent", m_current.curveExponent);
        m_current.microAimStrength = j.value("microAimStrength", m_current.microAimStrength);
        m_current.microAimThreshold= j.value("microAimThreshold", m_current.microAimThreshold);
        m_current.inputSmoothing   = j.value("inputSmoothing", m_current.inputSmoothing);
        m_current.maxYawSpeed      = j.value("maxYawSpeed", m_current.maxYawSpeed);
        m_current.maxPitchSpeed    = j.value("maxPitchSpeed", m_current.maxPitchSpeed);
        m_current.adsMultiplier    = j.value("ADSMultiplier", m_current.adsMultiplier);
        m_current.acceleration    = j.value("acceleration", m_current.acceleration);
        m_current.deceleration     = j.value("deceleration", m_current.deceleration);
        // Aim assist — keep existing values if keys are absent (old files).
        m_current.aaEnabled        = j.value("aaEnabled", m_current.aaEnabled);
        m_current.aaBubbleAngle    = j.value("aaBubbleAngle", m_current.aaBubbleAngle);
        m_current.aaMaxDistance    = j.value("aaMaxDistance", m_current.aaMaxDistance);
        m_current.aaStickiness     = j.value("aaStickiness", m_current.aaStickiness);
        m_current.aaRotationalGain = j.value("aaRotationalGain", m_current.aaRotationalGain);
        m_current.aaPullGain       = j.value("aaPullGain", m_current.aaPullGain);
        m_current.aaRotMaxSpeed    = j.value("aaRotMaxSpeed", m_current.aaRotMaxSpeed);
        m_current.aaMovementGate   = j.value("aaMovementGate", m_current.aaMovementGate);
        m_current.aaSmoothing      = j.value("aaSmoothing", m_current.aaSmoothing);
        return true;
    } catch (...) { return false; }
}

void Profile::refreshList() { scanDirectory(); }

// =============================================================================
// Config.cpp — persistent user settings in apexStickTrainer.conf next to exe.
// =============================================================================
#include "App.hpp"
#include "nlohmann/json.hpp"
#include <fstream>
#include <windows.h>

Config::Config() {
    // Resolve the config file path = <exeDir>/apexStickTrainer.conf
    char exePath[MAX_PATH];
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    std::string exeDir = exePath;
    size_t lastSlash = exeDir.find_last_of("\\/");
    if (lastSlash != std::string::npos) exeDir = exeDir.substr(0, lastSlash);
    m_filePath = exeDir + "\\apexStickTrainer.conf";
}

bool Config::load() {
    std::ifstream f(m_filePath);
    if (!f.is_open()) {
        // No config yet → write defaults and return success.
        save();
        return true;
    }
    try {
        std::string jsonStr((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        auto j = nlohmann::json::parse(jsonStr);
        m_data.targetModel    = (j.value("targetModel", 0) == 1)
                               ? TargetModel::Capsule : TargetModel::Sphere;
        m_data.targetRadius   = j.value("targetRadius", 0.45f);
        m_data.targetMinSpeed = j.value("targetMinSpeed", 2.0f);
        m_data.targetMaxSpeed = j.value("targetMaxSpeed", 6.0f);
        m_data.aimRedirectMin = j.value("aimRedirectMin", 4.0f);
        m_data.aimRedirectMax = j.value("aimRedirectMax", 4.0f);
        m_data.mouseSensX     = j.value("mouseSensX", 0.15f);
        m_data.mouseSensY     = j.value("mouseSensY", 0.15f);

        // Game mode + round + per-mode geometry. j.value defaults let old conf
        // files (without these keys) load with the defaults from AppConfig.
        m_data.gameMode        = j.value("gameMode", 0);
        m_data.strafeSpeedMode = j.value("strafeSpeedMode", 0);
        m_data.roundDuration    = j.value("roundDuration", 60.0f);
        m_data.countdownSec      = j.value("countdownSec", 3.0f);
        m_data.bounceRestitution = j.value("bounceRestitution", 0.7f);
        m_data.bounceGravity      = j.value("bounceGravity", -9.8f);
        m_data.bounceHeight       = j.value("bounceHeight", 1.5f);
        m_data.jumpEnabled        = j.value("jumpEnabled", true);
        m_data.targetBoundX       = j.value("targetBoundX", 8.0f);
        m_data.targetBoundZ       = j.value("targetBoundZ", 7.0f);
        m_data.targetBaseZ        = j.value("targetBaseZ", -10.0f);
        m_data.gridSpacing  = j.value("gridSpacing", 0.9f);
        m_data.gridRadius   = j.value("gridRadius", 0.18f);
        m_data.gridCenterY  = j.value("gridCenterY", 4.0f);
        m_data.gridCenterZ  = j.value("gridCenterZ", -12.0f);
        m_data.coverCount  = j.value("coverCount", 3);
        m_data.coverHalfX  = j.value("coverHalfX", 1.5f);
        m_data.coverHalfY  = j.value("coverHalfY", 2.0f);
        m_data.coverHalfZ  = j.value("coverHalfZ", 0.4f);
        m_data.pillarHeight = j.value("pillarHeight", 2.5f);
        m_data.pillarRadius = j.value("pillarRadius", 0.25f);
        m_data.pillarSpeed   = j.value("pillarSpeed", 4.0f);
        m_data.freeOrbitSpeed = j.value("freeOrbitSpeed", 5.0f);
        m_dirty = false;
        return true;
    } catch (...) {
        // Corrupt config → overwrite with defaults.
        save();
        return true;
    }
}

bool Config::save() const {
    nlohmann::json j;
    j["targetModel"]    = (m_data.targetModel == TargetModel::Capsule) ? 1 : 0;
    j["targetRadius"]   = m_data.targetRadius;
    j["targetMinSpeed"] = m_data.targetMinSpeed;
    j["targetMaxSpeed"] = m_data.targetMaxSpeed;
    j["aimRedirectMin"] = m_data.aimRedirectMin;
    j["aimRedirectMax"] = m_data.aimRedirectMax;
    j["mouseSensX"]     = m_data.mouseSensX;
    j["mouseSensY"]     = m_data.mouseSensY;
    j["gameMode"]        = m_data.gameMode;
    j["strafeSpeedMode"] = m_data.strafeSpeedMode;
    j["roundDuration"]    = m_data.roundDuration;
    j["countdownSec"]      = m_data.countdownSec;
    j["bounceRestitution"] = m_data.bounceRestitution;
    j["bounceGravity"]      = m_data.bounceGravity;
    j["bounceHeight"]       = m_data.bounceHeight;
    j["jumpEnabled"]        = m_data.jumpEnabled;
    j["targetBoundX"]       = m_data.targetBoundX;
    j["targetBoundZ"]       = m_data.targetBoundZ;
    j["targetBaseZ"]        = m_data.targetBaseZ;
    j["gridSpacing"]  = m_data.gridSpacing;
    j["gridRadius"]   = m_data.gridRadius;
    j["gridCenterY"]  = m_data.gridCenterY;
    j["gridCenterZ"]  = m_data.gridCenterZ;
    j["coverCount"]  = m_data.coverCount;
    j["coverHalfX"]  = m_data.coverHalfX;
    j["coverHalfY"]  = m_data.coverHalfY;
    j["coverHalfZ"]  = m_data.coverHalfZ;
    j["pillarHeight"] = m_data.pillarHeight;
    j["pillarRadius"] = m_data.pillarRadius;
    j["pillarSpeed"]   = m_data.pillarSpeed;
    j["freeOrbitSpeed"] = m_data.freeOrbitSpeed;
    std::ofstream f(m_filePath);
    if (!f.is_open()) return false;
    f << j.dump(4);
    return true;
}

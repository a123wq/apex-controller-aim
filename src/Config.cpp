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
    std::ofstream f(m_filePath);
    if (!f.is_open()) return false;
    f << j.dump(4);
    return true;
}

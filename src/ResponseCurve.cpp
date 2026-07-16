#include "App.hpp"
#include <cmath>

ResponseCurve::ResponseCurve() {
    m_smoothedX = m_smoothedY = 0.0f;
    m_storedSpeedX = m_storedSpeedY = 0.0f;
}

float ResponseCurve::applyDeadzone(float value, float deadzone, float outerDeadzone) {
    float absV = std::abs(value);
    if (absV <= deadzone) return 0.0f;
    if (absV >= (1.0f - outerDeadzone)) return std::copysignf(1.0f, value);
    float remapped = (absV - deadzone) / (1.0f - deadzone - outerDeadzone);
    return std::copysignf(remapped, value);
}

float ResponseCurve::applyCurve(float normalized, float exponent) {
    return std::pow(std::abs(normalized), exponent) * std::copysignf(1.0f, normalized);
}

float ResponseCurve::applyMicroAim(float value, float threshold, float strength) {
    float absV = std::abs(value);
    if (absV >= threshold) return value;
    float t = absV / threshold;
    float factor = 1.0f + strength * (1.0f - t);
    return value * factor;
}

float ResponseCurve::applySmoothing(float raw, float smoothing, float& smoothed) {
    float alpha = 1.0f - smoothing;
    smoothed = smoothed + alpha * (raw - smoothed);
    return smoothed;
}

float ResponseCurve::applyMaxSpeed(float value, float maxSpeed) {
    return value * maxSpeed;
}

float ResponseCurve::applyAcceleration(float target, float accel, float decel,
                                       float dt, float& stored) {
    float diff = target - stored;
    if (std::abs(diff) < 0.001f) { stored = target; return target; }
    float maxChange = (diff > 0.0f) ? accel : decel;
    maxChange *= static_cast<float>(dt);
    if (std::abs(diff) <= maxChange) { stored = target; }
    else { stored += std::copysignf(maxChange, diff); }
    return stored;
}

ResponseCurve::AngularVelocity ResponseCurve::process(
    float rawX, float rawY,
    const AimProfile& p,
    float adsMultiplier,
    double dt) {

    float dtf = static_cast<float>(dt);

    float x = applyDeadzone(rawX, p.deadzone, p.outerDeadzone);
    float y = applyDeadzone(rawY, p.deadzone, p.outerDeadzone);

    float mag = std::sqrt(x * x + y * y);
    if (mag > 0.0f) {
        float curvedMag = std::pow(mag, p.curveExponent);
        float scale = curvedMag / mag;
        x *= scale; y *= scale;
    }

    x = applyMicroAim(x, p.microAimThreshold, p.microAimStrength);
    y = applyMicroAim(y, p.microAimThreshold, p.microAimStrength);

    x = applySmoothing(x, p.inputSmoothing, m_smoothedX);
    y = applySmoothing(y, p.inputSmoothing, m_smoothedY);

    float yawSpeed   = applyMaxSpeed(x, p.maxYawSpeed);
    float pitchSpeed = applyMaxSpeed(y, p.maxPitchSpeed);

    yawSpeed   = applyAcceleration(yawSpeed,   p.acceleration, p.deceleration, dtf, m_storedSpeedX);
    pitchSpeed = applyAcceleration(pitchSpeed, p.acceleration, p.deceleration, dtf, m_storedSpeedY);

    yawSpeed   *= adsMultiplier;
    pitchSpeed *= adsMultiplier;

    return { yawSpeed, pitchSpeed };
}

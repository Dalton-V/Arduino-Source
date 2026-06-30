/*  Nintendo Switch Gyro Motion
 *
 *  From: https://github.com/PokemonAutomation/
 */

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Common/Cpp/CancellableScope.h"
#include "NintendoSwitch_Gyro.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

namespace{

bool is_finite(const GyroVector& vector){
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

GyroVector transform_vector(ControllerClass controller_class, const GyroVector& vector){
    switch (controller_class){
    case ControllerClass::NintendoSwitch_LeftJoycon:
    case ControllerClass::NintendoSwitch_ProController:
        return vector;
    case ControllerClass::NintendoSwitch_RightJoycon:
        // This is a 180-degree rotation about X, so the basis remains right-handed.
        return {vector.x, -vector.y, -vector.z};
    default:
        throw std::invalid_argument("Gyro motion is unsupported for this controller class.");
    }
}

}


bool is_finite(const GyroState& state){
    return is_finite(state.acceleration_g) && is_finite(state.angular_velocity_dps);
}
GyroState transform_gyro_state(ControllerClass controller_class, const GyroState& state){
    return {
        transform_vector(controller_class, state.acceleration_g),
        transform_vector(controller_class, state.angular_velocity_dps),
    };
}
GyroVector interpolate(const GyroVector& from, const GyroVector& to, double progress){
    return {
        from.x + (to.x - from.x) * progress,
        from.y + (to.y - from.y) * progress,
        from.z + (to.z - from.z) * progress,
    };
}
GyroState interpolate(const GyroState& from, const GyroState& to, double progress){
    return {
        interpolate(from.acceleration_g, to.acceleration_g, progress),
        interpolate(from.angular_velocity_dps, to.angular_velocity_dps, progress),
    };
}


namespace Internal{

std::vector<GyroMotionSample> sample_gyro_motion(
    Cancellable* cancellable,
    ControllerClass controller_class,
    Milliseconds duration,
    const GyroFunction& function
){
    if (duration <= Milliseconds::zero()){
        return {};
    }
    if (!function){
        throw std::invalid_argument("Gyro motion function must not be empty.");
    }

    // Validate the geometry even if the callback would otherwise return no
    // interesting values. Unsupported classes must never silently use identity.
    transform_gyro_state(controller_class, {});

    std::vector<GyroMotionSample> samples;
    samples.reserve((duration.count() + GYRO_REPORT_INTERVAL.count() - 1) / GYRO_REPORT_INTERVAL.count());
    Milliseconds elapsed = Milliseconds::zero();
    while (elapsed < duration){
        if (cancellable){
            cancellable->throw_if_cancelled();
        }

        const Milliseconds chunk = std::min(GYRO_REPORT_INTERVAL, duration - elapsed);
        elapsed += chunk;
        const GyroState canonical = function(elapsed);
        if (!is_finite(canonical)){
            throw std::invalid_argument("Gyro motion values must be finite.");
        }
        samples.emplace_back(GyroMotionSample{
            chunk,
            transform_gyro_state(controller_class, canonical),
        });
    }
    return samples;
}


}
}
}

/*  Nintendo Switch Gyro Motion
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Program-facing, physical-unit gyro motion types.
 */

#ifndef PokemonAutomation_NintendoSwitch_Gyro_H
#define PokemonAutomation_NintendoSwitch_Gyro_H

#include <functional>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Controllers/ControllerTypes.h"

namespace PokemonAutomation{

class Cancellable;

namespace NintendoSwitch{


struct GyroVector{
    double x = 0;
    double y = 0;
    double z = 0;
};

// Canonical controller frame:
//   +X points toward the trigger/shoulder-button edge.
//   +Y points toward the controller's left when looking at its controls.
//   +Z points out through the controls.
// Positive angular velocity follows the right-hand rule. Acceleration is
// sensor acceleration in g; gravity is not synthesized by this API. This is
// a fixed logical input convention, not a world-space reference frame.
struct GyroState{
    GyroVector acceleration_g;
    GyroVector angular_velocity_dps;
};

using GyroFunction = std::function<GyroState(Milliseconds elapsed)>;

// This is an implementation detail of controller backends, not a cadence a
// program needs to know when describing motion.
constexpr Milliseconds GYRO_REPORT_INTERVAL{15};

bool is_finite(const GyroState& state);
GyroState transform_gyro_state(ControllerClass controller_class, const GyroState& state);
GyroVector interpolate(const GyroVector& from, const GyroVector& to, double progress);
GyroState interpolate(const GyroState& from, const GyroState& to, double progress);


namespace Internal{

// Internal representation used while a trajectory is being validated before
// it reaches the scheduler. It intentionally contains no raw sensor counts.
struct GyroMotionSample{
    Milliseconds duration;
    GyroState state;
};

std::vector<GyroMotionSample> sample_gyro_motion(
    Cancellable* cancellable,
    ControllerClass controller_class,
    Milliseconds duration,
    const GyroFunction& function
);

}


}
}
#endif

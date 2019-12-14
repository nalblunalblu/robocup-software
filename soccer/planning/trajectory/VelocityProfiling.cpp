#include "VelocityProfiling.hpp"
#include <motion/TrapezoidalMotion.hpp>
#include <Utils.hpp>

namespace Planning {

using Geometry2d::Point;
using Geometry2d::Pose;
using Geometry2d::Twist;

Trajectory ProfileVelocity(const BezierPath& path,
                           double initial_speed,
                           double final_speed,
                           const MotionConstraints& constraints,
                           RJ::Time initial_time) {
    Trajectory result({});
    RobotInstant instant;

    // Add an initial point to the trajectory so that we keep track of initial
    // velocity
    path.Evaluate(0, &instant.pose.position(), &instant.velocity.linear());
    instant.stamp = initial_time;

    // Scale the velocity so that the initial speed is correct
    instant.velocity.linear() = instant.velocity.linear().normalized() * initial_speed;

    result.AppendInstant(instant);

    AppendProfiledVelocity(result, path, final_speed, constraints);
    return result;
}

void AppendProfiledVelocity(Trajectory& out,
                            const BezierPath& path,
                            double final_speed,
                            const MotionConstraints& constraints) {
    double initial_speed = 0;

    // The rest of this code will assume that we don't need to add the first
    // instant in the trajectory, because it will be at the end of the existing
    // trajectory. Add an instant to ensure this is true when out is empty.
    if (out.empty()) {
        RobotInstant instant;
        instant.stamp = RJ::now();
        path.Evaluate(0, &instant.pose.position());
        out.AppendInstant(instant);
    }

    // Planning starts after `out` ends, so we want to capture the ending speed.
    initial_speed = out.last().velocity.linear().mag();

    constexpr int num_segments = 15;

    // Scratch data that we will use later.
    std::vector<Point> points(num_segments + 1), derivs1(num_segments + 1);
    std::vector<double> curvature(num_segments + 1), speed(num_segments + 1, constraints.maxSpeed);

    //we must make this assumption for the next calculations, otherwise we get NANs
    assert(constraints.maxAcceleration >= constraints.maxCentripetalAcceleration);

    //note: these are just suggestions. if they are impossible given MotionConstraints, then we'll limit them
    speed[0] = initial_speed;
    speed[num_segments] = final_speed;

    // Velocity pass: fill points and calculate maximum velocity given curvature
    // at each point.
    for (int n = 0; n < num_segments + 1; n++) {
        double s = n / static_cast<double>(num_segments);
        path.Evaluate(s, &points[n], &derivs1[n], &curvature[n]);

        assert(curvature[n] >= 0.0);
        assert(!std::isnan(curvature[n]) && !std::isinf(curvature[n]));

        // Centripetal acceleration: a = v^2 / r => v = sqrt(ra)
        if (curvature[n] != 0.0) {
            //todo(Ethan) verify with kyle that its fine to switch this to `constraints.maxCentripetalAcceleration`
            speed[n] = std::min(speed[n], std::sqrt(constraints.maxCentripetalAcceleration / curvature[n]));
        }
    }

    // Acceleration pass: calculate maximum velocity at each point based on
    // acceleration limits forwards in time.
    for (int n = 0; n < num_segments; n++) {
        double centripetal = speed[n] * speed[n] * curvature[n];

        using std::pow;
        double accel = std::sqrt(pow(constraints.maxAcceleration, 2) - pow(centripetal, 2)+0.0000001);
        assert(!std::isnan(accel) && !std::isinf(accel));
        double distance = (points[n + 1] - points[n]).mag();

        // Do a trapezoid profile on maximum acceleration.
        // vf^2 = vi^2 + 2ad
        double max_speed = std::sqrt(pow(speed[n], 2) + 2 * accel * distance);
        if (max_speed < speed[n + 1]) {
            speed[n + 1] = max_speed;
        }
    }

    // Decceleration pass: calculate maximum velocity at each point based on
    // acceleration limits backwards in time.
    for (int n = num_segments; n > 0; n--) {
        double centripetal = speed[n] * speed[n] * curvature[n];

        using std::pow;
        double accel = std::sqrt(pow(constraints.maxAcceleration, 2) - pow(centripetal, 2) + 0.0000001);
        assert(!std::isnan(accel) && !std::isinf(accel));
        double distance = (points[n - 1] - points[n]).mag();

        // Do a trapezoid profile on maximum acceleration.
        // vf^2 = vi^2 + 2ad
        double max_speed = std::sqrt(pow(speed[n], 2) + 2 * accel * distance);
        if (max_speed < speed[n - 1]) {
            speed[n - 1] = max_speed;
        }
    }

    // TODO(Kyle): Allow the user to pass in an initial time. todo(Ethan) done?
    RJ::Time time = out.last().stamp;

    // We skip the first instant.
    for (int n = 1; n < num_segments + 1; n++) {
        double distance = (points[n] - points[n - 1]).mag();
        double vbar = (speed[n] + speed[n - 1]) / 2;
        double t_sec = distance / vbar;
        time = time + RJ::Seconds(t_sec);

        // Add point n in
        //todo(Ethan) verify this default angle w Kyle.
        out.AppendInstant(RobotInstant{Pose(points[n], derivs1[n].angle()+M_PI), Twist(derivs1[n].normalized() * speed[n], n), time});
    }
}
//todo(Ethan) verify this
void PlanAngles(Trajectory& trajectory,
                const RobotInstant& initial_state,
                const AngleFunction& angle_function,
                const RotationConstraints& constraints) {
    if(trajectory.empty()) {
        return;
    }
    //todo(Ethan) explain?
//    trajectory.instant(0).pose = initial_state.pose;
//    trajectory.instant(0).velocity = initial_state.velocity;

    // Move forwards in time. At each instant, calculate the goal angle and its
    // time derivative, and try to get there with a trapezoidal profile.
    for (int i = 0; i < trajectory.num_instants(); i++) {
//    for (int i = 0; i < trajectory.num_instants() - 1; i++) {
        RobotInstant& instant_initial = trajectory.instant(i);
        instant_initial.pose.heading() =
                angle_function(instant_initial.pose.position(),
                        instant_initial.velocity.linear(),
                        instant_initial.pose.heading());

//        RobotInstant& instant_final = trajectory.instant(i + 1);
//
//        double target_angle = angle_function(instant_final.pose.position(),
//                                             instant_final.velocity.linear(),
//                                             instant_initial.pose.heading());
//
//        double target_angle_delta = fixAngleRadians(target_angle - instant_initial.pose.heading());
//        double angle_delta;
//
//        // TODO(Kyle): Calculate a proper final speed instead of specifying zero.
//        TrapezoidalMotion(
//                target_angle_delta,
//                constraints.maxSpeed,
//                constraints.maxAccel,
//                std::chrono::duration_cast<RJ::Seconds>(instant_final.stamp - instant_initial.stamp).count(),
//                instant_initial.velocity.angular(),
//                0, angle_delta, instant_final.velocity.angular());
//        instant_final.pose.heading() = instant_initial.pose.heading() + angle_delta;
    }
}

} // namespace Planning
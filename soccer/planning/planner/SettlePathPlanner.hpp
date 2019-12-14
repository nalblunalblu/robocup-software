#pragma once

#include "planning/planner/DirectTargetPathPlanner.hpp"
#include "planning/planner/PathTargetPlanner.hpp"
#include "planning/planner/Planner.hpp"

namespace Planning {

/**
 * @brief Planner which tries to move around the ball to intercept it
 *
 * TODO: Clean up description
 */
class SettlePathPlanner : public PlannerForCommandType<SettleCommand> {
public:
    enum SettlePathPlannerStates {
        // Moves to the ball path in front of it
        Intercept,
        // Starts to dampen the ball with backward motion
        Dampen
    };

    SettlePathPlanner()
        : avgInstantaneousInterceptTarget(0, 0),
          averageBallVel(0, 0),
          firstInterceptTargetFound(false),
          firstBallVelFound(false),
          pathCreatedForDampen(false),
          currentState(Intercept){};

    virtual Trajectory plan(PlanRequest&& request) override;

    static void createConfiguration(Configuration* cfg);

    std::string name() const override {
        return "SettlePathPlanner";
    }

private:
    // Given the global target in `targetBounceDirection`
    // Calculate the delta position to get the robot in the correct location
    // And the face point to get the bounce right
    // If no targetBounceDirection is given, just get in front and face the ball
    void calcDeltaPosForDir(const Ball& ball, const RobotInstant& currentInstant,
                            double& angle, Geometry2d::Point& deltaRobotPos,
                            Geometry2d::Point& facePos);

    // Restarts the state machine if our calculations are whack
    // and won't intercept ball correctly anymore
    void checkSolutionValidity(const Ball& ball,
                               const RobotInstant& startInstant,
                               const Geometry2d::Point& deltaPos);

    // Figures out when to move to each state
    // (only in the standard transition)
    //
    // Note: startInstant may be changed inside this function
    //       when we are basically at the correct location and
    //       need to start the dampen
    void processStateTransition(const Ball& ball, const Trajectory& prevTrajectory,
                                RobotInstant& startInstant, const double angle,
                                const Geometry2d::Point& deltaPos);

    // State functions
    Trajectory intercept(PlanRequest&& planRequest, RobotInstant& startInstant,
                                    const Geometry2d::ShapeSet& obstacles,
                                    const Geometry2d::Point& deltaPos,
                                    const Geometry2d::Point& facePos);

    Trajectory dampen(PlanRequest&& planRequest, RobotInstant& startInstant,
                                    const Geometry2d::Point& deltaPos,
                                    const Geometry2d::Point& facePos);

    Trajectory invalid(const PlanRequest& planRequest);

    template <typename T>
    static T applyLowPassFilter(const T& oldValue, const T& newValue,
                                double gain);

    PathTargetPlanner rrtPlanner;
    DirectTargetPathPlanner directPlanner;
    std::optional<Geometry2d::Point> targetBounceDirection;

    SettlePathPlannerStates currentState;

    // Intercept Target Filtering Variables
    Geometry2d::Point avgInstantaneousInterceptTarget;
    Geometry2d::Point averageBallVel;
    bool firstInterceptTargetFound;
    bool firstBallVelFound;

    // Only change the target of the path if it changes significantly
    Geometry2d::Point pathInterceptTarget;

    bool pathCreatedForDampen;

    // How much of the ball seed to contact the ball with
    // before slowing down to dampen the initial hit
    static ConfigDouble* _ballSpeedPercentForDampen;  // %

    // Closest dist to start searching for intercept points
    static ConfigDouble* _searchStartDist;  // m
    // Furthest dist to search for intercept points
    static ConfigDouble* _searchEndDist;  // m
    // What dist increment to search for intercepts
    static ConfigDouble* _searchIncDist;  // m

    // How much sooner should we reach the intercept point than we need to
    // This is a percent of the calculated intercept time
    // Numbers greater than 1 mean we increase intercept time needed by X% over
    // actual Numbers less than 1 mean we get there X% faster than we plan
    // (Shouldn't ever happen)
    static ConfigDouble* _interceptBufferTime;  // %

    // Gain on the averaging function to smooth the target point to intercept
    // This is due to the high flucations in the ball velocity frame to frame
    // a*newPoint + (1-a)*oldPoint
    // The lower the number, the less noise affects the system, but the slower
    // it responds to changes The higher the number, the more noise affects the
    // system, but the faster it responds to changes
    static ConfigDouble* _targetPointGain;

    // Gain on the averaging function to smooth the ball velocity to for any
    // motion commands This is due to the high flucations in the ball velocity
    // frame to frame a*newPoint + (1-a)*oldPoint The lower the number, the less
    // noise affects the system, but the slower it responds to changes The
    // higher the number, the more noise affects the system, but the faster it
    // responds to changes
    static ConfigDouble* _ballVelGain;

    // Distance between robot and closest point on ball line such that we move
    // directly into the ball line instead of trying to find the point we hit
    // first This does take into account slow moving balls in which we should
    // move onto the ball to capture it
    static ConfigDouble* _shortcutDist;  // m

    // If the ball velocity angle changes by a large amount
    // we want to quickly react and clear all the smoothing filters
    // Lower numbers means it reacts faster, but more chance for false positives
    // Higher numbers means slower reaction, but less false positives
    static ConfigDouble* _maxBallAngleForReset;  // Deg

    // If the ball velocity itself changes by a large amount
    // we want to quickly react and clear all the smoothing filters
    // Lower numbers means it reacts faster, but more chance for false positives
    // Higher numbers means slower reaction, but less false positives
    static ConfigDouble* _maxBallVelForPathReset;  // m/s

    // Max angle between ball and target bounce direction
    static ConfigDouble* _maxBounceAngle;  // Deg
};
}  // namespace Planning
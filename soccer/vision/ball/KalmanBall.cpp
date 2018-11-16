#include "KalmanBall.hpp"

#include <algorithm>

#include "vision/util/VisionFilterConfig.hpp"

void KalmanBall::createConfiguration(Configuration* cfg) {
    max_time_outside_vision = new ConfigDouble(cfg, "VisionFilter/KalmanBall/max_time_outside_vision", 0.2);
}

KalmanBall::KalmanBall(unsigned int cameraID, RJ::Time creationTime,
                       CameraBall initMeasurement)
    : cameraID(cameraID), health(*VisionFilterConfig::filter_health_init),
      lastUpdateTime(creationTime), lastPredictTime(creationTime) {

    Geometry2d::Point initPos = initMeasurement.getPos();
    Geometry2d::Point initVel = Geometry2d::Point(0,0);

    filter = KalmanFilter2D(initPos, initVel);

    previousMeasurements.push_back(initMeasurement);
}

KalmanBall::KalmanBall(unsigned int cameraID, RJ::Time creationTime,
                       CameraBall initMeasurement, WorldBall& previousWorldBall)
    : cameraID(cameraID), health(*VisionFilterConfig::filter_health_init),
      lastUpdateTime(creationTime), lastPredictTime(creationTime);

    Geometry2d::Point initPos = initMeasurement.getPos();
    Geometry2d::Point initVel = previousWorldBall.getVel();

    filter = KalmanFilter2D(initPos, initVel);

    previousMeasurements.push_back(initMeasurement);
}

void KalmanBall::predict(RJ::Time currentTime) {
    lastPredictTime = currentTime;

    // Decrement but make sure you don't go too low
    health = std::max(health - *VisionFilterConfig::filter_health_dec,
                      *VisionFilterConfig::filter_health_min);

    filter.predict();
}

void KalmanBall::predictAndUpdate(RJ::Time currentTime, CameraBall updateBall) {
    lastPredictTime = currentTime;
    lastUpdateTime = currentTime;

    // Increment but make sure you don't go too high
    health = std::min(health + *VisionFilterConfig::filter_health_inc,
                      *VisionFilterConfig::filter_health_max);

    // Keep last X camera observations in list for kick detection and filtering
    previousMeasurements.push_back(initMeasurement);
    if (previousMeasurements.size() > *VisionFilterConfig::slow_kick_detector_history_length) {
        previousMeasurements.pop_front();
    }

    filter.PredictWithUpdate(updateBall.getPos());
}

bool KalmanBall::isUnhealthy() {
    updated_recently = RJ::numSeconds(lastPredictTime - lastUpdatedTime) < *max_time_outside_vision;

    return !updated_recently;
}

unsigned int KalmanBall::getCameraID() {
    return cameraID;
}

int KalmanBall::getHealth() {
    return health;
}

Geometry2d::Point KalmanBall::getPos() {
    return filter.getPos();
}

Geometry2d::Point KalmanBall::getVel() {
    return filter.getVel();
}

Geometry2d::Point KalmanBall::getPosCov() {
    return filter.getPosCov();
}

Geometry2d::Point KalmanBall::getVelCov() {
    return filter.getVelCov();
}

std::deque<CameraBall> KalmanBall::getPrevMeasurements() {
    return previousMeasurements;
}

void KalmanBall::setVel(Geometry2d::Point newVel) {
    filter.setVel(newVel);
}
#pragma once
#include "timer.h"
#include <glm/glm.hpp>

class Animation{
    Timer timer;
    int frameCount;
public:
    Animation() : timer(0) , frameCount(0) {}
    Animation(int frameCount, float frameLength) : timer(frameLength), frameCount(frameCount) {}
    int curFrame() const {
        return static_cast<int>(timer.getTime() / timer.getLength() * frameCount);
    }
    float getLength() const { return timer.getLength(); }
    void step(float tmDelta){
        timer.step(tmDelta);
    }
    bool done() const { return timer.isTmOut(); }
};
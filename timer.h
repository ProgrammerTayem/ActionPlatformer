#pragma once

class Timer {
    float len, tm;
    bool tmout;
    public:
    Timer(float length) : len(length), tm(0.0f), tmout(false) {}
    bool step(float tmDelta){
        tm += tmDelta;
        if(tm > len){
            tm -= len;
            tmout = true;
            return true;
        }
        return false;
    }
    bool isTmOut() const { return tmout; }
    float getTime() const { return tm; }
    void reset() { tm = 0.0f; tmout = false; }
    float getLength() const { return len; }

};
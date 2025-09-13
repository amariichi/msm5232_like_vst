#pragma once

namespace msm5232 {

struct ADSRParams {
    float attack = 0.01f;
    float decay = 0.1f;
    float sustain = 0.7f; // 0..1
    float release = 0.2f;
};

class ADSR {
public:
    void setSampleRate(float sr) { sr_ = sr > 1.0f ? sr : 48000.0f; }
    void set(const ADSRParams& p) { p_ = p; }
    void gate(bool on);
    float process();
    bool isActive() const { return state_ != Idle; }
private:
    enum State { Idle, Attack, Decay, Sustain, Release };
    State state_ = Idle;
    ADSRParams p_{};
    float sr_ = 48000.0f;
    float env_ = 0.0f;
};

}


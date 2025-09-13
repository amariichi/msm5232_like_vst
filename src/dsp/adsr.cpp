#include "dsp/adsr.h"
#include <algorithm>

namespace msm5232 {

void ADSR::gate(bool on) {
    if (on) {
        state_ = Attack;
    } else {
        if (state_ != Idle) state_ = Release;
    }
}

float ADSR::process() {
    switch (state_) {
        case Idle:
            env_ = 0.0f; break;
        case Attack: {
            float step = (p_.attack <= 1e-5f) ? 1.0f : 1.0f / (p_.attack * sr_);
            env_ += step;
            if (env_ >= 1.0f) { env_ = 1.0f; state_ = Decay; }
        } break;
        case Decay: {
            float target = std::clamp(p_.sustain, 0.0f, 1.0f);
            float step = (p_.decay <= 1e-5f) ? 1.0f : 1.0f / (p_.decay * sr_);
            env_ -= step * (1.0f - target);
            if (env_ <= target) { env_ = target; state_ = Sustain; }
        } break;
        case Sustain:
            // hold
            break;
        case Release: {
            float step = (p_.release <= 1e-5f) ? 1.0f : 1.0f / (p_.release * sr_);
            env_ -= step * std::max(env_, 0.0f);
            if (env_ <= 1e-5f) { env_ = 0.0f; state_ = Idle; }
        } break;
    }
    return env_;
}

}


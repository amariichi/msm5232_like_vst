#ifdef HAVE_VST3_SDK
#include "dsp/synth.h"
#include <public.sdk/source/vst/vstaudioeffect.h>
#include <public.sdk/source/vst/vstparameters.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <cmath>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
// Map normalized 0..1 -> noise ratio d (0..100), with fine control in 0..10%
// x in [0,0.5] : linear 0..0.1 (0..10%)
// x in (0.5,1] : logarithmic 0.1..100 (10%..10000%)
static inline float noiseNormToRatio(float x) {
    if (x < 0.f) x = 0.f; if (x > 1.f) x = 1.f;
    if (x <= 0.5f) return 0.2f * x; // 0..0.1
    float y = (x - 0.5f) / 0.5f;    // 0..1
    return 0.1f * std::pow(1000.0f, y);
}
// Inverse: ratio (0..100) -> normalized 0..1
static inline float noiseRatioToNorm(float r) {
    if (r <= 0.f) return 0.f;
    if (r <= 0.1f) return r / 0.2f;
    float y = std::log(r / 0.1f) / std::log(1000.0f);
    float x = 0.5f + 0.5f * y;
    if (x < 0.f) x = 0.f; if (x > 1.f) x = 1.f;
    return x;
}
// Parameter IDs
enum ParamIDs : ParamID {
    kParamTone = 1000,
    kParamAttack,
    kParamDecay,
    kParamSustain,
    kParamRelease,
    kParamGain,
    kParamPolyphony,
    kParamTableSize,
    kParamQuantize4,
    kParamDetune, // new: fine detune +/- 0.5 semitone
    kParamVibratoDepth, // new: vibrato depth 0..0.5 st
    kParamVibratoRate,  // new: vibrato rate 0..16 Hz
    kParamNoiseAdd,     // new: additive noise depth 0..100%
    kParamBLQuality,    // new: bandlimit quality 0=Off, 1..8 bands per octave
    kParamHQMode,       // new: HQ mode 0=Off,1=Auto2x,2=Force2x,3=Force4x,4=Force8x
    kParamPreHighCutMode,   // 0=Off,1=Fixed,2=ByMaxNote
    kParamPreHighCutMaxNote,// 0..127 (default 108)
};
}

class Msm5232Processor : public AudioEffect {
public:
    Msm5232Processor() { setControllerClass(FUID(0x0B5C2B11,0xF2C84185,0x9F6E3CB5,0x88947733)); }
    static FUnknown* create(void*);

    tresult PLUGIN_API initialize(FUnknown* ctx) SMTG_OVERRIDE {
        tresult r = AudioEffect::initialize(ctx);
        if (r != kResultOk) return r;
        addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
        addEventInput(STR16("MIDI In"), 16);
        synth_.setup(48000.0f);
        return kResultOk;
    }
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* in, int32 numIn, SpeakerArrangement* out, int32 numOut) SMTG_OVERRIDE {
        if (numIn != 0) return kResultFalse;
        if (numOut != 1) return kResultFalse;
        if (out[0] != SpeakerArr::kStereo) return kResultFalse;
        return kResultOk;
    }
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE {
        sampleRate_ = (float)setup.sampleRate;
        synth_.setup(sampleRate_);
        return kResultOk;
    }
    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE {
        // Handle incoming parameter changes (including MIDI CC / PitchBend per VST3 convention)
        if (data.inputParameterChanges) {
            int32 count = data.inputParameterChanges->getParameterCount();
            bool paramsAffectCore = false;      // Tone/Table/Quantize/BLQuality/ADSR/Gain/Polyphony
            for (int32 i = 0; i < count; ++i) {
                auto* queue = data.inputParameterChanges->getParameterData(i);
                if (!queue) continue;
                int32 pcount = queue->getPointCount();
                int32 offset; ParamValue val;
                if (pcount > 0 && queue->getPoint(pcount - 1, offset, val) == kResultOk) {
                    switch (queue->getParameterId()) {
                        case kParamTone: params_.toneMask = 1 + (int)(val * 14.999); paramsAffectCore = true; break;
                        case kParamAttack: params_.adsr.attack = (float)val * 2.0f; paramsAffectCore = true; break;
                        case kParamDecay: params_.adsr.decay = (float)val * 2.0f; paramsAffectCore = true; break;
                        case kParamSustain: params_.adsr.sustain = (float)val; paramsAffectCore = true; break;
                        case kParamRelease: params_.adsr.release = (float)val * 2.0f; paramsAffectCore = true; break;
                        case kParamGain: params_.gain = (float)val; paramsAffectCore = true; break;
                        case kParamPolyphony: params_.polyphony = 1 + (int)std::floor(val * 31.0); paramsAffectCore = true; break;
                        case kParamTableSize: {
                            // 64 / 128 / 256 の3段
                            if (val < (1.0/3.0)) params_.tableLen = 64;
                            else if (val < (2.0/3.0)) params_.tableLen = 128;
                            else params_.tableLen = 256;
                            paramsAffectCore = true;
                        } break;
                        case kParamQuantize4: params_.quantize4 = (val >= 0.5); paramsAffectCore = true; break;
                        case kParamDetune: {
                            // Map 0..1 -> -0.5..+0.5 semitones
                            float semis = (float(val) - 0.5f) * 1.0f;
                            synth_.setDetuneSemis(semis);
                        } break;
                        case kParamVibratoDepth: {
                            // 0..1 -> 0..0.5 st
                            float depthSemis = (float)val * 0.5f;
                            synth_.setVibratoDepthSemis(depthSemis);
                            // If precut depends on vibrato depth, rebuild
                            if (params_.preHighCutMode == 2) paramsAffectCore = true;
                        } break;
                        case kParamVibratoRate: {
                            // 0..1 -> 0..16 Hz
                            float hz = (float)val * 16.0f;
                            synth_.setVibratoRateHz(hz);
                        } break;
                        case kParamNoiseAdd: {
                            // Normalized 0..1 -> ratio 0..100 (1:100)
                            float ratio = noiseNormToRatio((float)val);
                            synth_.setNoiseAdd(ratio);
                        } break;
                        case kParamBLQuality: {
                            // 0..1 -> 0..8
                            int q = (int)std::floor(val * 9.0);
                            if (q < 0) q = 0; if (q > 8) q = 8;
                            params_.blQuality = q;
                            paramsAffectCore = true;
                        } break;
                        case kParamHQMode: {
                            // 0..1 -> 0..4
                            int m = (int)std::floor(val * 5.0);
                            if (m < 0) m = 0; if (m > 4) m = 4;
                            params_.hqMode = m;
                            paramsAffectCore = true; // affects runtime decision but safe
                        } break;
                        case kPitchBend: {
                            // VST3 normalized pitch bend (0..1, 0.5 center). Use +/-2 semitone range.
                            float semis = (float(val) - 0.5f) * 4.0f;
                            synth_.setPitchBendSemis(semis);
                        } break;
                        case kCtrlModWheel: {
                            // CC#1 0..1 -> -0.5..+0.5 st detune, also reflect to Detune parameter for GUI
                            float semis = (float(val) - 0.5f) * 1.0f;
                            synth_.setDetuneSemis(semis);
                            if (data.outputParameterChanges) {
                                int32 indexOut = 0; IParamValueQueue* outQ = data.outputParameterChanges->addParameterData(kParamDetune, indexOut);
                                if (outQ) {
                                    int32 dummy = 0;
                                    outQ->addPoint(offset, val, dummy);
                                }
                            }
                        } break;
                        case kParamPreHighCutMode: {
                            // 0..1 -> 0..2
                            int m = (int)std::floor(val * 3.0);
                            if (m < 0) m = 0; if (m > 2) m = 2;
                            params_.preHighCutMode = m;
                            paramsAffectCore = true;
                        } break;
                        case kParamPreHighCutMaxNote: {
                            int note = (int)std::floor(val * 127.0 + 0.5);
                            if (note < 0) note = 0; if (note > 127) note = 127;
                            params_.preHighCutMaxNote = note;
                            paramsAffectCore = true;
                        } break;
                        case 24: { // CC#24 -> Vibrato Depth
                            float depthSemis = (float)val * 0.5f;
                            synth_.setVibratoDepthSemis(depthSemis);
                            // Reflect to GUI VibratoDepth parameter
                            if (data.outputParameterChanges) {
                                int32 indexOut = 0; IParamValueQueue* outQ = data.outputParameterChanges->addParameterData(kParamVibratoDepth, indexOut);
                                if (outQ) {
                                    int32 dummy = 0;
                                    outQ->addPoint(offset, val, dummy);
                                }
                            }
                        } break;
                        case 25: { // CC#25 -> Vibrato Rate
                            float hz = (float)val * 16.0f;
                            synth_.setVibratoRateHz(hz);
                            // Reflect to GUI VibratoRate parameter
                            if (data.outputParameterChanges) {
                                int32 indexOut = 0; IParamValueQueue* outQ = data.outputParameterChanges->addParameterData(kParamVibratoRate, indexOut);
                                if (outQ) {
                                    int32 dummy = 0;
                                    // Map 0..16 Hz -> normalized 0..1
                                    outQ->addPoint(offset, val, dummy);
                                }
                            }
                        } break;
                    }
                }
            }
            if (paramsAffectCore) {
                synth_.setParams(params_);
            }
        }

        if (data.inputEvents) {
            int32 num = data.inputEvents->getEventCount();
            for (int32 i = 0; i < num; ++i) {
                Event e; if (data.inputEvents->getEvent(i, e) == kResultOk) {
                    if (e.type == Event::kNoteOnEvent) {
                        synth_.noteOn(e.noteOn.pitch, int(e.noteOn.velocity * 127.0f));
                    } else if (e.type == Event::kNoteOffEvent) {
                        synth_.noteOff(e.noteOff.pitch);
                    }
                }
            }
        }

        if (data.numOutputs > 0) {
            auto& bus = data.outputs[0];
            float* L = bus.channelBuffers32[0];
            float* R = bus.channelBuffers32[1];
            synth_.process(L, R, data.numSamples);
        }
        return kResultOk;
    }

private:
    msm5232::Synth synth_{};
    msm5232::SynthParams params_{};
    float sampleRate_ = 48000.0f;
};

// Out-of-class definition to ensure linker symbol exists across translation units
Steinberg::FUnknown* Msm5232Processor::create(void*) {
    return (Steinberg::Vst::IAudioProcessor*)new Msm5232Processor();
}

#endif // HAVE_VST3_SDK

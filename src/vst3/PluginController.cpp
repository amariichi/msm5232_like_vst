#ifdef HAVE_VST3_SDK
#include <public.sdk/source/vst/vsteditcontroller.h>
#include <public.sdk/source/vst/vstparameters.h>
#include <pluginterfaces/base/ustring.h>
#include <cmath>
#include <string>
#include <string>
#include <cstdio>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
// Shared helper (duplicated to avoid cross-TU deps)
static inline float noiseNormToRatio(float x) {
    if (x < 0.f) x = 0.f; if (x > 1.f) x = 1.f;
    if (x <= 0.5f) return 0.2f * x; // 0..0.1
    float y = (x - 0.5f) / 0.5f;    // 0..1
    return 0.1f * std::pow(1000.0f, y);
}
static inline float noiseRatioToNorm(float r) {
    if (r <= 0.f) return 0.f;
    if (r <= 0.1f) return r / 0.2f;
    float y = std::log(r / 0.1f) / std::log(1000.0f);
    float x = 0.5f + 0.5f * y;
    if (x < 0.f) x = 0.f; if (x > 1.f) x = 1.f;
    return x;
}
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
    kParamDetune,
    kParamVibratoDepth,
    kParamVibratoRate,
    kParamNoiseAdd, // 0..100% additive noise depth (relative to |signal|)
    kParamBLQuality, // 0=Off, 1=1/oct .. 8=8/oct
    kParamHQMode,    // 0=Off, 1=Auto2x, 2=Force2x, 3=Force4x, 4=Force8x
    kParamPreHighCutMode, // 0=Off, 1=Fixed, 2=ByMaxNote
    kParamPreHighCutMaxNote, // 0..127
};
}

class Msm5232Controller : public EditController {
public:
    Msm5232Controller() = default;
    static FUnknown* create(void*);
    tresult PLUGIN_API getParamStringByValue (ParamID id, ParamValue valueNormalized, String128 string) SMTG_OVERRIDE {
        UString out(string, 128);
        if (id == kParamTone) {
            int tone = 1 + (int)std::floor(valueNormalized * 14.0 + 0.5);
            if (tone < 1) tone = 1; if (tone > 15) tone = 15;
            std::string s = std::to_string(tone);
            out.fromAscii(s.c_str());
            return kResultOk;
        }
        if (id == kParamPolyphony) {
            int poly = 1 + (int)std::floor(valueNormalized * 31.0 + 0.5);
            if (poly < 1) poly = 1; if (poly > 32) poly = 32;
            std::string s = std::to_string(poly);
            out.fromAscii(s.c_str());
            return kResultOk;
        }
        if (id == kParamTableSize) {
            int sz;
            if (valueNormalized < (1.0/3.0)) sz = 64;
            else if (valueNormalized < (2.0/3.0)) sz = 128;
            else sz = 256;
            std::string s = std::to_string(sz);
            out.fromAscii(s.c_str());
            return kResultOk;
        }
        if (id == kParamDetune) {
            // Show signed value without unit; unit is provided by parameter's unit label
            double semis = (valueNormalized - 0.5) * 1.0; // -0.5..+0.5
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%+.2f", semis);
            out.fromAscii(buf);
            return kResultOk;
        }
        if (id == kParamVibratoDepth) {
            // Depth is 0..0.5 st
            double semis = valueNormalized * 0.5;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", semis);
            out.fromAscii(buf);
            return kResultOk;
        }
        if (id == kParamVibratoRate) {
            // Rate is 0..16 Hz
            double hz = valueNormalized * 16.0;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.2f", hz);
            out.fromAscii(buf);
            return kResultOk;
        }
        if (id == kParamNoiseAdd) {
            // Display percent up to 10000% (1:100). Use mapping for fine low-end control.
            float ratio = noiseNormToRatio((float)valueNormalized); // 0..100
            int pct = (int)std::floor(ratio * 100.0f + 0.5f);
            if (pct < 0) pct = 0; if (pct > 10000) pct = 10000;
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%d", pct);
            out.fromAscii(buf);
            return kResultOk;
        }
        if (id == kParamBLQuality) {
            // Discrete steps: Off, 1/Oct .. 8/Oct
            int idx = (int)std::floor(valueNormalized * 9.0); // 0..8 within [0,1]
            if (idx < 0) idx = 0; if (idx > 8) idx = 8;
            static const char* names[9] = {
                "Off","1/Oct","2/Oct","3/Oct","4/Oct","5/Oct","6/Oct","7/Oct","8/Oct"
            };
            out.fromAscii(names[idx]);
            return kResultOk;
        }
        if (id == kParamHQMode) {
            int idx = (int)std::floor(valueNormalized * 5.0); // 0..4
            if (idx < 0) idx = 0; if (idx > 4) idx = 4;
            static const char* names[5] = { "Off", "Auto2x", "Force2x", "Force4x", "Force8x" };
            out.fromAscii(names[idx]);
            return kResultOk;
        }
        if (id == kParamPreHighCutMode) {
            int idx = (int)std::floor(valueNormalized * 3.0); // 0..2
            if (idx < 0) idx = 0; if (idx > 2) idx = 2;
            const char* names[3] = { "Off", "Fixed", "ByMaxNote" };
            out.fromAscii(names[idx]);
            return kResultOk;
        }
        if (id == kParamPreHighCutMaxNote) {
            int note = (int)std::floor(valueNormalized * 127.0 + 0.5);
            if (note < 0) note = 0; if (note > 127) note = 127;
            char buf[32]; std::snprintf(buf, sizeof(buf), "%d", note);
            out.fromAscii(buf);
            return kResultOk;
        }
        return EditController::getParamStringByValue(id, valueNormalized, string);
    }
    tresult PLUGIN_API getParamValueByString (ParamID id, TChar* string, ParamValue& valueNormalized) SMTG_OVERRIDE {
        UString in(string, 128);
        char ascii[64] = {0};
        in.toAscii(ascii, sizeof(ascii));
        int v = std::atoi(ascii);
        if (id == kParamTone) {
            if (v < 1) v = 1; if (v > 15) v = 15;
            valueNormalized = (v - 1) / 14.0;
            return kResultOk;
        }
        if (id == kParamPolyphony) {
            if (v < 1) v = 1; if (v > 32) v = 32;
            valueNormalized = (v - 1) / 31.0;
            return kResultOk;
        }
        if (id == kParamTableSize) {
            // Three-step: 64 -> 0.0, 128 -> 0.5, 256 -> 1.0
            if (v <= 96) valueNormalized = 0.0; // 64
            else if (v < 192) valueNormalized = 0.5; // 128
            else valueNormalized = 1.0; // 256
            return kResultOk;
        }
        if (id == kParamDetune) {
            // Expect string as cents or semitone float; accept like -0.25
            // Clamp to -0.5..+0.5
            double semis = std::atof(ascii);
            if (semis < -0.5) semis = -0.5;
            if (semis > 0.5) semis = 0.5;
            valueNormalized = (semis / 1.0) + 0.5; // map back to 0..1
            return kResultOk;
        }
        if (id == kParamVibratoDepth) {
            // Accept 0..0.5
            double semis = std::atof(ascii);
            if (semis < 0.0) semis = 0.0;
            if (semis > 0.5) semis = 0.5;
            valueNormalized = (semis / 0.5);
            return kResultOk;
        }
        if (id == kParamVibratoRate) {
            // Accept 0..16 Hz
            double hz = std::atof(ascii);
            if (hz < 0.0) hz = 0.0;
            if (hz > 16.0) hz = 16.0;
            valueNormalized = (hz / 16.0);
            return kResultOk;
        }
        if (id == kParamNoiseAdd) {
            // Accept 0..10000 percent (maps to ratio 0..100)
            int pct = std::atoi(ascii);
            if (pct < 0) pct = 0; if (pct > 10000) pct = 10000;
            float ratio = (float)pct / 100.0f;
            valueNormalized = (double)noiseRatioToNorm(ratio);
            return kResultOk;
        }
        if (id == kParamBLQuality) {
            // Accept Off/0..8 or words 1/Oct..8/Oct
            if (std::strcmp(ascii, "Off") == 0) { valueNormalized = 0.0; return kResultOk; }
            int q = std::atoi(ascii);
            if (q < 0) q = 0; if (q > 8) q = 8;
            valueNormalized = q / 8.0; // map 0..8 -> 0..1
            return kResultOk;
        }
        if (id == kParamHQMode) {
            if (std::strcmp(ascii, "Off") == 0) { valueNormalized = 0.0; return kResultOk; }
            if (std::strcmp(ascii, "Auto2x") == 0) { valueNormalized = 0.25; return kResultOk; }
            if (std::strcmp(ascii, "Force2x") == 0) { valueNormalized = 0.5; return kResultOk; }
            if (std::strcmp(ascii, "Force4x") == 0) { valueNormalized = 0.75; return kResultOk; }
            if (std::strcmp(ascii, "Force8x") == 0) { valueNormalized = 1.0; return kResultOk; }
            int m = std::atoi(ascii);
            if (m < 0) m = 0; if (m > 4) m = 4;
            valueNormalized = m / 4.0;
            return kResultOk;
        }
        if (id == kParamPreHighCutMode) {
            if (std::strcmp(ascii, "Off") == 0) { valueNormalized = 0.0; return kResultOk; }
            if (std::strcmp(ascii, "Fixed") == 0) { valueNormalized = 0.5; return kResultOk; }
            if (std::strcmp(ascii, "ByMaxNote") == 0) { valueNormalized = 1.0; return kResultOk; }
            int v = std::atoi(ascii);
            if (v < 0) v = 0; if (v > 2) v = 2;
            valueNormalized = (double)v / 2.0;
            return kResultOk;
        }
        if (id == kParamPreHighCutMaxNote) {
            int note = std::atoi(ascii);
            if (note < 0) note = 0; if (note > 127) note = 127;
            valueNormalized = (double)note / 127.0;
            return kResultOk;
        }
        return EditController::getParamValueByString(id, string, valueNormalized);
    }
    tresult PLUGIN_API initialize(FUnknown* ctx) SMTG_OVERRIDE {
        tresult r = EditController::initialize(ctx);
        if (r != kResultOk) return r;
        parameters.addParameter( STR16("Tone"), STR16(""), 14, 0.0, 0, kParamTone );
        parameters.addParameter( STR16("Attack"), nullptr, 0, 0.01, 0, kParamAttack );
        parameters.addParameter( STR16("Decay"), nullptr, 0, 0.2, 0, kParamDecay );
        parameters.addParameter( STR16("Sustain"), nullptr, 0, 0.6, 0, kParamSustain );
        parameters.addParameter( STR16("Release"), nullptr, 0, 0.3, 0, kParamRelease );
        parameters.addParameter( STR16("Gain"), nullptr, 0, 0.3, 0, kParamGain );
        parameters.addParameter( STR16("Polyphony"), nullptr, 31, 31.0/31.0, 0, kParamPolyphony ); // default 32
        // Three-step selector: 0=64, 1=128, 2=256 (default 128)
        parameters.addParameter( STR16("TableSize"), nullptr, 2, 0.5, 0, kParamTableSize );
        parameters.addParameter( STR16("Quantize4bit"), nullptr, 1, 1.0, 0, kParamQuantize4 ); // 0/1
        // Detune: -0.5 .. +0.5 semitone mapped to 0..1
        parameters.addParameter( STR16("Detune"), STR16("st"), 0, 0.5, 0, kParamDetune );
        // Vibrato Depth: 0 .. 0.5 st
        parameters.addParameter( STR16("VibratoDepth"), STR16("st"), 0, 0.0, 0, kParamVibratoDepth );
        // Vibrato Rate: 0 .. 16 Hz
        parameters.addParameter( STR16("VibratoRate"), STR16("Hz"), 0, 0.0, 0, kParamVibratoRate );
        // Additive noise depth (0..10000%). Internally maps to ratio 0..100 (1:100)
        parameters.addParameter( STR16("NoiseAdd"), STR16("%"), 0, 0.0, 0, kParamNoiseAdd );
        // Bandlimit quality selector (0..8)
        parameters.addParameter( STR16("Bandlimit"), STR16(""), 8, 0.0, 0, kParamBLQuality );
        // HQ mode selector (0..4)
        parameters.addParameter( STR16("HQMode"), STR16(""), 4, 0.0, 0, kParamHQMode );
        // PreHighCut mode (0..2)
        parameters.addParameter( STR16("PreHighCutMode"), STR16(""), 2, 0.0, 0, kParamPreHighCutMode );
        // PreHighCut MaxNote (0..127), default 64
        parameters.addParameter( STR16("PreHighCutMaxNote"), STR16("note"), 127, 64.0/127.0, 0, kParamPreHighCutMaxNote );
        return kResultOk;
    }
};

// Out-of-class definition to ensure linker symbol exists across translation units
Steinberg::FUnknown* Msm5232Controller::create(void*) {
    return (Steinberg::Vst::IEditController*)new Msm5232Controller();
}

#endif // HAVE_VST3_SDK

#ifdef HAVE_VST3_SDK
#include <public.sdk/source/main/pluginfactory.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>

#ifndef FULL_VERSION_STR
#define FULL_VERSION_STR "1.0.0"
#endif

// Forward declares of our classes (with create signatures)
namespace Steinberg { class FUnknown; }
struct Msm5232Processor { static Steinberg::FUnknown* create(void*); };
struct Msm5232Controller { static Steinberg::FUnknown* create(void*); };

// Company and plugin info
static const Steinberg::FUID kProcessorUID (0x8A49B8A1,0x1F5B4A40,0xA3B0B055,0xDFA0B929);
static const Steinberg::FUID kControllerUID (0x0B5C2B11,0xF2C84185,0x9F6E3CB5,0x88947733);

BEGIN_FACTORY_DEF ("msm5232vst", "https://example.com", "support@example.com")
    DEF_CLASS2 (INLINE_UID_FROM_FUID(kProcessorUID),
        PClassInfo::kManyInstances,
        kVstAudioEffectClass,
        "MSM5232 Synth",
        Vst::kDistributable,
        "Instrument|Synth",
        FULL_VERSION_STR,
        kVstVersionString,
        Msm5232Processor::create)

    DEF_CLASS2 (INLINE_UID_FROM_FUID(kControllerUID),
        PClassInfo::kManyInstances,
        kVstComponentControllerClass,
        "MSM5232 Controller",
        0,
        "",
        FULL_VERSION_STR,
        kVstVersionString,
        Msm5232Controller::create)
END_FACTORY

#endif // HAVE_VST3_SDK

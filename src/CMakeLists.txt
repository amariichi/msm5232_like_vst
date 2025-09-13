add_library(msm5232_dsp
    dsp/msm5232_wavetable.cpp
    dsp/bandlimited.cpp
    dsp/adsr.cpp
    dsp/voice.cpp
    dsp/synth.cpp
)
# Ensure MSVC treats sources as UTF-8 to avoid codepage warnings
if(MSVC)
    target_compile_options(msm5232_dsp PRIVATE /utf-8)
endif()
target_include_directories(msm5232_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(msm5232_render
    app/render_main.cpp
)
target_link_libraries(msm5232_render PRIVATE msm5232_dsp)
if(MSVC)
    target_compile_options(msm5232_render PRIVATE /utf-8)
endif()

if(BUILD_VST3)
    smtg_add_vst3plugin(msm5232_vst3
        SOURCES_LIST
            vst3/PluginController.cpp
            vst3/PluginProcessor.cpp
            vst3/factory.cpp
        )
    target_compile_definitions(msm5232_vst3 PRIVATE HAVE_VST3_SDK=1)
    # Ensure SDK expects a build mode macro
    target_compile_definitions(msm5232_vst3 PRIVATE NDEBUG=1)
    target_include_directories(msm5232_vst3 PRIVATE
        ${VST3_SDK_DIR}
        ${VST3_SDK_DIR}/pluginterfaces
        ${VST3_SDK_DIR}/public.sdk
        ${VST3_SDK_DIR}/base
        ${CMAKE_CURRENT_SOURCE_DIR}
    )
    # Link against Steinberg SDK libs
    target_link_libraries(msm5232_vst3 PRIVATE msm5232_dsp sdk base pluginterfaces)
    # Fallback: include pluginfactory implementation directly (avoids link issues on some setups)
    target_sources(msm5232_vst3 PRIVATE ${VST3_SDK_DIR}/public.sdk/source/main/pluginfactory.cpp)
    if(MSVC)
        target_compile_options(msm5232_vst3 PRIVATE /utf-8)
    endif()
endif()

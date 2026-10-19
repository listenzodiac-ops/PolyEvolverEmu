cmake_minimum_required(VERSION 3.22)

project(PolyEvolverEmu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Downloads JUCE automatically the first time you configure (needs internet + git).
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# Copying into ~/Library after the build is handy on your own Mac, but pointless on a CI runner.
if(DEFINED ENV{CI})
    set(EVO_COPY_AFTER_BUILD FALSE)
else()
    set(EVO_COPY_AFTER_BUILD TRUE)
endif()

juce_add_plugin(PolyEvolverEmu
    COMPANY_NAME                "YourName"
    PLUGIN_MANUFACTURER_CODE    Ynam
    PLUGIN_CODE                 Pevo
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Poly Evolver Emu"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    COPY_PLUGIN_AFTER_BUILD     ${EVO_COPY_AFTER_BUILD}
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    VST3_CATEGORIES             Instrument Synth)

juce_generate_juce_header(PolyEvolverEmu)

target_sources(PolyEvolverEmu PRIVATE
    Source/Params.h
    Source/Params.cpp
    Source/WaveTables.h
    Source/SynthVoice.h
    Source/StereoDelay.h
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp)

target_compile_definitions(PolyEvolverEmu PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PolyEvolverEmu
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

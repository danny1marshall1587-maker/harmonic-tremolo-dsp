cmake_minimum_required(VERSION 3.15)
project(HarmonicTremolo VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.9
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HarmonicTremolo
    COMPANY_NAME "OpenDSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE OpDSP
    PLUGIN_CODE Htrm
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Tri-Verb Harmonic Tremolo"
)

target_sources(HarmonicTremolo
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        ${CMAKE_CURRENT_SOURCE_DIR}/../include/HarmonicTremoloEngine.hpp
)

target_include_directories(HarmonicTremolo
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(HarmonicTremolo
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(HarmonicTremolo
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

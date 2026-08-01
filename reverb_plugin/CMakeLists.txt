cmake_minimum_required(VERSION 3.15)
project(CyberWaveReverb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

# Fetch JUCE 8.0.4
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(CyberWaveReverb
    COMPANY_NAME "OpenDSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_AUTO_MANIFEST FALSE
    PLUGIN_MANUFACTURER_CODE OpDSP
    PLUGIN_CODE CwRv
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "CyberWave Reverb"
)

juce_generate_juce_header(CyberWaveReverb)

target_sources(CyberWaveReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    ../embedded/mooer_ir_reverb.c
)

target_include_directories(CyberWaveReverb PRIVATE
    ../include
    ../embedded
    Source
)

target_link_libraries(CyberWaveReverb PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_graphics
)

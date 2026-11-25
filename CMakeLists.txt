cmake_minimum_required(VERSION 3.22)
project(FidiuCompressor VERSION 1.0.2)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch JUCE 8 automatically or point JUCE_DIR to your local JUCE installation
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(FidiuCompressor
    COMPANY_NAME "Fidiu Audio"
    PLUGIN_MANUFACTURER_CODE Fidu
    PLUGIN_CODE Fdc3
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Fidiu Compressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(FidiuCompressor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(FidiuCompressor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(FidiuCompressor PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_basics
)

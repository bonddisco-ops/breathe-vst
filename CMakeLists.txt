cmake_minimum_required(VERSION 3.22)

# Universal macOS build that also runs on older macOS versions
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
# Static MSVC runtime: the plugin does not depend on the host's C++ runtime DLLs
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Breathe VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Local test builds can point at an already downloaded JUCE folder
set(BRT_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
option(BRT_HEADLESS_TEST "Linux test build without optional system libraries" OFF)

if(BRT_JUCE_DIR)
    add_subdirectory(${BRT_JUCE_DIR} juce EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(BRT_FORMATS VST3 AU Standalone)
else()
    set(BRT_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Breathe
    COMPANY_NAME "Primal Rage Lab"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.primalragelab.breathe"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Distortion Mastering
    PLUGIN_MANUFACTURER_CODE Prlb
    PLUGIN_CODE Brth
    FORMATS ${BRT_FORMATS}
    COPY_PLUGIN_AFTER_BUILD FALSE
    PRODUCT_NAME "Breathe")

target_sources(Breathe PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Ui.cpp)

if(MSVC)
    target_compile_options(Breathe PRIVATE /utf-8)
endif()

target_compile_definitions(Breathe PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if(BRT_HEADLESS_TEST)
    target_compile_definitions(Breathe PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
        JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(Breathe
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags)

# Offline test tool: renders audio through the processor and a screenshot of the UI
if(BRT_HEADLESS_TEST)
    juce_add_console_app(BRTTest PRODUCT_NAME "BRTTest")
    target_sources(BRTTest PRIVATE
        Tests/OfflineTest.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Ui.cpp)
    target_include_directories(BRTTest PRIVATE Source)
    target_compile_definitions(BRTTest PRIVATE
        JucePlugin_Name="Breathe" JucePlugin_VersionString="0.1.0" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0
        JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0
        JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0
        JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0 JUCE_ALSA=0 JUCE_JACK=0)
    target_link_libraries(BRTTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

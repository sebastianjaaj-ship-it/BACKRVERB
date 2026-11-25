cmake_minimum_required(VERSION 3.22)

project(Backrverb VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>") # static CRT: no VC++ redistributable needed

# Use a local JUCE checkout if given (-DJUCE_DIR=...), otherwise download JUCE 8.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(BRV_SOURCES Source/PluginProcessor.cpp Source/PluginEditor.cpp)

juce_add_plugin(Backrverb
    COMPANY_NAME            "BACKRVERB"
    COMPANY_WEBSITE         "https://github.com/sebastianjaaj-ship-it/BACKRVERB"
    PLUGIN_MANUFACTURER_CODE Bkrv
    PLUGIN_CODE             Brv1
    FORMATS                 VST3
    PRODUCT_NAME            "BACKRVERB"
    VERSION                 1.0.0
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         Fx Reverb)

juce_generate_juce_header(Backrverb)
target_sources(Backrverb PRIVATE ${BRV_SOURCES})

target_compile_definitions(Backrverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Backrverb
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless test host (not needed for the plugin): -DBRV_BUILD_TESTS=ON
option(BRV_BUILD_TESTS "Build the headless test host" OFF)
if(BRV_BUILD_TESTS)
    juce_add_console_app(brv_host_test PRODUCT_NAME "brv_host_test")
    juce_generate_juce_header(brv_host_test)
    target_sources(brv_host_test PRIVATE tests/host_test.cpp ${BRV_SOURCES})
    target_compile_definitions(brv_host_test PRIVATE JucePlugin_Name="BACKRVERB" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(brv_host_test PRIVATE juce::juce_audio_processors juce::juce_dsp
        juce::juce_audio_formats juce::juce_recommended_config_flags)
endif()

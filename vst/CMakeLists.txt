cmake_minimum_required(VERSION 3.22)

project(TapPanZeit VERSION 13.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

set(TPZ_JUCE_TAG "8.0.15" CACHE STRING "JUCE git tag to build against")

include(FetchContent)
FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG ${TPZ_JUCE_TAG}
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(TapPanZeit
  COMPANY_NAME "Zack Steinkamp"
  COMPANY_WEBSITE "https://plugins.steinkamp.us"
  COMPANY_EMAIL "zack@steinkamp.us"
  BUNDLE_ID us.steinkamp.TapPanZeit
  PLUGIN_MANUFACTURER_CODE Zstk
  PLUGIN_CODE Tpzt
  PRODUCT_NAME "TapPanZeit"
  FORMATS VST3 AU Standalone
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE
  VST3_CATEGORIES Fx Delay
  AU_MAIN_TYPE kAudioUnitType_Effect
)

target_sources(TapPanZeit PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/FunctionEditor.cpp
)

target_compile_definitions(TapPanZeit PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TapPanZeit
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# Offline DSP checks: cmake --build build --target TapPanZeitTests && ./build/TapPanZeitTests_artefacts/Release/TapPanZeitTests
juce_add_console_app(TapPanZeitTests PRODUCT_NAME "TapPanZeitTests")
target_sources(TapPanZeitTests PRIVATE
  Tests/DspTest.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/FunctionEditor.cpp
)
target_compile_definitions(TapPanZeitTests PRIVATE
  JucePlugin_Name="TapPanZeit"
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)
target_link_libraries(TapPanZeitTests PRIVATE
  juce::juce_audio_utils
  juce::juce_recommended_config_flags
)

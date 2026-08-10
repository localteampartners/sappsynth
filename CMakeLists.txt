cmake_minimum_required(VERSION 3.24)

project(SappSynth VERSION 0.11.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(SAPPSYNTH_BUILD_PLUGIN "Build the JUCE plugin + standalone targets" ON)
option(SAPPSYNTH_BUILD_TESTS  "Build unit tests"                           ON)
option(SAPPSYNTH_BUILD_TOOLS  "Build offline CLI tools"                    ON)

include(cmake/CompilerWarnings.cmake)
include(FetchContent)

# ---------------------------------------------------------------- core ------
# Framework-independent DSP core. No JUCE, no plugin concepts — this is the
# library that unit tests, offline tools, and the plugin adapter all share.
add_library(sappsynth_core STATIC
  source/engine/SynthEngine.cpp
  source/engine/SynthVoice.cpp
  source/engine/VoiceManager.cpp
  source/engine/PresetPatch.cpp
  source/lab/OfflineRenderer.cpp
  source/lab/SignalAnalyzer.cpp
  # Framework-free despite living under source/plugin: the bank is plain data,
  # and the core needs it so level regressions can render every factory preset.
  source/plugin/FactoryPresets.cpp
)
target_include_directories(sappsynth_core PUBLIC source)
sappsynth_set_warnings(sappsynth_core)

# ---------------------------------------------------------------- tests -----
if(SAPPSYNTH_BUILD_TESTS)
  FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(sappsynth_tests
    tests/unit/test_random.cpp
    tests/unit/test_analyzer.cpp
    tests/unit/test_oscillator.cpp
    tests/unit/test_envelope.cpp
    tests/unit/test_filter.cpp
    tests/unit/test_mixer.cpp
    tests/unit/test_drift.cpp
    tests/unit/test_engine.cpp
    tests/unit/test_effects.cpp
    tests/unit/test_headroom.cpp
    tests/unit/test_gainstaging.cpp
    tests/unit/test_arp.cpp
    tests/unit/test_dna.cpp
    tests/unit/test_sapplink.cpp
    source/plugin/SappLinkCCMap.cpp
  )
  target_link_libraries(sappsynth_tests PRIVATE sappsynth_core Catch2::Catch2WithMain)
  target_compile_definitions(sappsynth_tests PRIVATE
    SAPPSYNTH_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
  sappsynth_set_warnings(sappsynth_tests)

  enable_testing()
  add_test(NAME unit COMMAND sappsynth_tests)
endif()

# ---------------------------------------------------------------- tools -----
if(SAPPSYNTH_BUILD_TOOLS)
  add_executable(sapp-render tools/sapp-render/main.cpp)
  target_link_libraries(sapp-render PRIVATE sappsynth_core)
  sappsynth_set_warnings(sapp-render)

  add_executable(sapp-bench tools/sapp-bench/main.cpp)
  target_link_libraries(sapp-bench PRIVATE sappsynth_core)
  sappsynth_set_warnings(sapp-bench)
endif()

# ---------------------------------------------------------------- plugin ----
if(SAPPSYNTH_BUILD_PLUGIN)
  # Pinned exact tag per docs/architecture.md §3.1. JUCE 9.0.0 shipped weeks
  # ago; we stay on the last stable 8.x until 9.x settles (see DECISIONS.md).
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(JUCE)

  juce_add_plugin(SappSynthPlugin
    PRODUCT_NAME                "SappSynth"
    COMPANY_NAME                "LocalTeamPartners"
    BUNDLE_ID                   com.localteampartners.sappsynth
    PLUGIN_MANUFACTURER_CODE    Ltpr
    PLUGIN_CODE                 Spsy
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    FORMATS                     Standalone VST3 AU
  )
  if(APPLE)
    # Copy into ~/Library/Audio/Plug-Ins locally; on Windows the default VST3
    # dir needs admin rights (breaks CI), so users/installers copy manually.
    juce_enable_copy_plugin_step(SappSynthPlugin)
  endif()

  juce_add_binary_data(SappSynthAssets SOURCES
    assets/wood_side.png
    assets/panel.png
    assets/knob_cream.png
    assets/knob_black.png
    assets/screw.png
  )

  target_sources(SappSynthPlugin PRIVATE
    source/plugin/PluginProcessor.cpp
    source/plugin/PluginEditor.cpp
    source/plugin/UserPresets.cpp
    source/plugin/SappLinkCCMap.cpp
  )
  target_link_libraries(SappSynthPlugin PRIVATE
    sappsynth_core
    SappSynthAssets
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
  )
  target_compile_definitions(SappSynthPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
  )

  if(APPLE)
    # Level audit: renders every factory preset and reports peak/RMS, so the
    # bank is levelled by measurement rather than by ear.
    juce_add_console_app(preset-audit PRODUCT_NAME "preset-audit")
    target_sources(preset-audit PRIVATE
      tools/preset-audit/main.cpp
      source/plugin/PluginProcessor.cpp
      source/plugin/PluginEditor.cpp
      source/plugin/UserPresets.cpp
      source/plugin/SappLinkCCMap.cpp
    )
    target_include_directories(preset-audit PRIVATE source/plugin)
    target_link_libraries(preset-audit PRIVATE
      sappsynth_core
      SappSynthAssets
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_recommended_config_flags
    )
    target_compile_definitions(preset-audit PRIVATE
      JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_VersionString="audit")

    # Offscreen editor snapshot tool (UI verification without screen capture).
    juce_add_gui_app(SappUiShot PRODUCT_NAME "SappUiShot")
    target_sources(SappUiShot PRIVATE
      tools/sapp-uishot/main.cpp
      source/plugin/PluginProcessor.cpp
      source/plugin/PluginEditor.cpp
      source/plugin/UserPresets.cpp
      source/plugin/SappLinkCCMap.cpp
    )
    target_include_directories(SappUiShot PRIVATE source/plugin)
    target_link_libraries(SappUiShot PRIVATE
      sappsynth_core
      SappSynthAssets
      juce::juce_audio_utils
      juce::juce_dsp
      juce::juce_recommended_config_flags
    )
    target_compile_definitions(SappUiShot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
  endif()
endif()

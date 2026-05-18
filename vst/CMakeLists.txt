# JUCE VST3 / Standalone wrapper for FxPatchSDK effects.
#
# Design and rationale: ../docs/vst-host-plan.md
# This build is fully separate from the ARM firmware build (the root Makefile).
# It references source/ and effects/ read-only and never modifies them.

cmake_minimum_required(VERSION 3.22)

project(FxPatchVST VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Effect selection -------------------------------------------------------
# Mirrors scripts/build_effects.sh --effect <name>: wrap one effects/<name>.cpp.
# Reconfigure for another effect with, e.g.:  cmake -S vst -B vst/build -DFX_EFFECT=chorus
set(FX_EFFECT "tube_screamer" CACHE STRING
    "Effect to wrap: the basename of a file in effects/ (no .cpp extension)")

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(FX_EFFECT_SRC "${REPO_ROOT}/effects/${FX_EFFECT}.cpp")

if(NOT EXISTS "${FX_EFFECT_SRC}")
    message(FATAL_ERROR
        "FX_EFFECT='${FX_EFFECT}' not found at ${FX_EFFECT_SRC}\n"
        "Set -DFX_EFFECT=<name> to the basename of a file in effects/.")
endif()
message(STATUS "FxPatchVST: wrapping effect '${FX_EFFECT}'")

# --- JUCE -------------------------------------------------------------------
# Pulled at configure time, never vendored into the repo. Pinned to a release
# tag for reproducibility — bump deliberately, never track a branch.
include(FetchContent)
FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
    GIT_PROGRESS   TRUE)
FetchContent_MakeAvailable(juce)

# --- Generated config header ------------------------------------------------
configure_file(src/EffectConfig.h.in
               "${CMAKE_CURRENT_BINARY_DIR}/generated/EffectConfig.h" @ONLY)

# --- Plugin target ----------------------------------------------------------
juce_add_plugin(FxPatchVST
    PRODUCT_NAME              "FxPatch ${FX_EFFECT}"
    COMPANY_NAME              "FxPatchSDK"
    BUNDLE_ID                 "dev.fxpatchsdk.fxpatchvst"
    PLUGIN_MANUFACTURER_CODE  Fxps
    PLUGIN_CODE               Fxp1
    FORMATS                   VST3 Standalone
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx)

target_sources(FxPatchVST PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    "${FX_EFFECT_SRC}")

target_include_directories(FxPatchVST PRIVATE
    "${REPO_ROOT}/source"
    "${CMAKE_CURRENT_BINARY_DIR}/generated")

# The effect translation unit is firmware code: compile its float literals the
# firmware way. Exceptions/RTTI stay enabled for the target as a whole because
# JUCE requires them; the host probe build (scripts/analyze_effects.py) makes
# the same compromise, so behavior parity with the probe holds.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties("${FX_EFFECT_SRC}" PROPERTIES
        COMPILE_OPTIONS "-fsingle-precision-constant")
endif()

target_compile_definitions(FxPatchVST PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FxPatchVST PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags)

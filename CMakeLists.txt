cmake_minimum_required(VERSION 3.22)

project(ForceSampleDelay VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE setup -----------------------------------------------------------
add_subdirectory(externals/JUCE)

set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(externals/clap-juce-extensions EXCLUDE_FROM_ALL)

# Common formats on Windows, macOS, and Linux
set(PLUGIN_FORMATS VST3)

# AUv3 requires macOS and Xcode.
# Standalone is the containing application for AUv3.
if(APPLE)
    list(APPEND PLUGIN_FORMATS AUv3 Standalone)
endif()

# ---------------------------
# Plugin Target
# ---------------------------
juce_add_plugin(ForceSampleDelay
    # Company
    COMPANY_NAME "Nylarte"
    COMPANY_WEBSITE ""
    COMPANY_EMAIL "reon.nynes@gmail.com"

    # Bundle / IDs
    # Adjust these if you already have a reverse-DNS you use for Apple code signing.
    BUNDLE_ID "com.nylarte.ForceSampleDelay"
    PLUGIN_MANUFACTURER_CODE Nylt
    PLUGIN_CODE Fspd

    # Product
    PRODUCT_NAME "Force Sample Delay"
    PLUGIN_NAME "Force Sample Delay"
    DESCRIPTION "Force a sample delay of 0 to 2000ms."

    # Plugin behavior
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    # Formats
    FORMATS ${PLUGIN_FORMATS}
	COPY_PLUGIN_AFTER_BUILD FALSE
)

# ---------------------------
# Sources
# ---------------------------
target_sources(ForceSampleDelay
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
)

# ---------------------------
# JUCE Modules / Linkage
# ---------------------------
target_compile_definitions(ForceSampleDelay
    PRIVATE
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ForceSampleDelay
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_processors_headless
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
        juce::juce_recommended_lto_flags
)

clap_juce_extensions_plugin(
    TARGET ForceSampleDelay
    CLAP_ID "com.nylarte.ForceSampleDelay"
    CLAP_FEATURES audio-effect delay
)
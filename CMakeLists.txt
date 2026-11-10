cmake_minimum_required(VERSION 3.22)
project(PulseDeck VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- JUCE (fetched automatically on first configure; needs internet access) ---
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PulseDeck
    COMPANY_NAME               "YourName"        # <- change me
    PLUGIN_MANUFACTURER_CODE   Ynme              # 4 chars, first one uppercase
    PLUGIN_CODE                Pdk1              # 4 chars, exactly one uppercase
    PRODUCT_NAME               "PulseDeck"
    IS_SYNTH                   TRUE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE
    FORMATS                    VST3 Standalone)   # add AU on macOS if you want it

target_sources(PulseDeck PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(PulseDeck PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PulseDeck
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- optional: JUCE-free test of the sound engine ---
add_executable(pulsedeck_core_test EXCLUDE_FROM_ALL tests/core_test.cpp)

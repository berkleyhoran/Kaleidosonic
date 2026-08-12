cmake_minimum_required(VERSION 3.22)

project(Kaleidosonic VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# ---------------------------------------------------------------------------
# Shader resources -> BinaryData
# ---------------------------------------------------------------------------
file(GLOB SHADER_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.vert
                       ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.frag
                       ${CMAKE_CURRENT_SOURCE_DIR}/Shaders/*.glsl)

juce_add_binary_data(KaleidosonicShaders SOURCES ${SHADER_FILES})
set_target_properties(KaleidosonicShaders PROPERTIES POSITION_INDEPENDENT_CODE ON)

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
# App/installer icon: drop a square-ish PNG at Assets/logo.png and it's
# picked up automatically on the next configure (JUCE generates the
# Windows .ico for the Standalone exe from it). Absent file = no icon,
# no error.
set(KALEIDOSONIC_LOGO "${CMAKE_CURRENT_SOURCE_DIR}/Assets/logo.png")
set(KALEIDOSONIC_ICON_ARGS "")
if(EXISTS ${KALEIDOSONIC_LOGO})
    set(KALEIDOSONIC_ICON_ARGS ICON_BIG "${KALEIDOSONIC_LOGO}" ICON_SMALL "${KALEIDOSONIC_LOGO}")
endif()

juce_add_plugin(Kaleidosonic
    ${KALEIDOSONIC_ICON_ARGS}
    COMPANY_NAME "Kaleidosonic"
    PLUGIN_MANUFACTURER_CODE Klds
    PLUGIN_CODE Kal1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Kaleidosonic"
    VST3_CATEGORIES "Fx" "Analyzer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(Kaleidosonic)

target_sources(Kaleidosonic PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/AudioAnalyzer.cpp
    Source/VisualizerParameters.cpp
    Source/Presets/PresetManager.cpp
    Source/Rendering/VisualizerRenderer.cpp
    Source/Rendering/FractalNavigator.cpp
    Source/Rendering/PipeNetwork.cpp
    Source/Rendering/MazeWalker.cpp)

target_compile_definitions(Kaleidosonic PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Kaleidosonic PRIVATE
    KaleidosonicShaders
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

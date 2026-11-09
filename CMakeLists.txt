cmake_minimum_required(VERSION 3.22)
project(CrestTamer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------------------------------------
# JUCE: due modi (scegline uno)
#   1) automatico: CMake lo scarica da GitHub (serve Git e internet)           -> nessuna opzione
#   2) cartella già presente sul computer:  cmake -B build -DJUCE_DIR=C:/percorso/JUCE
# ---------------------------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Cartella di JUCE già scaricata (vuoto = scarica da GitHub)")
option(CT_COPY_AFTER_BUILD "Copia il VST3 nella cartella di sistema dopo la compilazione (serve Amministratore su Windows)" OFF)

if(JUCE_DIR)
    add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(CrestTamer
    COMPANY_NAME               "Crest Tamer"
    PRODUCT_NAME               "Crest Tamer"
    PLUGIN_MANUFACTURER_CODE   Cstm
    PLUGIN_CODE                CrTm
    FORMATS                    VST3 Standalone
    VST3_CATEGORIES            Fx Dynamics
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    ${CT_COPY_AFTER_BUILD})

target_sources(CrestTamer PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(CrestTamer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CrestTamer
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

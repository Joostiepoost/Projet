# Projet — sketch timeline for acousmatic pieces
# Build instructions: see README.md (short version: ./build.sh)

cmake_minimum_required(VERSION 3.22)

# Apple Silicon only, macOS 12 or newer.
set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")

project(Projet VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- JUCE --------------------------------------------------------------------
# By default JUCE is downloaded automatically (first build takes a few minutes).
# Set PROJET_JUCE_PATH to use a local copy instead (for example Trajet's
# build/_deps/juce-src, to skip the download).
set(PROJET_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout")

if(PROJET_JUCE_PATH)
    add_subdirectory(${PROJET_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Sources shared by the app and the measurement program (no UI).
set(PROJET_CORE_SOURCES
    Source/Engine/SincKernel.cpp
    Source/Engine/SoundData.cpp
    Source/Engine/Importer.cpp
    Source/Engine/Arrangement.cpp
    Source/Engine/PlaybackEngine.cpp
    Source/Model/Project.cpp
    Source/Model/ProjectFolder.cpp)

set(PROJET_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_FLAC=1)

# --- The app -----------------------------------------------------------------
option(PROJET_BUILD_APP "Build the Projet app" ON)

if(PROJET_BUILD_APP)
juce_add_gui_app(Projet
    PRODUCT_NAME        "Projet"
    COMPANY_NAME        "Projet"
    BUNDLE_ID           "com.projet.app"
    VERSION             ${PROJECT_VERSION}
    MICROPHONE_PERMISSION_ENABLED FALSE)

juce_generate_juce_header(Projet)

target_sources(Projet PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/UI/Theme.cpp
    Source/UI/TimelineView.cpp
    Source/UI/Inspector.cpp
    Source/UI/TransportBar.cpp
    ${PROJET_CORE_SOURCES})

target_compile_definitions(Projet PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Projet,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:Projet,JUCE_VERSION>"
    ${PROJET_DEFINITIONS})

target_link_libraries(Projet
    PRIVATE
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_cryptography
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
endif()

# --- Measurement program (./build.sh tests) ---------------------------------
# Checks import (placement, length, level of Vorbis and FLAC copies),
# sample-accurate playback, fades, locating and CPU, and writes a report.
option(PROJET_BUILD_TESTS "Build the ProjetTests measurement program" OFF)

if(PROJET_BUILD_TESTS)
    juce_add_console_app(ProjetTests PRODUCT_NAME "ProjetTests")
    juce_generate_juce_header(ProjetTests)
    target_sources(ProjetTests PRIVATE Tests/EngineTests.cpp ${PROJET_CORE_SOURCES})
    target_compile_definitions(ProjetTests PRIVATE
        ${PROJET_DEFINITIONS} JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(ProjetTests
        PRIVATE
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_data_structures
            juce::juce_cryptography
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endif()

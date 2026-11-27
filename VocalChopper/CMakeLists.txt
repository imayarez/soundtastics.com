cmake_minimum_required(VERSION 3.22)

project(VocalChopper VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
endif()
if(MSVC)
    # Runtime estatico: el plugin no necesita que el usuario instale nada extra en Windows
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: si ya tienes una copia de JUCE en la carpeta "JUCE" junto a este archivo, se usa esa.
# Si no, se descarga sola (JUCE 8.0.9) la primera vez que configuras el proyecto.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VC_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND VC_FORMATS AU)
endif()

juce_add_plugin(VocalChopper
    COMPANY_NAME "Soundtastics"
    COMPANY_WEBSITE "https://soundtastics.com"
    BUNDLE_ID "com.soundtastics.vocalchopper"
    PLUGIN_MANUFACTURER_CODE Sndt
    PLUGIN_CODE Vchp
    PRODUCT_NAME "Vocal Chopper"
    FORMATS ${VC_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Vocal Chopper usa el microfono para grabar audio y chopearlo."
    VST3_CATEGORIES Instrument Sampler
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(VocalChopperData SOURCES
    Resources/fonts/ArchivoBlack-Regular.ttf
    Resources/fonts/Oswald-SemiBold.ttf
    Resources/img/by_soundtastics_white.png)

target_sources(VocalChopper PRIVATE
    Source/Analysis.cpp
    Source/Factory.cpp
    Source/Engine.cpp
    Source/Fx.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI.cpp)

target_compile_definitions(VocalChopper PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(VocalChopper PRIVATE /utf-8)
endif()

target_link_libraries(VocalChopper
    PRIVATE
        VocalChopperData
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

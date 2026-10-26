cmake_minimum_required(VERSION 3.24)

# macOS: universal (Intel x86_64 + Apple Silicon arm64), macOS 10.13+. Windows: x64, MSVC, runtime estático.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(SeniorMyAcousticGuitars VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE (local: -DFETCHCONTENT_SOURCE_DIR_JUCE=/caminho/para/JUCE)
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.16-security
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(SMS_FORMATS AU VST3 Standalone)
elseif(WIN32)
    set(SMS_FORMATS VST3 Standalone)
else()
    set(SMS_FORMATS Standalone)   # Linux: só para verificação local
endif()

juce_add_plugin(SeniorMyAcousticGuitars
    COMPANY_NAME "SeniorMy"
    COMPANY_COPYRIGHT "W. Carolino - SeniorMy - CNPJ 69.610.819/0001-77"
    COMPANY_WEBSITE "https://seniormy.com"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    BUNDLE_ID com.seniormy.acousticguitars
    PRODUCT_NAME "SeniorMy Acoustic Guitars"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Smny
    PLUGIN_CODE SmAg
    FORMATS ${SMS_FORMATS}
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    AU_SANDBOX_SAFE TRUE
    HARDENED_RUNTIME_ENABLED FALSE)

juce_add_binary_data(SeniorMyAcousticGuitarsData
    HEADER_NAME AGBinaryData.h
    NAMESPACE AGBinaryData
    SOURCES
        Resources/legal/termos-pt-BR.txt
        Resources/legal/termos-en.txt
        Resources/legal/THIRD-PARTY-NOTICES.txt
        Resources/logo-256.png
        Resources/wood-bg.jpg
        Resources/instruments/steel.png
        Resources/instruments/nylon.png
        Resources/instruments/flamenco.png
        Resources/instruments/twelve.png
        Resources/instruments/viola.png
        Resources/instruments/cavaquinho.png
        Resources/instruments/mandolin.png
        Resources/instruments/ukulele.png
        Resources/instruments/banjo.png
        Resources/instruments/sitar.png)

target_sources(SeniorMyAcousticGuitars PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Components.cpp
    Source/LookAndFeel.cpp
    Source/Terms.cpp
    Source/SampleLibrary.cpp
    Source/PresetManager.cpp
    Source/LoopImporter.cpp)

target_include_directories(SeniorMyAcousticGuitars PRIVATE Source)

target_compile_definitions(SeniorMyAcousticGuitars PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_OGGVORBIS=1
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0)

if(MSVC)
    target_compile_options(SeniorMyAcousticGuitars PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(SeniorMyAcousticGuitars
    PRIVATE
        SeniorMyAcousticGuitarsData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# --- Testes ------------------------------------------------------------------
# Núcleo (sem JUCE): digitações, campo harmônico, padrões, motor (render offline), presets.
add_executable(core_test tests/core_test.cpp)
target_include_directories(core_test PRIVATE Source)
if(MSVC)
    target_compile_options(core_test PRIVATE /utf-8 /bigobj)
endif()

# Biblioteca + importação + presets com JUCE (sem interface): carrega a biblioteca, toca cada
# instrumento, exporta MIDI e reimporta, confere presets JSON.
juce_add_console_app(library_test PRODUCT_NAME "library_test")
target_sources(library_test PRIVATE tests/library_test.cpp Source/SampleLibrary.cpp Source/LoopImporter.cpp Source/PresetManager.cpp)
target_include_directories(library_test PRIVATE Source)
target_compile_definitions(library_test PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(library_test PRIVATE juce::juce_core juce::juce_events juce::juce_audio_basics juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(library_test PRIVATE /utf-8 /bigobj)
endif()

# --- Demonstrações de áudio ----------------------------------------------------
# Render offline (WAV 48 kHz/24 bits) dos presets com amostras gravadas, pelo mesmo motor do plugin.
juce_add_console_app(render_demo PRODUCT_NAME "render_demo")
target_sources(render_demo PRIVATE tools/render_demo.cpp Source/SampleLibrary.cpp)
target_include_directories(render_demo PRIVATE Source)
target_compile_definitions(render_demo PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_FLAC=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1)
target_link_libraries(render_demo PRIVATE juce::juce_core juce::juce_events juce::juce_audio_basics juce::juce_audio_formats
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
if(MSVC)
    target_compile_options(render_demo PRIVATE /utf-8 /bigobj)
endif()

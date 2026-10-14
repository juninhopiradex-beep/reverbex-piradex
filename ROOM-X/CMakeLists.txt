cmake_minimum_required(VERSION 3.22)
project(RoomXByPiradex VERSION 1.0.0)

# -----------------------------------------------------------------------------
#  ROOM-X by Piradex — reverb modal musical (AU + VST3 macOS, VST3 Windows)
#
#  Compilar:   cmake -S ROOM-X -B build-roomx && cmake --build build-roomx --config Release
#  JUCE local: -DJUCE_PATH=/caminho/para/JUCE   (senão descarrega a 8.0.15)
#  Testes:     -DROOMX_BUILD_TESTS=ON
# -----------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Macs antigos (Catalina 10.15 / High Sierra 10.13, Intel) e novos (Apple Silicon)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS mínimo")
# Windows: runtime do Visual C++ embutido (não precisa do "VC++ Redistributable")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(ROOMX_FORMATS VST3 AU)
elseif(WIN32)
    set(ROOMX_FORMATS VST3)
else()
    set(ROOMX_FORMATS VST3 Standalone)   # Linux: só para desenvolvimento/testes
endif()

juce_add_plugin(RoomX
    COMPANY_NAME "Piradex"
    COMPANY_WEBSITE "https://www.beatfreakstudio.com"
    COMPANY_EMAIL "juninhopiradex@hotmail.com"
    BUNDLE_ID "com.piradex.roomx"
    PLUGIN_MANUFACTURER_CODE Prdx
    PLUGIN_CODE RmX1
    FORMATS ${ROOMX_FORMATS}
    PRODUCT_NAME "ROOM-X by Piradex"
    DESCRIPTION "Reverb modal musical"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

juce_add_binary_data(RoomXData SOURCES
    assets/fonts/Barlow-Regular.ttf
    assets/fonts/Barlow-Medium.ttf
    assets/fonts/Barlow-SemiBold.ttf)

set(ROOMX_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/dsp/EarlyReflections.cpp
    Source/dsp/FDNReverb.cpp
    Source/dsp/ModalBank.cpp
    Source/dsp/ReverbEngine.cpp
    Source/dsp/SpaceLibrary.cpp
    Source/music/MusicTheory.cpp
    Source/music/SequenceEngine.cpp
    Source/state/Parameters.cpp
    Source/state/PresetManager.cpp
    Source/state/FactoryPresets.cpp
    Source/state/SequenceModel.cpp
    Source/licensing/License.cpp
    Source/ui/LookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/NoteScape.cpp
    Source/ui/SpectrumView.cpp
    Source/ui/Panels.cpp
    Source/ui/SequencePanel.cpp
    Source/ui/TopBar.cpp
    Source/ui/LoginOverlay.cpp)

target_sources(RoomX PRIVATE ${ROOMX_SOURCES})
target_include_directories(RoomX PRIVATE Source)

target_compile_definitions(RoomX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    ROOMX_VERSION_STRING="${PROJECT_VERSION}")

# MSVC: ler o código-fonte como UTF-8 (textos em português)
if(MSVC)
    target_compile_options(RoomX PRIVATE /utf-8)
endif()

target_link_libraries(RoomX
    PRIVATE
        RoomXData
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
#  Testes
# -----------------------------------------------------------------------------
option(ROOMX_BUILD_TESTS "Compilar testes" OFF)
if(ROOMX_BUILD_TESTS)
    # núcleo DSP / música (sem JUCE)
    add_executable(roomx_dsp_test
        tests/test_dsp.cpp
        Source/dsp/EarlyReflections.cpp Source/dsp/FDNReverb.cpp Source/dsp/ModalBank.cpp
        Source/dsp/ReverbEngine.cpp Source/dsp/SpaceLibrary.cpp
        Source/music/MusicTheory.cpp Source/music/SequenceEngine.cpp)
    target_include_directories(roomx_dsp_test PRIVATE Source)

    # plugin completo (estado, presets, licença, transparência, render da UI)
    target_compile_definitions(RoomX PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1 ROOMX_ALLOW_TEST_UNLOCK=1)
    add_executable(roomx_plugin_test tests/test_plugin.cpp)
    target_include_directories(roomx_plugin_test PRIVATE Source $<TARGET_PROPERTY:RoomX,INCLUDE_DIRECTORIES>)
    target_compile_definitions(roomx_plugin_test PRIVATE $<TARGET_PROPERTY:RoomX,COMPILE_DEFINITIONS>)
    target_link_libraries(roomx_plugin_test PRIVATE RoomX)
endif()

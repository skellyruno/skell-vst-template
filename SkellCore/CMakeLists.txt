cmake_minimum_required(VERSION 3.22)
project(SkellCore VERSION 0.2.0 LANGUAGES CXX)

add_library(SkellCoreDSP INTERFACE)
add_library(SkellCore::DSP ALIAS SkellCoreDSP)
target_compile_features(SkellCoreDSP INTERFACE cxx_std_17)
target_include_directories(SkellCoreDSP INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

# This optional target is available only after JUCE's gui_basics target exists.
if(TARGET juce::juce_gui_basics)
  add_library(SkellCoreUI INTERFACE)
  add_library(SkellCore::UI ALIAS SkellCoreUI)
  target_compile_features(SkellCoreUI INTERFACE cxx_std_17)
  target_include_directories(SkellCoreUI INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(SkellCoreUI INTERFACE juce::juce_gui_basics juce::juce_audio_processors)
endif()

option(SKELLCORE_BUILD_TESTS "Build portable SkellCore DSP tests" OFF)
if(SKELLCORE_BUILD_TESTS)
  enable_testing()
  add_executable(SkellCoreTests tests/test_core.cpp)
  target_link_libraries(SkellCoreTests PRIVATE SkellCore::DSP)
  add_test(NAME SkellCoreTests COMMAND SkellCoreTests)
  add_executable(SkellCoreIntegrationsTests tests/test_integrations.cpp)
  target_link_libraries(SkellCoreIntegrationsTests PRIVATE SkellCore::DSP)
  add_test(NAME SkellCoreIntegrationsTests COMMAND SkellCoreIntegrationsTests)
  add_executable(SkellCoreMultirateTests tests/test_multirate.cpp)
  target_link_libraries(SkellCoreMultirateTests PRIVATE SkellCore::DSP)
  add_test(NAME SkellCoreMultirateTests COMMAND SkellCoreMultirateTests)
endif()

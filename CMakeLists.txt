cmake_minimum_required(VERSION 3.15)
project(HarmonicTremoloSuite VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(au_plugin)

enable_testing()
add_executable(test_runner tests/test_dsp.cpp embedded/mooer_harmonic_tremolo.c)
target_include_directories(test_runner PRIVATE include)
add_test(NAME dsp_tests COMMAND test_runner)

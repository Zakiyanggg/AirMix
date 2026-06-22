cmake_minimum_required(VERSION 3.16)

project(AirMix LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(airmix_demo src/main.cpp)
target_include_directories(airmix_demo PRIVATE src)

enable_testing()

add_executable(test_motion_detector src/tests/test_motion_detector.cpp)
target_include_directories(test_motion_detector PRIVATE src)
add_test(NAME test_motion_detector COMMAND test_motion_detector)

add_executable(test_tap_tempo src/tests/test_tap_tempo.cpp)
target_include_directories(test_tap_tempo PRIVATE src)
add_test(NAME test_tap_tempo COMMAND test_tap_tempo)

add_executable(test_effect_mapping src/tests/test_effect_mapping.cpp)
target_include_directories(test_effect_mapping PRIVATE src)
add_test(NAME test_effect_mapping COMMAND test_effect_mapping)


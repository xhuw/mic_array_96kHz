cmake_minimum_required(VERSION 3.21.0)
project(mic_that_does_it_all LANGUAGES C ASM CXX)


add_subdirectory(lib_xcore_math)
add_subdirectory(lib_mic_array)
add_subdirectory(lib_sw_pll)

add_executable(${PROJECT_NAME} main.cpp)
target_link_options(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/target.xn -fxscope)
target_compile_options(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/target.xn -g -O3 -mcmodel=large)
target_link_libraries(${PROJECT_NAME} lib_sw_pll lib_mic_array)


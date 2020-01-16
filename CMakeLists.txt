cmake_minimum_required(VERSION 2.8)
project(hs_hash VERSION 0.0.1 LANGUAGES C)

add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}:${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} INTERFACE c_std_11)
target_include_directories(${PROJECT_NAME} INTERFACE include/)


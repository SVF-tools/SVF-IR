cmake_minimum_required(VERSION 3.23)

project(
  SVFIR
  VERSION 0.0
  DESCRIPTION "SVFIR is an IR to target for SVF to consume"
  HOMEPAGE_URL "https://github.com/SVF-Tools/SVF-IR"
  LANGUAGES CXX
)

include(sources.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(svfir STATIC ${SVFIR_HEADERS} ${SVFIR_SOURCES})
target_include_directories(svfir PUBLIC svfir/include/)

add_executable(testsvfir test/svfir.cpp)
target_link_libraries(testsvfir PRIVATE svfir)

add_executable(testvalidation test/validation.cpp)
target_link_libraries(testvalidation PRIVATE svfir)

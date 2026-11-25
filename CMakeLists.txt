cmake_minimum_required(VERSION 3.23)

project(
  SVFIR
  VERSION 0.0
  DESCRIPTION "SVFIR is an IR to target for SVF to consume"
  HOMEPAGE_URL "https://github.com/SVF-Tools/SVF-IR"
  LANGUAGES CXX
)

include(cmake/sources.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(svfir STATIC ${SVFIR_HEADERS} ${SVFIR_SOURCES})
set_target_properties(svfir PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(svfir PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

install(
  TARGETS svfir
  EXPORT SVFIRTargets
  ARCHIVE DESTINATION lib
  INCLUDES DESTINATION include
)
install(DIRECTORY include/ DESTINATION include)

install(EXPORT SVFIRTargets
  FILE SVFIRTargets.cmake
  NAMESPACE SVFIR::
  DESTINATION lib/cmake/SVFIR
)

install(FILES cmake/SVFIRConfig.cmake
  DESTINATION lib/cmake/SVFIR
)

if(PROJECT_IS_TOP_LEVEL)
  add_executable(test test/test.cpp)
  target_link_libraries(test PRIVATE svfir)
endif()

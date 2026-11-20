cmake_minimum_required(VERSION 3.16)

project(ssc32 VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SSC32_BUILD_TESTING "Build the test suite" ON)
option(SSC32_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(ssc32
  src/controller.cpp
  src/protocol.cpp
  src/serial_transport.cpp
)
add_library(ssc32::ssc32 ALIAS ssc32)

target_compile_features(ssc32 PUBLIC cxx_std_17)
target_include_directories(ssc32
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(ssc32 PRIVATE
    -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    $<$<BOOL:${SSC32_WARNINGS_AS_ERRORS}>:-Werror>
  )
endif()

set_target_properties(ssc32 PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME ssc32
)

install(TARGETS ssc32 EXPORT ssc32Targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT ssc32Targets
  FILE ssc32Targets.cmake
  NAMESPACE ssc32::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ssc32
)

configure_package_config_file(
  cmake/ssc32Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/ssc32Config.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ssc32
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/ssc32ConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/ssc32Config.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/ssc32ConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ssc32
)

if(SSC32_BUILD_TESTING)
  enable_testing()
  add_executable(ssc32_tests tests/ssc32_tests.cpp)
  target_link_libraries(ssc32_tests PRIVATE ssc32::ssc32)
  target_compile_features(ssc32_tests PRIVATE cxx_std_17)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ssc32_tests PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
      $<$<BOOL:${SSC32_WARNINGS_AS_ERRORS}>:-Werror>
    )
  endif()
  add_test(NAME ssc32_tests COMMAND ssc32_tests)
endif()

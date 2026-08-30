cmake_minimum_required(VERSION 3.20)
project(zenoh_node)

include(FetchContent)

# Find cargo at configure time so the build step does not rely on PATH
find_program(CARGO_EXECUTABLE
  NAMES cargo
  HINTS
    "$ENV{HOME}/.cargo/bin"
    "$ENV{CARGO_HOME}/bin"
  REQUIRED
)
message(STATUS "Found cargo: ${CARGO_EXECUTABLE}")

# zenoh-cpp is header-only — no build step needed
FetchContent_Declare(
  zenoh_cpp
  GIT_REPOSITORY https://github.com/eclipse-zenoh/zenoh-cpp.git
  GIT_TAG        1.9.0
  GIT_SHALLOW    TRUE
)

# zenoh-c is the compiled Rust/C library that zenoh-cpp wraps.
# PATCH_COMMAND replaces the bare 'cargo' invocation with the full path so
# the make build step does not depend on cargo being in PATH at build time.
FetchContent_Declare(
  zenoh_c
  GIT_REPOSITORY https://github.com/eclipse-zenoh/zenoh-c.git
  GIT_TAG        1.9.0
  GIT_SHALLOW    TRUE
  PATCH_COMMAND  ${CMAKE_COMMAND}
    -DSOURCE_DIR=<SOURCE_DIR>
    -DCARGO=${CARGO_EXECUTABLE}
    -P ${CMAKE_SOURCE_DIR}/cmake/patch-zenoh-c-cargo.cmake
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(zenoh_c zenoh_cpp)

# zenoh-cpp is header-only — grab its include path from the fetched source
FetchContent_GetProperties(zenoh_cpp SOURCE_DIR ZENOH_CPP_SOURCE_DIR)

# node-addon-api may be hoisted by the consuming project's package manager,
# so use Node's own resolver rather than a hardcoded relative path.
execute_process(
  COMMAND node -e "process.stdout.write(require('node-addon-api').include.replace(/\"/g, ''))"
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE NODE_ADDON_API_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if(NOT EXISTS "${NODE_ADDON_API_DIR}/napi.h")
  message(FATAL_ERROR "napi.h not found — node-addon-api not installed or not resolvable from ${CMAKE_SOURCE_DIR}")
endif()
message(STATUS "node-addon-api: ${NODE_ADDON_API_DIR}")

add_library(${PROJECT_NAME} SHARED src/zenoh.cpp ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_JS_INC}
  ${NODE_ADDON_API_DIR}
  ${ZENOH_CPP_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
  ${CMAKE_JS_LIB}
  zenohc::static
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  NAPI_VERSION=8
  NAPI_DISABLE_CPP_EXCEPTIONS
  ZENOHCXX_ZENOHC
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

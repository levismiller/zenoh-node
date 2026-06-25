cmake_minimum_required(VERSION 3.20)
project(zenoh_node)

include(FetchContent)

# zenoh-cpp is header-only — no build step needed
FetchContent_Declare(
  zenoh_cpp
  GIT_REPOSITORY https://github.com/eclipse-zenoh/zenoh-cpp.git
  GIT_TAG        1.9.0
  GIT_SHALLOW    TRUE
)

# zenoh-c is the compiled Rust/C library that zenoh-cpp wraps.
# Requires Rust + Cargo toolchain at build time.
FetchContent_Declare(
  zenoh_c
  GIT_REPOSITORY https://github.com/eclipse-zenoh/zenoh-c.git
  GIT_TAG        1.9.0
  GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(zenoh_c zenoh_cpp)

# zenoh-cpp is header-only — grab its include path from the fetched source
FetchContent_GetProperties(zenoh_cpp SOURCE_DIR ZENOH_CPP_SOURCE_DIR)

# node-addon-api ships with napi.h — reference it directly from node_modules
set(NODE_ADDON_API_DIR "${CMAKE_SOURCE_DIR}/node_modules/node-addon-api")

add_library(${PROJECT_NAME} SHARED src/zenoh.cpp ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_JS_INC}
  ${NODE_ADDON_API_DIR}
  ${ZENOH_CPP_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
  ${CMAKE_JS_LIB}
  zenohc::lib
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  NAPI_VERSION=8
  NAPI_DISABLE_CPP_EXCEPTIONS
  ZENOHCXX_ZENOHC
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

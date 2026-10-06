# Caprice32 -> Symbian (EKA2). Configure with the SDK toolchain:
#   cmake -S symbian -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$SDK/cmake/symbian-arm.cmake \
#         -DSYMBIAN_SDK_PREFIX=$SDK -DCMAKE_MAKE_PROGRAM=$SDK/bin/ninja -DCAP32_ROOT=<patched repo>
cmake_minimum_required(VERSION 3.20)
project(caprice32_symbian CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CAP32_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Patched Caprice32 source tree")
find_package(Symbian CONFIG QUIET)   # SDK provides targets Symbian::Streams etc.

file(GLOB CORE_SRC ${CAP32_ROOT}/src/*.cpp ${CAP32_ROOT}/src/capsimg/*/*.cpp)
list(FILTER CORE_SRC EXCLUDE REGEX "glfuncs\\.cpp$|/gui/")
file(GLOB ZLIB_SRC ${CMAKE_SOURCE_DIR}/third_party/zlib/*.c)

add_executable(caprice32
  ${CORE_SRC} ${ZLIB_SRC}
  shim/sdl_shim.cc shim/vkbd.cc app/app_main.cc app/backend_ws.cc app/vkbd_symbian.cc)

target_include_directories(caprice32 BEFORE PRIVATE
  ${CMAKE_SOURCE_DIR}/shim ${CMAKE_SOURCE_DIR}/third_party/zlib
  ${CAP32_ROOT} ${CAP32_ROOT}/src
  ${CAP32_ROOT}/src/capsimg/LibIPF ${CAP32_ROOT}/src/capsimg/Device ${CAP32_ROOT}/src/capsimg/CAPSImg
  ${CAP32_ROOT}/src/capsimg/Codec ${CAP32_ROOT}/src/capsimg/Core)
target_compile_definitions(caprice32 PRIVATE CAPRICE_NO_WGUI NDEBUG HAVE_UNISTD_H)
target_compile_options(caprice32 PRIVATE -O2 -fno-exceptions -fno-rtti -w
                       $<$<COMPILE_LANGUAGE:C>:-std=gnu99>)
# Symbian::Streams = libc++ with string streams (file streams come from shim/fstream)
target_link_libraries(caprice32 PRIVATE Symbian::Streams)

# Platform / startup glue copied from the SDK's examples/gui_app by CI (see workflow).
if(EXISTS ${CMAKE_SOURCE_DIR}/glue/CMakeLists.inc)
  include(${CMAKE_SOURCE_DIR}/glue/CMakeLists.inc)
endif()
symbian_publish_executable(caprice32 UID3 0xe0000831)

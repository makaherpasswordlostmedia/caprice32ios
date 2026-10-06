# Caprice32 -> Symbian (EKA2). Configure with the SDK toolchain:
#   cmake -S symbian -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=$SDK/cmake/symbian-arm.cmake \
#         -DSYMBIAN_SDK_PREFIX=$SDK -DCMAKE_MAKE_PROGRAM=$SDK/bin/ninja -DCAP32_ROOT=<patched repo>
cmake_minimum_required(VERSION 3.20)
project(caprice32_symbian CXX C ASM)
set(CMAKE_CXX_STANDARD 17)
set(CAP32_ROOT "${CMAKE_SOURCE_DIR}/.." CACHE PATH "Patched Caprice32 source tree")
# The SDK ships its helper commands (symbian_publish_executable, Symbian::* targets) as CMake
# modules/packages under <SDK>/cmake. Load whatever defines them.
list(APPEND CMAKE_MODULE_PATH "${SYMBIAN_SDK_PREFIX}/cmake")
list(APPEND CMAKE_PREFIX_PATH "${SYMBIAN_SDK_PREFIX}" "${SYMBIAN_SDK_PREFIX}/cmake" "${SYMBIAN_SDK_PREFIX}/lib/cmake")
foreach(_pkg Symbian SymbianSdk SymbianSDK symbian-sdk)
  if(NOT COMMAND symbian_publish_executable)
    find_package(${_pkg} CONFIG QUIET)
  endif()
endforeach()
if(NOT COMMAND symbian_publish_executable)
  file(GLOB _sdk_mods "${SYMBIAN_SDK_PREFIX}/cmake/*.cmake")
  message(STATUS "SDK cmake modules: ${_sdk_mods}")
  foreach(_m ${_sdk_mods})
    get_filename_component(_n ${_m} NAME)
    # Host* modules are for building the host SDK (they fail on target configs); toolchain is already loaded.
    if(NOT _n MATCHES "Host|toolchain|symbian-arm|Config\\.cmake$|Version\\.cmake$" AND NOT COMMAND symbian_publish_executable)
      message(STATUS "including ${_n}")
      include(${_m} OPTIONAL)
    endif()
  endforeach()
endif()
if(NOT COMMAND symbian_publish_executable)
  message(FATAL_ERROR "SDK helper commands not found. Contents of ${SYMBIAN_SDK_PREFIX}/cmake were printed by the 'Inspect SDK' CI step.")
endif()

file(GLOB CORE_SRC ${CAP32_ROOT}/src/*.cpp ${CAP32_ROOT}/src/capsimg/*/*.cpp)
list(FILTER CORE_SRC EXCLUDE REGEX "glfuncs\\.cpp$|/gui/")
file(GLOB ZLIB_SRC ${CMAKE_SOURCE_DIR}/third_party/zlib/*.c)

set(CAP_SOURCES ${CORE_SRC} ${ZLIB_SRC}
  shim/sdl_shim.cc shim/vkbd.cc app/app_main.cc app/backend_ws.cc app/vkbd_symbian.cc)

# Same wiring as the SDK's examples/gui_app: import proxies (libc, libm, euser, ...) + startup bridge + linker script.
# CI copies startup.S / startup.cc / image.ld from the SDK example into symbian/glue.
foreach(dll euser ws32 gdi libc libm libpthread drtaeabi)
  list(APPEND SYMBIAN_IMPORT_PROXIES "${SYMBIAN_SDK_PREFIX}/proxies/${dll}/${dll}.dso")
endforeach()
set(GLUE ${CMAKE_SOURCE_DIR}/glue)
if(COMMAND symbian_add_import_executable AND EXISTS ${GLUE}/startup.S AND EXISTS ${GLUE}/image.ld)
  symbian_add_import_executable(caprice32 STARTUP ${GLUE}/startup.S LINKER_SCRIPT ${GLUE}/image.ld
    SOURCES ${CAP_SOURCES} ${GLUE}/startup.cc)
else()
  message(WARNING "symbian_add_import_executable or glue files missing; falling back to add_executable (libc will not link)")
  add_executable(caprice32 ${CAP_SOURCES})
endif()
# --error-limit=0: report every unresolved symbol in one run (default stops after 20).
target_link_options(caprice32 PRIVATE --gc-sections --error-limit=0)

target_include_directories(caprice32 BEFORE PRIVATE
  ${CMAKE_SOURCE_DIR}/shim ${CMAKE_SOURCE_DIR}/third_party/zlib
  ${CAP32_ROOT} ${CAP32_ROOT}/src
  ${CAP32_ROOT}/src/capsimg/LibIPF ${CAP32_ROOT}/src/capsimg/Device ${CAP32_ROOT}/src/capsimg/CAPSImg
  ${CAP32_ROOT}/src/capsimg/Codec ${CAP32_ROOT}/src/capsimg/Core)
target_compile_definitions(caprice32 PRIVATE CAPRICE_NO_WGUI NDEBUG HAVE_UNISTD_H PATH_MAX=260)
target_compile_options(caprice32 PRIVATE -O2 -fno-exceptions -fno-rtti -w
                       $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
                       "$<$<COMPILE_LANGUAGE:C>:SHELL:-include ${CMAKE_SOURCE_DIR}/shim/c_compat.h>")
# Symbian::Streams = libc++ with string streams (file streams come from shim/fstream)
target_link_libraries(caprice32 PRIVATE Symbian::Streams)

# Platform / startup glue copied from the SDK's examples/gui_app by CI (see workflow).
if(EXISTS ${CMAKE_SOURCE_DIR}/glue/CMakeLists.inc)
  include(${CMAKE_SOURCE_DIR}/glue/CMakeLists.inc)
endif()
symbian_publish_executable(caprice32 UID3 0xe0000831)

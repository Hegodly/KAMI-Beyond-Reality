cmake_minimum_required(VERSION 3.24)
project(KAMI_Beyond_Reality LANGUAGES C CXX)
if(NOT WIN32)
  message(FATAL_ERROR "KAMI currently supports Windows only")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Use a pinned OpenJPEG source release. This is built statically in the
# Windows artifact; do not require users to install an external DLL.
include(FetchContent)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_CODEC OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_Declare(openjpeg
  GIT_REPOSITORY https://github.com/uclouvain/openjpeg.git
  # Security: v2.5.4 omits upstream decoder bounds fix 91d08b11.
  # This later pinned commit also includes the encoder overflow correction.
  GIT_TAG 839936aa33eb8899bbbd80fda02796bb65068951
  GIT_SHALLOW FALSE)
FetchContent_MakeAvailable(openjpeg)
# zlib is used only to inflate Second Life's bounded, compressed mesh LOD
# blocks. Statically link it into the Windows executable.
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG v1.3.1
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(zlib)
if(NOT TARGET zlibstatic)
  message(FATAL_ERROR "zlibstatic target missing")
endif()
if(NOT TARGET openjp2)
  message(FATAL_ERROR "OpenJPEG openjp2 target is unavailable")
endif()
# OpenJPEG's CMake otherwise sends executables into its own bin folder.
# Keep KAMI's published Windows artifact path stable.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src")
add_subdirectory(src)

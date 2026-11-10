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
  GIT_TAG v2.5.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(openjpeg)
if(NOT TARGET openjp2)
  message(FATAL_ERROR "OpenJPEG openjp2 target is unavailable")
endif()
add_subdirectory(src)

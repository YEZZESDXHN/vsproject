cmake_minimum_required(VERSION 3.10)

project(CaplDllExample VERSION 0.0)

add_library(capldll SHARED ../Sources/capldll.cpp)


if (WIN32)
  # Do not use the library file name prefix 'lib' on windows platforms (x86 and x64).
  set_target_properties(capldll PROPERTIES PREFIX "")
else()
  # Do not export all functions by default
  target_compile_options(capldll PUBLIC "-fvisibility=hidden")
endif()

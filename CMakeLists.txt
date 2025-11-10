cmake_minimum_required(VERSION 3.25)

project(
  pycc
  VERSION 0.1.0
  DESCRIPTION "pycc: AOT Python 3 compiler targeting LLVM"
  LANGUAGES CXX)

# Modular CMake configuration
include(cmake/ProjectOptions.cmake)
include(cmake/Sources.cmake)
include(cmake/Targets.cmake)
include(cmake/Testing.cmake)
include(cmake/ClangTidyPlugins.cmake)

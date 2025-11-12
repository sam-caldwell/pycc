cmake_minimum_required(VERSION 3.25)

# This CMakeLists is intentionally minimal. Tests are discovered via
# cmake/Testing.cmake using the pattern:
#   test/{component}/{unit,integration,e2e}/*.cpp
# No direct add_subdirectory() calls are needed here.

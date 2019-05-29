

cmake_minimum_required(VERSION 3.6)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_CURRENT_BINARY_DIR)
  message(FATAL_ERROR
    "In-source builds are not supported.  Please create a build directory "
    "separate from the source directory")
endif()

project(ADIOS2_C_Examples VERSION 0.1)

enable_language(C)
find_package(ADIOS2 REQUIRED)
find_package(MPI REQUIRED)

# Workaround for various MPI implementations forcing the link of C++ bindings
add_definitions(-DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX)

add_subdirectory(examples) 

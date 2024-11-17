cmake_minimum_required(VERSION 3.10)
project(interpose)

# Set the output directory for the shared library
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Create the shared library
add_library(interpose SHARED interpose.c)

# Link with pthread
target_link_libraries(interpose PRIVATE pthread)
target_link_libraries(interpose PRIVATE dl)

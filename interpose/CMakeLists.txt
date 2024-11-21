# Create the shared library
add_library(interpose SHARED interpose.c interpose.h)
set_target_properties(interpose PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Link with pthread & dl
target_link_libraries(interpose PRIVATE pthread dl)

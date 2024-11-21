add_library(interpose STATIC interpose.c interpose.h)
set_target_properties(interpose PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Link with pthread & dl
target_link_libraries(interpose PRIVATE pthread dl)

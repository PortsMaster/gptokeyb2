# Find freedesktop.org's libevdev headers and library 
# This module defines the following variables:
#
#  EVDEV_FOUND          - true if libevdev header and library was found
#  EVDEV_INCLUDE_DIR    - include path for libevdev
#  EVDEV_LIBRARY       - library path for libevdev
#

SET(EVDEV_SEARCH_PATHS
    /usr/local
    /usr
    /
)

FIND_PATH(LIBEVDEV_INCLUDE_DIR libevdev/libevdev.h
    HINTS $ENV{EVDEVDIR}
    PATH_SUFFIXES include/libevdev-1.0 include
    PATHS ${EVDEV_SEARCH_PATHS}
)

FIND_LIBRARY(LIBEVDEV_LIBRARY
    NAMES libevdev.a evdev
    HINTS $ENV{EVDEVDIR}
    PATH_SUFFIXES lib lib64
    PATHS ${EVDEV_SEARCH_PATHS}
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LIBEVDEV REQUIRED_VARS LIBEVDEV_LIBRARY LIBEVDEV_INCLUDE_DIR)

mark_as_advanced(LIBEVDEV_INCLUDE_DIR)

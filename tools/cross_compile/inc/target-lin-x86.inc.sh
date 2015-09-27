#!/bin/sh
MINGW_DIR=""
TLIB_DIR="lib/"
ENABLE_SHARED="YES"
SHARED_LIBRARY_EXT="so"
DATA_DIR=""
HOST=
HOST_PREF=""
CFLAGS_COMMON="${CFLAGS} -m32"
CFLAGS_SOURCE="-D_M_IX86"
CXXLAGS="${CXXLAGS} -m32"
LDFLAGS="${LDFLAGS} -m32"
COMMAND_ENV="ARCH=i386"
CMAKE_TOOLCHAIN_FILE="cmake/i386-linux-gnu.cmake"
CMAKE_C_FLAGS="-m32"
CMAKE_CXX_FLAGS="-m32"
CMAKE_FIND_ROOT_PATH=~/deb/temp

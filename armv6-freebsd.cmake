# Build with "cmake -DCMAKE_TOOLCHAIN_FILE=../armv6-freebsd.cmake .."
set(CMAKE_SYSTEM_NAME FreeBSD)
set(CMAKE_C_COMPILER /usr/armv6-freebsd/usr/bin/cc)
set(CMAKE_CXX_COMPILER /usr/armv6-freebsd/usr/bin/c++)
set(CMAKE_FIND_ROOT_PATH /usr/armv6-freebsd)

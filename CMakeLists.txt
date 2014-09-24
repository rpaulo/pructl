cmake_minimum_required(VERSION 2.8)
project(pructl)
add_executable(pructl pructl.c)
include_directories(/usr/local/include ${CMAKE_SOURCE_DIR}/../libpru)
find_library(pru_library NAMES pru PATHS /usr/local/lib ${CMAKE_SOURCE_DIR}/../libpru/build)
target_link_libraries(pructl ${pru_library})
set(CMAKE_C_FLAGS "-Weverything -Werror")
install(TARGETS pructl DESTINATION sbin)



cmake_minimum_required(VERSION 2.8)
project(pructl)
add_executable(pructl pructl.c)
find_library(pru_library pru /usr/local/lib)
target_link_libraries(pructl ${pru_library})
set(CMAKE_C_FLAGS "-Weverything -Werror")
install(TARGETS pructl DESTINATION sbin)



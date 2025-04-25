cmake_minimum_required(VERSION 3.29)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_CXX_EXTENSIONS off)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
project(tutorSocket VERSION 1.0.0 DESCRIPTION "A tutor on socket programming")

find_package(fmt CONFIG REQUIRED)


aux_source_directory(src/ SOURCE_FILES)
add_executable(tutorSocket ${SOURCE_FILES})
target_include_directories(tutorSocket PUBLIC include)
target_link_libraries(tutorSocket PRIVATE fmt::fmt)
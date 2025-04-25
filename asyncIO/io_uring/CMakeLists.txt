cmake_minimum_required(VERSION 3.30)
project(io_uring LANGUAGES CXX VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(io_uring
    main.cpp
)

target_link_libraries(io_uring
    -luring
)

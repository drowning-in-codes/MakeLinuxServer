add_executable(testThread ${CMAKE_CURRENT_SOURCE_DIR}/testThread.cpp ${CMAKE_SOURCE_DIR}/src/CurrentThread.cpp)
target_include_directories(testThread PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(testEpoll ${CMAKE_CURRENT_SOURCE_DIR}/testEpoll.cpp)

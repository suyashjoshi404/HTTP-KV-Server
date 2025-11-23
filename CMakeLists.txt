cmake_minimum_required(VERSION 3.16)
project(HTTP-KV-Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_executable(kv_server src/server.cpp src/db_handler.cpp)
target_include_directories(kv_server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(kv_server PRIVATE Threads::Threads)

find_library(MYSQLCLIENT_LIB NAMES mysqlclient)
if (MYSQLCLIENT_LIB)
    target_link_libraries(kv_server PRIVATE ${MYSQLCLIENT_LIB})
else()
    message(WARNING "libmysqlclient not found. Install libmysqlclient-dev.")
endif()

add_executable(load_generator src/load_generator.cpp)
target_include_directories(load_generator PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(load_generator PRIVATE Threads::Threads)

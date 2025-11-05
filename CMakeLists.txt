cmake_minimum_required(VERSION 3.10)
project(HTTP-KV-Server)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2 -pthread")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

# Server executable
add_executable(kv_server src/server.cpp src/db_handler.cpp)

# Load generator executable
add_executable(load_generator src/load_generator.cpp)

# Link MySQL client library only to server
find_library(MYSQLCLIENT_LIB NAMES mysqlclient)
if (NOT MYSQLCLIENT_LIB)
    message(WARNING "libmysqlclient not found. Install libmysqlclient-dev.")
endif()
target_link_libraries(kv_server PRIVATE ${MYSQLCLIENT_LIB})

cmake_minimum_required(VERSION 3.16)
project(FeminizmasAnalize LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(CURL REQUIRED)

add_executable(projektas
    main.cpp
)

target_link_libraries(projektas PRIVATE CURL::libcurl)


cmake_minimum_required(VERSION 3.15)
project(easygamechat_consumer LANGUAGES CXX)

find_package(easygamechat CONFIG REQUIRED)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE easygamechat::easygamechat)

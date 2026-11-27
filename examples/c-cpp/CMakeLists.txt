cmake_minimum_required(VERSION 3.15)
project(chat_client LANGUAGES CXX)

add_subdirectory(../../clients/cpp easygamechat)

add_executable(chat_client main.cpp)
target_link_libraries(chat_client PRIVATE easygamechat::easygamechat)

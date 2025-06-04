cmake_minimum_required(VERSION 3.15)
project(chat_client)

find_package(nlohmann_json CONFIG REQUIRED)

if(NOT WIN32)
  find_package(OpenSSL REQUIRED)
endif()

add_executable(chat_client main.cpp)

if(WIN32)
  target_link_libraries(chat_client
          PRIVATE
          nlohmann_json::nlohmann_json
          ws2_32
          secur32
          crypt32
  )
else()
  find_package(Threads REQUIRED)
  target_link_libraries(chat_client
          PRIVATE
          nlohmann_json::nlohmann_json
          Threads::Threads
          OpenSSL::SSL
          OpenSSL::Crypto
  )
endif()
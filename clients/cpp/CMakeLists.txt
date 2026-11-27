cmake_minimum_required(VERSION 3.15)
project(easygamechat VERSION 0.4.0 LANGUAGES C CXX)

option(EASYGAMECHAT_BUILD_TESTS "Build EasyGameChat tests" OFF)

find_package(nlohmann_json CONFIG REQUIRED)

add_library(easygamechat STATIC
  src/c_api.cpp
  src/easygamechat.cpp
  src/tls_socket.cpp
  src/validation.cpp
)
add_library(easygamechat::easygamechat ALIAS easygamechat)

set_target_properties(easygamechat PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(easygamechat PUBLIC cxx_std_14)
target_include_directories(easygamechat
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(easygamechat PRIVATE $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>)

if(WIN32)
  target_link_libraries(easygamechat PUBLIC ws2_32 secur32 crypt32)
else()
  find_package(OpenSSL REQUIRED)
  find_package(Threads REQUIRED)
  target_link_libraries(easygamechat PUBLIC OpenSSL::SSL OpenSSL::Crypto Threads::Threads)
endif()

include(GNUInstallDirs)

install(TARGETS easygamechat EXPORT easygamechatTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT easygamechatTargets
  FILE easygamechatTargets.cmake
  NAMESPACE easygamechat::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easygamechat
)

include(CMakePackageConfigHelpers)

configure_package_config_file(
  cmake/easygamechatConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/easygamechatConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easygamechat
)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/easygamechatConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/easygamechatConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/easygamechatConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easygamechat
)

if(EASYGAMECHAT_BUILD_TESTS)
  enable_testing()

  add_executable(validation_test tests/validation_test.cpp)
  target_link_libraries(validation_test PRIVATE easygamechat::easygamechat)
  add_test(NAME validation COMMAND validation_test)

  add_executable(c_api_test tests/c_api_test.c)
  set_target_properties(c_api_test PROPERTIES LINKER_LANGUAGE CXX)
  target_link_libraries(c_api_test PRIVATE easygamechat::easygamechat)
  add_test(NAME c_api COMMAND c_api_test)
endif()

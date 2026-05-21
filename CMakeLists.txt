cmake_minimum_required(VERSION 3.21)
project(Chat LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
set(FETCHCONTENT_QUIET ON)
set(FETCHCONTENT_TRY_FIND_PACKAGE_MODE ALWAYS)

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

include(FetchContent)

find_package(asio QUIET)
if(NOT asio_FOUND)
  find_package(asio CONFIG QUIET)
  if(NOT asio_FOUND)
    find_package(asio PATHS build/_deps/asio-src NO_DEFAULT_PATH)
    if(NOT asio_FOUND)
      message(STATUS "ASIO not found via find_package, using FetchContent configuration")
      FetchContent_Declare(asio GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git GIT_TAG master)
      FetchContent_MakeAvailable(asio)
    endif()
  endif()
endif()

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
  message(STATUS "spdlog not found via find_package, using FetchContent configuration")
  FetchContent_Declare(spdlog GIT_REPOSITORY https://github.com/gabime/spdlog.git GIT_TAG v1.x)
  FetchContent_MakeAvailable(spdlog)
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
  message(STATUS "nlohmann_json not found via find_package, using FetchContent configuration")
  FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG develop)
  FetchContent_MakeAvailable(nlohmann_json)
endif()

add_executable(chat
    src/main.cpp
    src/Config.cpp
    src/Logger.cpp
    src/Ipc.cpp
    src/Serializer.cpp
    src/BinaryProtocol.cpp
    src/WebSocketSession.cpp
    src/WorkerClient.cpp
    src/WorkerMaster.cpp
)
target_include_directories(chat PRIVATE include ${asio_SOURCE_DIR}/include)
target_link_libraries(chat
    Threads::Threads
    SQLite::SQLite3
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
)
target_compile_options(chat PRIVATE -Wall -Wextra -pedantic)
target_link_options(chat PRIVATE -pthread)

cmake_minimum_required (VERSION 3.16)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
find_package(MySQL)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(BOT_NAME "beholder")

project(${BOT_NAME})
aux_source_directory("src" coresrc)
aux_source_directory("src/commands" coresrc)
aux_source_directory("src/3rdparty" coresrc)
aux_source_directory("src/proc" coresrc)
add_executable(${BOT_NAME} ${coresrc})


find_package(CxxUrl REQUIRED)

target_precompile_headers(${BOT_NAME} INTERFACE "src/3rdparty/httplib.h")

aux_source_directory("tessd" tessdsrc)
add_executable("tessd" ${tessdsrc} src/config.cpp src/sentry.cpp)

string(ASCII 27 Esc)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set_target_properties(${BOT_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
find_package(DPP REQUIRED)

if(APPLE)
	set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
	find_package(OpenSSL REQUIRED)
else()
	find_package(OpenSSL REQUIRED)
endif()

set(CMAKE_CXX_FLAGS "-g -O0")
target_compile_definitions(${BOT_NAME} PUBLIC DPP_CORO=ON CPPHTTPLIB_OPENSSL_SUPPORT)

target_include_directories(${BOT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${DPP_INCLUDE_DIR}
)

target_include_directories("tessd" PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries("tessd"
    tesseract
    leptonica
    sentry
    fmt
    ${CMAKE_THREAD_LIBS_INIT}
)


target_link_libraries(${BOT_NAME}
    fmt
    spdlog
    mysqlclient
    crypto
    ssl
    sentry
    CxxUrl
    ${CMAKE_THREAD_LIBS_INIT}
    ${DPP_LIBRARIES}
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
   WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
   COMMAND "../sentry-symbols.sh"
)

cmake_minimum_required (VERSION 3.16)

find_program(MOLD_LINKER mold)

if(MOLD_LINKER)
	message(STATUS "Using mold linker")
	add_link_options(-fuse-ld=mold)
else()
	message(STATUS "mold not found, using default linker")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
find_package(MySQL)

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
set(BOT_NAME "beholder")

project(${BOT_NAME})
aux_source_directory("src" coresrc)
aux_source_directory("src/commands" coresrc)
add_executable(${BOT_NAME} ${coresrc})

find_package(CxxUrl REQUIRED)
find_library(FMT_LIBRARY NAMES fmt REQUIRED)
find_library(TENSORFLOW_LIBRARY NAMES tensorflow REQUIRED)
find_package(OpenCV REQUIRED COMPONENTS core imgproc img_hash)
find_package(spdlog REQUIRED)

link_directories(/usr/local/lib)

aux_source_directory("tessd" tessdsrc)
add_executable("tessd"
	${tessdsrc}
	tessd/sha256.cpp
	src/wildcard.cpp
	tessd/3rdparty/httplib.cpp
	tessd/proc/cpipe.cpp
	tessd/proc/spawn.cpp
)

target_precompile_headers("tessd" INTERFACE "tessd/3rdparty/httplib.h")

target_compile_options(tessd PRIVATE -Wno-deprecated-enum-enum-conversion -Wno-missing-field-initializers)

aux_source_directory("nsfwd" nsfwdsrc)
add_executable("nsfwd"
	${nsfwdsrc}
	tessd/proc/cpipe.cpp
	tessd/proc/spawn.cpp
	src/logger.cpp
)

string(ASCII 27 Esc)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set_target_properties(${BOT_NAME} PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
)
set_target_properties("tessd" PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
)
set_target_properties("nsfwd" PROPERTIES
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

option(PRODUCTION_BUILD "Optimised production build" OFF)

if (PRODUCTION_BUILD)
	add_compile_options(-O2)
else()
	add_compile_options(-O0)
endif()

set(CMAKE_CXX_FLAGS "-g -Wall -Wno-unused-private-field -Wno-psabi -Wempty-body -Wignored-qualifiers -Wimplicit-fallthrough -Wmissing-field-initializers -Wsign-compare -Wtype-limits -Wuninitialized -Wshift-negative-value")
target_compile_definitions(${BOT_NAME} PUBLIC DPP_CORO=ON SPDLOG_FMT_EXTERNAL CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions("tessd" PUBLIC DPP_CORO=ON SPDLOG_FMT_EXTERNAL CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions("nsfwd" PUBLIC DPP_CORO=ON SPDLOG_FMT_EXTERNAL CPPHTTPLIB_OPENSSL_SUPPORT)

target_include_directories(${BOT_NAME} PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${DPP_INCLUDE_DIR}
)

target_include_directories("tessd" PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${DPP_INCLUDE_DIR}
)

target_include_directories("tessd" SYSTEM PRIVATE
	${OpenCV_INCLUDE_DIRS}
)

find_package(PkgConfig REQUIRED)

pkg_check_modules(WEBP REQUIRED IMPORTED_TARGET libwebp libwebpdemux)

pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libavutil libswscale)

target_include_directories("nsfwd" PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	/usr/include/jsoncpp
	${DPP_INCLUDE_DIR}
)


target_link_libraries("tessd"
	tesseract
	leptonica
	ssl
	crypto
	${FMT_LIBRARY}	
	CxxUrl
	${OpenCV_LIBS}
	PkgConfig::WEBP
	PkgConfig::FFMPEG
	avif
	${DPP_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries("nsfwd"
	${TENSORFLOW_LIBRARY}
	drogon
	jsoncpp
	trantor
	z
	uuid
	ssl
	crypto
	${FMT_LIBRARY}
	spdlog::spdlog
	${DPP_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)

target_link_libraries(${BOT_NAME}
	${FMT_LIBRARY}
	spdlog::spdlog
	mysqlclient
	crypto
	ssl
	CxxUrl
	tre
	${CMAKE_THREAD_LIBS_INIT}
	${DPP_LIBRARIES}
)

cmake_minimum_required(VERSION 3.5 FATAL_ERROR)

project(VirtualMedia CXX)

cmake_policy(SET CMP0054 NEW)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fno-rtti")
# Silence sdbusplus warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-parameter")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Os -flto")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0")

option(YOCTO_DEPENDENCIES "Use YOCTO dependencies system" OFF)

option(VM_USE_VALGRIND "Build VirtualMedia to work with valgrind" OFF)

option(VM_VERBOSE_NBDKIT_LOGS "Include detailed logs from nbdkit" OFF)

option(LEGACY_MODE_ENABLED
       "Enable 'legacy mode' - mounting remote shares using nbdkit" OFF)

if(NOT ${YOCTO_DEPENDENCIES})
  include(ExternalProject)

  ExternalProject_Add(
    Boost
    URL "https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.bz2"
    URL_MD5 "33334dd7f862e8ac9fe1cc7c6584fb6d"
    SOURCE_DIR ${CMAKE_BINARY_DIR}/src/boost
    BINARY_DIR ${CMAKE_BINARY_DIR}/libs/boost
    CONFIGURE_COMMAND cd <SOURCE_DIR> && ./bootstrap.sh
    BUILD_COMMAND cd <SOURCE_DIR> && ./b2 --exec-prefix=<BINARY_DIR>
                  --prefix=<BINARY_DIR> --with-system --with-coroutine install
    INSTALL_COMMAND ""
    UPDATE_COMMAND "")

  set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR}/libs/boost/ ${CMAKE_PREFIX_PATH})
  include_directories(SYSTEM ${CMAKE_BINARY_DIR}/libs/boost/include)
  link_directories(SYSTEM ${CMAKE_BINARY_DIR}/libs/boost/lib)

  ExternalProject_Add(
    sdbusplus-project
    PREFIX ${CMAKE_BINARY_DIR}/sdbusplus-project
    GIT_REPOSITORY https://github.com/openbmc/sdbusplus.git
    GIT_TAG afe80cf2e5dc4aefe3b041adeb0230e61929bf12
    SOURCE_DIR ${CMAKE_BINARY_DIR}/sdbusplus-src
    BINARY_DIR ${CMAKE_BINARY_DIR}/sdbusplus-build
    CONFIGURE_COMMAND ""
    BUILD_COMMAND cd ${CMAKE_BINARY_DIR}/sdbusplus-src && ./bootstrap.sh &&
                  ./configure --enable-transaction && make -j libsdbusplus.la
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
    UPDATE_COMMAND "")

  include_directories(${CMAKE_BINARY_DIR}/sdbusplus-src)
  link_directories(${CMAKE_BINARY_DIR}/sdbusplus-src/.libs)

  ExternalProject_Add(
    nlohmann-json
    GIT_REPOSITORY "https://github.com/nlohmann/json.git"
    GIT_TAG aafad2be1f3cd259a1e79d2f6fcf267d1ede9ec7
    SOURCE_DIR "${CMAKE_BINARY_DIR}/nlohmann-json-src"
    BINARY_DIR "${CMAKE_BINARY_DIR}/nlohmann-json-build"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND
      mkdir -p "${CMAKE_BINARY_DIR}/prefix/include/nlohmann" && cp -r
      "${CMAKE_BINARY_DIR}/nlohmann-json-src/include/nlohmann"
      "${CMAKE_BINARY_DIR}/prefix/include"
    UPDATE_COMMAND "")

  include_directories(${CMAKE_BINARY_DIR}/prefix/include)

else()
  # Include Boost library. This allows specify exact version of BOOST to be
  # used, especially important while using valgrind, to point BOOST that is
  # compiled with valgrind support
  if(${BOOST_VERSION})
    find_package(Boost ${BOOST_VERSION} EXACT)
  else()
    find_package(Boost 1.69 REQUIRED COMPONENTS coroutine context)
  endif()
  message("++ Using Boost version: " ${Boost_VERSION})

  include_directories(${Boost_INCLUDE_DIRS})
  link_directories(${Boost_LIBRARY_DIRS})
endif()

# Include UDEV library
find_package(udev REQUIRED)
include_directories(${UDEV_INCLUDE_DIRS})
link_directories(${UDEV_LIBRARIES})

# Boost related definitions
add_definitions(-DBOOST_COROUTINES_NO_DEPRECATION_WARNING)
add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)
add_definitions(-DBOOST_SYSTEM_NO_DEPRECATED)
add_definitions(-DBOOST_ALL_NO_LIB)
add_definitions(-DBOOST_NO_RTTI)
add_definitions(-DBOOST_NO_TYPEID)
add_definitions(-DBOOST_ASIO_DISABLE_THREADS)

# Define source files
include_directories(src)
set(SRC_FILES src/main.cpp)

# Executables
add_executable(virtual-media ${SRC_FILES} ${HEADER_FILES})
if(NOT ${YOCTO_DEPENDENCIES})
  add_dependencies(virtual-media nlohmann-json sdbusplus-project)
endif()

# Default linkage
target_link_libraries(virtual-media systemd)
target_link_libraries(virtual-media -lsdbusplus)
target_link_libraries(virtual-media -ludev)
target_link_libraries(virtual-media -lboost_coroutine)
target_link_libraries(virtual-media -lboost_context)
install(TARGETS virtual-media DESTINATION sbin)

# Options based compile definitions
target_compile_definitions(
  virtual-media
  PRIVATE $<$<BOOL:${VM_USE_VALGRIND}>:
          -DBOOST_USE_VALGRIND>
          $<$<BOOL:${VM_VERBOSE_NBDKIT_LOGS}>:
          -DVM_VERBOSE_NBDKIT_LOGS>
          $<$<BOOL:${LEGACY_MODE_ENABLED}>:
          -DLEGACY_MODE_ENABLED>
          $<$<BOOL:${CUSTOM_DBUS_PATH}>:
          -DCUSTOM_DBUS_PATH="${CUSTOM_DBUS_PATH}">)

if(CMAKE_INSTALL_SYSCONFDIR)
  install(FILES ${PROJECT_SOURCE_DIR}/virtual-media.json
          DESTINATION ${CMAKE_INSTALL_SYSCONFDIR})
endif()
install(FILES ${PROJECT_SOURCE_DIR}/xyz.openbmc_project.VirtualMedia.service
        DESTINATION /lib/systemd/system/)
install(FILES ${PROJECT_SOURCE_DIR}/tmpfiles.d/virtual-media.conf
        DESTINATION /etc/tmpfiles.d/)

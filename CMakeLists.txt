cmake_minimum_required(VERSION 2.8)
project(vncmanager)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")

include(GNUInstallDirs)

if(NOT DEFINED SYSCONF_INSTALL_DIR)
    set(SYSCONF_INSTALL_DIR ${CMAKE_INSTALL_FULL_SYSCONFDIR})
endif(NOT DEFINED SYSCONF_INSTALL_DIR)

find_package(PkgConfig REQUIRED)

pkg_check_modules(SYSTEMD "systemd")

add_definitions(-std=c++11)

add_executable(
  vncmanager

  main.cpp
  Configuration.cpp
  ControllerConnection.cpp
  ControllerManager.cpp
  FdStream.cpp
  GreeterConnection.cpp
  GreeterManager.cpp
  Log.cpp
  ReadSelector.cpp
  Server.cpp
  Stream.cpp
  StreamFormatter.cpp
  TLSStream.cpp
  VncTunnel.cpp
  Xvnc.cpp
  XvncConnection.cpp
  XvncManager.cpp
)

find_package(Threads REQUIRED)

find_package(Boost COMPONENTS iostreams program_options REQUIRED)

find_package(GnuTLS REQUIRED)

target_link_libraries(vncmanager ${CMAKE_THREAD_LIBS_INIT} ${Boost_LIBRARIES} ${GNUTLS_LIBRARIES})

install(TARGETS vncmanager RUNTIME DESTINATION bin)

if(SYSTEMD_FOUND)
    if (NOT DEFINED SYSTEMD_SYSTEM_UNIT_DIR)
        execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR)
        string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SYSTEM_UNIT_DIR ${SYSTEMD_SYSTEM_UNIT_DIR})
    endif()

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/vncmanager.service.in" "${CMAKE_CURRENT_BINARY_DIR}/vncmanager.service")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/vncmanager.service" DESTINATION "${SYSTEMD_SYSTEM_UNIT_DIR}")
endif()

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/vncmanager.conf" DESTINATION "${SYSCONF_INSTALL_DIR}/vnc")

# Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/communicator
# contact@m2osw.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

AtomicNames(names.an)

##
## communicator library
##
project(communicator)

# Put the version in the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

add_library(${PROJECT_NAME} SHARED
    # This first list is considered to be part of the library
    #
    communicator_connection.cpp
    flags.cpp
    loadavg.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/names.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/names.h
    version.cpp

    # The following are parts of the daemon but it has to be in a library
    # so the daemon can have plugins that access those objects (links
    # against them)
    #
    daemon/cache.cpp
    daemon/remote_communicators.cpp
    daemon/communicatord.cpp
    daemon/utils.cpp

    # system
    daemon/interrupt.cpp

    # listeners (a.k.a. servers)
    daemon/listener.cpp
    daemon/ping.cpp                # Ping is a UDP listener
    daemon/unix_listener.cpp

    # connections (steam-based pipes that send/receive messages)
    daemon/base_connection.cpp
    daemon/gossip_connection.cpp
    daemon/remote_connection.cpp
    daemon/service_connection.cpp
    daemon/unix_connection.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CPPPROCESS_INCLUDE_DIRS}
        ${EDHTTP_INCLUDE_DIRS}
        ${EVENTDISPATCHER_INCLUDE_DIRS}
        ${LIBADDR_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${SERVERPLUGINS_INCLUDE_DIRS}
        ${SNAPLOGGER_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    ${CPPPROCESS_LIBRARIES}
    ${EDHTTP_LIBRARIES}
    ${EVENTDISTPACHER_LIBRARIES}
    ${LIBADDR_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    ${SERVERPLUGINS_LIBRARIES}
    ${SNAPLOGGER_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION
        ${COMMUNICATOR_VERSION_MAJOR}.${COMMUNICATOR_VERSION_MINOR}

    SOVERSION
        ${COMMUNICATOR_VERSION_MAJOR}
)

install(
    TARGETS
        ${PROJECT_NAME}

    RUNTIME DESTINATION
        bin

    LIBRARY DESTINATION
        lib

    ARCHIVE DESTINATION
        lib
)

install(
    FILES
        communicator_connection.h
        exception.h
        flags.h
        loadavg.h
        ${CMAKE_CURRENT_BINARY_DIR}/names.h
        ${CMAKE_CURRENT_BINARY_DIR}/version.h

    DESTINATION
        include/communicator
)

# For plugins and also the daemon/main.cpp
install(
    FILES
        daemon/base_connection.h
        daemon/cache.h
        daemon/communicatord.h
        daemon/remote_connection.h
        daemon/service_connection.h
        daemon/unix_connection.h
        daemon/utils.h

    DESTINATION
        include/communicator/plugins
)

# vim: ts=4 sw=4 et

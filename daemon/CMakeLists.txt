# Copyright (c) 2011-2023  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/communicatord
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


# We create a shared library used by the unit tests to verify most every
# algorithm so we keep the list of files in a variable in order to be
# able to include the same files in the daemon and the test library
#
set(COMMUNICATORD_SOURCE_FILES
    cache.cpp
    remote_communicators.cpp
    server.cpp
    utils.cpp

    # eventdispatcher connections

        # system
        interrupt.cpp
        load_timer.cpp
        stable_clock.cpp

        # listeners (a.k.a. servers)
        listener.cpp
        ping.cpp                # Ping is a UDP listener
        unix_listener.cpp

        # connections (steam-based pipes that send/receive messages)
        base_connection.cpp
        gossip_connection.cpp
        remote_connection.cpp
        service_connection.cpp
        unix_connection.cpp

        # we also have a logrotate UDP listener which specifically handles
        # LOG messages in regard to the snaplogger
)


##
## communicatord
##
project(communicatordaemon)

add_executable(${PROJECT_NAME}
    main.cpp
    ${COMMUNICATORD_SOURCE_FILES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${ADVGETOPT_INCLUDE_DIRS}
        ${CPPTHREAD_INCLUDE_DIRS}
        ${EDHTTP_INCLUDE_DIRS}
        ${EVENTDISPATCHER_INCLUDE_DIRS}
        ${LIBADDR_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${SNAPLOGGER_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    communicatord
    ${ADVGETOPT_LIBRARIES}
    ${CPPTHREAD_LIBRARIES}
    ${EDHTTP_LIBRARIES}
    ${EVENTDISPATCHER_LIBRARIES}
    ${LIBADDR_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    ${SNAPLOGGER_LIBRARIES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "communicatord")

install(
    TARGETS
        ${PROJECT_NAME}

    RUNTIME DESTINATION
        sbin
)


##
## testcommunicatordaemon
##
#
# This is a "library-like" version (i.e. no main() function) used by the tests
#
project(testcommunicatordaemon)

add_library(${PROJECT_NAME}
    ${COMMUNICATORD_SOURCE_FILES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${ADVGETOPT_INCLUDE_DIRS}
        ${EDHTTP_INCLUDE_DIRS}
        ${EVENTDISPATCHER_INCLUDE_DIRS}
        ${LIBADDR_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${SNAPLOGGER_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    communicatord
    ${ADVGETOPT_LIBRARIES}
    ${EDHTTP_LIBRARIES}
    ${EVENTDISPATCHER_LIBRARIES}
    ${LIBADDR_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    ${SNAPLOGGER_LIBRARIES}
)

# NOT INSTALLED -- THIS IS JUST FOR OUR UNIT TESTS


# vim: ts=4 sw=4 et

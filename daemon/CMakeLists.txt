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


##
## communicatord
##
project(communicatord)

add_executable(${PROJECT_NAME}
    main.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${ADVGETOPT_INCLUDE_DIRS}
        ${EVENTDISPATCHER_INCLUDE_DIRS}
        ${SNAPDEV_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${SERVERPLUGINS_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    communicator
    #${ADVGETOPT_LIBRARIES}
    #${CPPTHREAD_LIBRARIES}
    #${EDHTTP_LIBRARIES}
    ${EVENTDISPATCHER_LIBRARIES}
    #${LIBADDR_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    #${SERVERPLUGINS_LIBRARIES}
    #${SNAPLOGGER_LIBRARIES}
)

install(
    TARGETS
        ${PROJECT_NAME}

    RUNTIME DESTINATION
        sbin
)

# for plugins, we also need some of the headers
#install(
#    FILES
#        base_connection.h
#        cache.h
#        communicatord.h
#        remote_connection.h
#        service_connection.h
#        unix_connection.h
#        utils.h
#
#    DESTINATION
#        include/communicator/plugins
#)


##
## testcommunicatordaemon
##
#
# This is a "library-like" version (i.e. no main() function) used by the tests
#
# -- this is now in the libcommunicator.so file!!! because we need it there
#    to be accessible by the plugins so no need although maybe at some point
#    we want to instead consider creating a libcommunicatord.so ...
#
#project(testcommunicatordaemon)
#
#add_library(${PROJECT_NAME}
#    ${COMMUNICATORD_SOURCE_FILES}
#)
#
#target_include_directories(${PROJECT_NAME}
#    PUBLIC
#        ${ADVGETOPT_INCLUDE_DIRS}
#        ${EDHTTP_INCLUDE_DIRS}
#        ${EVENTDISPATCHER_INCLUDE_DIRS}
#        ${LIBADDR_INCLUDE_DIRS}
#        ${LIBEXCEPT_INCLUDE_DIRS}
#        ${SNAPLOGGER_INCLUDE_DIRS}
#)
#
#target_link_libraries(${PROJECT_NAME}
#    communicator
#    ${ADVGETOPT_LIBRARIES}
#    ${EDHTTP_LIBRARIES}
#    ${EVENTDISPATCHER_LIBRARIES}
#    ${LIBADDR_LIBRARIES}
#    ${LIBEXCEPT_LIBRARIES}
#    ${SNAPLOGGER_LIBRARIES}
#)

# NOT INSTALLED -- THIS IS JUST FOR OUR UNIT TESTS


add_subdirectory(message-definitions)


# vim: ts=4 sw=4 et

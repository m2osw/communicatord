# Copyright (c) 2012-2022  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/snapcommunicator
# contact@m2osw.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

##
## snapcommunicator library
##
project(snapcommunicator)

# Put the version in the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

add_library(${PROJECT_NAME} SHARED
    snapcommunicator.cpp
    version.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${EVENTDISPATCHER_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${SNAPLOGGER_INCLUDE_DIRS}
        ${LIBADDR_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    ${EVENTDISTPACHER_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    ${SNAPLOGGER_LIBRARIES}
    ${LIBADDR_LIBRARIES}
)


set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION
        ${SNAPCOMMUNICATOR_VERSION_MAJOR}.${SNAPCOMMUNICATOR_VERSION_MINOR}

    SOVERSION
        ${SNAPCOMMUNICATOR_VERSION_MAJOR}
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
        snapcommunicator.h
        ${CMAKE_CURRENT_BINARY_DIR}/version.h

    DESTINATION
        include/snapcommunicator
)


# vim: ts=4 sw=4 et

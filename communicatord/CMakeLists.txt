# Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
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

##
## communicatord library
##
project(communicatord)

# Put the version in the header file
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/version.h
)

add_library(${PROJECT_NAME} SHARED
    loadavg.cpp
    communicatord.cpp
    version.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${EDHTTP_INCLUDE_DIRS}
        ${EVENTDISPATCHER_INCLUDE_DIRS}
        ${LIBEXCEPT_INCLUDE_DIRS}
        ${SERVERPLUGINS_INCLUDE_DIRS}
        ${SNAPLOGGER_INCLUDE_DIRS}
        ${LIBADDR_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME}
    ${EDHTTP_LIBRARIES}
    ${EVENTDISTPACHER_LIBRARIES}
    ${LIBEXCEPT_LIBRARIES}
    ${SNAPLOGGER_LIBRARIES}
    ${LIBADDR_LIBRARIES}
)


set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION
        ${COMMUNICATORD_VERSION_MAJOR}.${COMMUNICATORD_VERSION_MINOR}

    SOVERSION
        ${COMMUNICATORD_VERSION_MAJOR}
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
        communicatord.h
        ${CMAKE_CURRENT_BINARY_DIR}/version.h

    DESTINATION
        include/communicatord
)


# vim: ts=4 sw=4 et

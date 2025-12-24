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
## communicator unittest
##
find_package(SnapCatch2)
find_package(Reporter)

if(SnapCatch2_FOUND AND Reporter_FOUND)

    project(unittest)

    add_executable(${PROJECT_NAME}
        catch_main.cpp

        catch_base_connection.cpp
        catch_communicator.cpp
        catch_version.cpp
    )

    target_include_directories(${PROJECT_NAME}
        PUBLIC
            ${CMAKE_BINARY_DIR}
            ${PROJECT_SOURCE_DIR}

            ${ADVGETOPT_INCLUDE_DIRS}
            ${LIBADDR_INCLUDE_DIRS}
            ${LIBEXCEPT_INCLUDE_DIRS}
            ${REPORTER_SOURCE_DIR}
            ${SNAPCATCH2_INCLUDE_DIRS}
            ${SNAPDEV_INCLUDE_DIRS}
    )

    target_link_libraries(${PROJECT_NAME}
        testcommunicatordaemon
        ${ADVGETOPT_LIBRARIES}
        ${LIBADDR_LIBRARIES}
        ${LIBEXCEPT_LIBRARIES}
        ${REPORTER_LIBRARIES}
        ${SNAPCATCH2_LIBRARIES}
    )

else()

    message("snapcatch2 not found... no test will be built.")

endif()

if(SnapCatch2_FOUND)

    find_package(SnapTestRunner)
    AddUnitTestsTarget(
        PROJECT_NAME
            rununittests
    )

endif(SnapCatch2_FOUND)

# vim: ts=4 sw=4 et

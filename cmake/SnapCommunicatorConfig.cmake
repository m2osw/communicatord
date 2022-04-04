# - Try to find Sitter
#
# Once done this will define
#
# SNAPCOMMUNICATOR_FOUND        - System has Sitter
# SNAPCOMMUNICATOR_INCLUDE_DIRS - The Sitter include directories
# SNAPCOMMUNICATOR_LIBRARIES    - The libraries needed to use Sitter
# SNAPCOMMUNICATOR_DEFINITIONS  - Compiler switches required for using Sitter
#
# License:
#
# Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
#
# https://snapwebsites.org/project/snapcommunicator
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

find_path(
    SNAPCOMMUNICATOR_INCLUDE_DIR
        snapcommunicator/version.h

    PATHS
        $ENV{SNAPCOMMUNICATOR_INCLUDE_DIR}
)

find_library(
    SNAPCOMMUNICATOR_LIBRARY
        snapcommunicator

    PATHS
        $ENV{SNAPCOMMUNICATOR_LIBRARY}
)

mark_as_advanced(
    SNAPCOMMUNICATOR_INCLUDE_DIR
    SNAPCOMMUNICATOR_LIBRARY
)

set(SNAPCOMMUNICATOR_INCLUDE_DIRS ${SNAPCOMMUNICATOR_INCLUDE_DIR})
set(SNAPCOMMUNICATOR_LIBRARIES    ${SNAPCOMMUNICATOR_LIBRARY})

include(FindPackageHandleStandardArgs)

# handle the QUIETLY and REQUIRED arguments and set SNAPCOMMUNICATOR_FOUND to
# TRUE if all listed variables are TRUE
find_package_handle_standard_args(
    SnapCommunicator
    DEFAULT_MSG
    SNAPCOMMUNICATOR_INCLUDE_DIR
    SNAPCOMMUNICATOR_LIBRARY
)

# vim: ts=4 sw=4 et

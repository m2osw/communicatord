# - Find Communicator Daemon
#
# COMMUNICATORD_FOUND                - System has Communicator Daemon library
# COMMUNICATORD_INCLUDE_DIRS         - The Communicator Daemon library include directories
# COMMUNICATORD_LIBRARIES            - The libraries needed to use the Communicator Daemon library
# COMMUNICATORD_DEFINITIONS          - Compiler switches required for using Communicator Daemon library
# COMMUNICATORD_SERVICES_INSTALL_DIR - Directory where to install Communicator Daemon .service files
#
# License:
#
# Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
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

find_path(
    COMMUNICATORD_INCLUDE_DIR
        communicatord/version.h

    PATHS
        ENV COMMUNICATORD_INCLUDE_DIR
)

find_library(
    COMMUNICATORD_LIBRARY
        communicatord

    PATHS
        ${COMMUNICATORD_LIBRARY_DIR}
        ENV COMMUNICATORD_LIBRARY
)

mark_as_advanced(
    COMMUNICATORD_INCLUDE_DIR
    COMMUNICATORD_LIBRARY
)

set(COMMUNICATORD_INCLUDE_DIRS ${COMMUNICATORD_INCLUDE_DIR})
set(COMMUNICATORD_LIBRARIES    ${COMMUNICATORD_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Communicatord
    REQUIRED_VARS
        COMMUNICATORD_INCLUDE_DIR
        COMMUNICATORD_LIBRARY
)

set(COMMUNICATORD_SERVICES_INSTALL_DIR share/communicatord/services)

# vim: ts=4 sw=4 et

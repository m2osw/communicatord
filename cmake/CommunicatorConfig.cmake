# - Find the Communicator project
#
# COMMUNICATOR_FOUND                - System has Communicator Daemon library
# COMMUNICATOR_INCLUDE_DIRS         - The Communicator Daemon library include directories
# COMMUNICATOR_LIBRARIES            - The libraries needed to use the Communicator Daemon library
# COMMUNICATOR_DEFINITIONS          - Compiler switches required for using Communicator Daemon library
# COMMUNICATOR_SERVICES_INSTALL_DIR - Directory where to install Communicator Daemon .service files
#
# License:
#
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

find_path(
    COMMUNICATOR_INCLUDE_DIR
        communicator/version.h

    PATHS
        ENV COMMUNICATOR_INCLUDE_DIR
)

find_library(
    COMMUNICATOR_LIBRARY
        communicator

    PATHS
        ${COMMUNICATOR_LIBRARY_DIR}
        ENV COMMUNICATOR_LIBRARY
)

mark_as_advanced(
    COMMUNICATOR_INCLUDE_DIR
    COMMUNICATOR_LIBRARY
)

set(COMMUNICATOR_INCLUDE_DIRS ${COMMUNICATOR_INCLUDE_DIR})
set(COMMUNICATOR_LIBRARIES    ${COMMUNICATOR_LIBRARY})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    Communicator
    REQUIRED_VARS
        COMMUNICATOR_INCLUDE_DIR
        COMMUNICATOR_LIBRARY
)

set(COMMUNICATOR_SERVICES_INSTALL_DIR share/communicator/services)
set(COMMUNICATOR_PLUGINS_INSTALL_DIR lib/communicator/plugins)

# vim: ts=4 sw=4 et

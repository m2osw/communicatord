// Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/communicatord
// contact@m2osw.com
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
#pragma once

/** \file
 * \brief Various utilities.
 *
 * Useful types and functions for the Communicator.
 */

// advgetopt
//
#include    <advgetopt/utils.h>


// libaddr
//
#include    <libaddr/addr.h>


// C++
//
#include    <set>
#include    <string>



namespace communicator_daemon
{



advgetopt::string_set_t     canonicalize_services(std::string const & services);
std::string                 canonicalize_server_types(std::string const & server_types);
std::string                 canonicalize_neighbors(std::string const & neighbors);



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

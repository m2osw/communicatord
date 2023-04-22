// Copyright (c) 2011-2023  Made to Order Software Corp.  All Rights Reserved
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

/** \file
 * \brief Verify some of the communicatord client implementation.
 *
 * This test file implements verifications of the communicatord client
 * class.
 */

// communicatord
//
#include    <communicatord/communicatord.h>


// self
//
#include    "catch_main.h"





CATCH_TEST_CASE("communicatord", "[client]")
{
    CATCH_START_SECTION("communicatord: verify default strings")
    {
        std::stringstream port_str;
        port_str << communicatord::LOCAL_PORT;
        CATCH_REQUIRE(port_str.str() == std::string(communicatord::g_communicatord_default_port));

        std::stringstream ip_port;
        ip_port << communicatord::g_communicatord_default_ip << ":" << communicatord::LOCAL_PORT;
        CATCH_REQUIRE(ip_port.str() == std::string(communicatord::g_communicatord_default_ip_port));
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et

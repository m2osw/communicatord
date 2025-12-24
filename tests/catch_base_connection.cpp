// Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/communicator
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
 * \brief Verify the base_connection class.
 *
 * This file implements tests to verify that the base_connection
 * class functions as expected.
 */

// self
//
#include    "catch_main.h"


// communicator daemon
//
#include    <daemon/base_connection.h>



class test_connection
    : public communicator_daemon::base_connection
{
public:
    test_connection(communicator_daemon::communicatord * s)
        : base_connection(s, false)
    {
    }

    virtual int get_socket() const override
    {
        return -1;
    }
};


CATCH_TEST_CASE("base_connection", "[connection]")
{
    CATCH_START_SECTION("base_connection: verify default object")
    {
        communicator_daemon::communicatord * s(nullptr);
        test_connection tc(s);

        // verify defaults
        //
        CATCH_REQUIRE(tc.get_connection_started() == -1);
        CATCH_REQUIRE(tc.get_connection_ended() == -1);
        CATCH_REQUIRE(tc.get_server_name().empty());
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et

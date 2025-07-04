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

// self
//
#include    "catch_main.h"


// communicator
//
#include    <communicatord/version.h>


// last include
//
#include    <snapdev/poison.h>




CATCH_TEST_CASE("Version", "[version]")
{
    CATCH_START_SECTION("version: verify runtime vs compile time communicator version numbers")
    {
        CATCH_REQUIRE(communicatord::get_major_version()   == COMMUNICATORD_VERSION_MAJOR);
        CATCH_REQUIRE(communicatord::get_release_version() == COMMUNICATORD_VERSION_MINOR);
        CATCH_REQUIRE(communicatord::get_patch_version()   == COMMUNICATORD_VERSION_PATCH);
        CATCH_REQUIRE(strcmp(communicatord::get_version_string(), COMMUNICATORD_VERSION_STRING) == 0);
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et

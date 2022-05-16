// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
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


// advgetopt
//
#include    <advgetopt/advgetopt.h>


// eventdispatcher
//
#include    <eventdispatcher/connection.h>



namespace communicatord
{



constexpr int const     LOCAL_PORT = 4040;      // cd://<loopback-ip>
constexpr int const     UDP_PORT = 4041;        // cdu://<loopback-ip> (any IP is accepted at the moment, but it's expected to be local)
constexpr int const     REMOTE_PORT = 4042;     // cd://<private-ip>
constexpr int const     SECURE_PORT = 4043;     // cds://<public-ip>
constexpr char const *  g_communicatord_default_ip_port = "127.0.0.1:4040";



class communicator
{
public:
                        communicator(advgetopt::getopt & opts);

    void                add_communicator_options();
    void                process_communicator_options();

private:
    advgetopt::getopt & f_opts;
    ed::connection::pointer_t
                        f_communicator_connection = ed::connection::pointer_t();
};


} // namespace communicatord
// vim: ts=4 sw=4 et

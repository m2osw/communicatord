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
#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/connection.h>
#include    <eventdispatcher/message.h>


// snapdev
//
#include    <snapdev/join_strings.h>
#include    <snapdev/to_string_literal.h>


// C++
//
#include    <string_view>


namespace communicatord
{



constexpr int const         LOCAL_PORT = 4040;      // cd://<loopback-ip>
constexpr int const         UDP_PORT = 4041;        // cdu://<loopback-ip> (any IP is accepted at the moment, but it's expected to be local)
constexpr int const         REMOTE_PORT = 4042;     // cd://<private-ip>
constexpr int const         SECURE_PORT = 4043;     // cds://<public-ip>
constexpr std::string_view  g_communicatord_default_ip = "127.0.0.1";
constexpr std::string_view  g_communicatord_any_ip = "0.0.0.0";
constexpr std::string_view  g_communicatord_default_port = snapdev::integer_to_string_literal<LOCAL_PORT>.data();
constexpr std::string_view  g_communicatord_colon = ":";
constexpr std::string_view  g_communicatord_default_ip_port = snapdev::join_string_views<g_communicatord_default_ip, g_communicatord_colon, g_communicatord_default_port>;
constexpr std::string_view  g_communicatord_any_ip_port = snapdev::join_string_views<g_communicatord_any_ip, g_communicatord_colon, g_communicatord_default_port>;



class communicator
{
public:
                        communicator(advgetopt::getopt & opts);
    virtual             ~communicator();

    void                add_communicator_options();
    void                process_communicator_options();
    bool                send_message(ed::message & msg, bool cache = false);
    void                unregister_communicator(bool quitting);

private:
    advgetopt::getopt &         f_opts;
    ed::communicator::pointer_t f_communicator = ed::communicator::pointer_t();
    ed::connection::pointer_t   f_communicator_connection = ed::connection::pointer_t();
};


} // namespace communicatord
// vim: ts=4 sw=4 et

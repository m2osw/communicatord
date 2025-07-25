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

/** \file
 * \brief Implementation of the ping connection.
 *
 */

// self
//
#include    "ping.h"



namespace communicator_daemon
{



/** \class ping
 * \brief Handle UDP messages from clients.
 *
 * This class is a server connection listening for UDP messages.
 *
 * These messages cannot receive replies at the moment. If you need a
 * reply, make sure to use a TCP connection.
 */



/** \brief The messenger initialization.
 *
 * The messenger receives UDP messages from various sources (mainly
 * backends at this point).
 *
 * \param[in] cs  The communicator server we are listening for.
 * \param[in] address  The address and port to listen on. Most often it is
 *                     127.0.0.1 for the UDP because we currently only allow
 *                     for local messages.
 */
ping::ping(server::pointer_t cs, addr::addr const & address)
    : udp_server_message_connection(address)
    , base_connection(cs, true)
{
}


int ping::get_socket() const
{
    return udp_server_message_connection::get_socket();
}


void ping::process_message(ed::message & msg)
{
    //f_server->process_message(shared_from_this(), msg, true);
    base_connection::pointer_t conn(std::dynamic_pointer_cast<base_connection>(shared_from_this()));
    msg.user_data(conn);
    f_server->dispatch_message(msg);
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

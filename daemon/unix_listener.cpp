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
 * \brief Implementation of the listener.
 *
 * The listener connection is the one listening for connections from
 * local and remote services.
 */

// self
//
#include    "unix_listener.h"

#include    "unix_connection.h"


// eventdispatcher
//
#include    <eventdispatcher/tcp_bio_client.h>


// snaplogger
//
#include    <snaplogger/message.h>


// last include
//
#include    <snapdev/poison.h>







namespace communicator_daemon
{



/** \class unix_listener
 * \brief Handle new connections from clients.
 *
 * This class is an implementation of the server connection so we can
 * accept messages from various clients using a local Unix Stream-based
 * connection.
 */




/** \brief The listener initialization.
 *
 * The listener creates a new TCP server to listen for incoming
 * TCP connection.
 *
 * \warning
 * At this time the \p max_connections parameter is ignored.
 *
 * \param[in] address  The address:port to listen on. Most often it is
 * 0.0.0.0:4040 (plain connection) or 0.0.0.0:4041 (secure connection).
 * \param[in] certificate  The filename of a PEM file with a certificate.
 * \param[in] private_key  The filename of a PEM file with a private key.
 * \param[in] max_connections  The maximum number of connections to keep
 *                             waiting; if more arrive, refuse them until
 *                             we are done with some existing connections.
 * \param[in] local  Whether this connection expects local services only.
 * \param[in] server_name  The name of the server running this instance.
 * \param[in] secure  Whether to create a secure (true) or non-secure (false)
 *                    connection.
 */
unix_listener::unix_listener(
          server::pointer_t cs
        , addr::addr_unix const & address
        , int max_connections
        , std::string const & server_name)
    : local_stream_server_connection(address, max_connections, true, true)
    , f_server(cs)
    , f_server_name(server_name)
{
}


void unix_listener::process_accept()
{
    // a new client just connected, create a new service_connection
    // object and add it to the ed::communicator object.
    //
    snapdev::raii_fd_t new_client(std::move(accept()));
    if(new_client == nullptr)
    {
        // an error occurred, report in the logs
        int const e(errno);
        SNAP_LOG_ERROR
            << "somehow accept() failed with errno: "
            << e
            << " -- "
            << strerror(e)
            << SNAP_LOG_SEND;
        return;
    }

    unix_connection::pointer_t service(
            std::make_shared<unix_connection>(
                      f_server
                    , std::move(new_client)
                    , f_server_name));

    // set a default name in each new connection, this changes
    // whenever we receive a REGISTER message from that connection
    //
    service->set_name("client unix connection");

    service->set_server_name(f_server_name);

    if(!ed::communicator::instance()->add_connection(service))
    {
        // this should never happen here since each new creates a
        // new pointer
        //
        SNAP_LOG_ERROR
            << "new client connection could not be added to the ed::communicator list of connections."
            << SNAP_LOG_SEND;
    }
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

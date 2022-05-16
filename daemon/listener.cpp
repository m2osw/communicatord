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

/** \file
 * \brief Implementation of the listener.
 *
 * The listener connection is the one listening for connections from
 * local and remote services.
 */

// self
//
#include    "listener.h"

#include    "service_connection.h"


// snaplogger
//
#include <snaplogger/message.h>


// libaddr
//
#include <libaddr/addr.h>
#include <libaddr/addr_parser.h>


// included last
//
#include <snapdev/poison.h>



namespace communicator_daemon
{





/** \class listener
 * \brief Handle new connections from clients.
 *
 * This class is an implementation of the TCP server connection so we can
 * handle messages from various TCP clients.
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
listener::listener(
          server::pointer_t cs
        , addr::addr const & address
        , std::string const & certificate
        , std::string const & private_key
        , int max_connections
        , bool local
        , std::string const & server_name)
    : tcp_server_connection(
              address
            , certificate
            , private_key
            , (certificate.empty() || private_key.empty()
                ? ed::mode_t::MODE_PLAIN
                : ed::mode_t::MODE_SECURE)
            , max_connections
            , true)
    , f_server(cs)
    , f_local(local)
    , f_server_name(server_name)
{
}


void listener::process_accept()
{
    // a new client just connected, create a new service_connection
    // object and add it to the ed::communicator object.
    //
    ed::tcp_bio_client::pointer_t const new_client(accept());
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

    service_connection::pointer_t service(std::make_shared<service_connection>(
                  f_server
                , new_client
                , f_server_name));

    // TBD: is that a really weak test?
    //
    //QString const addr(service->get_remote_address());
    // the get_remote_address() function may return an IP and a port so
    // parse that to remove the port; also remote_addr() has a function
    // that tells us whether the IP is private, local, or public
    //
    addr::addr const remote_addr(service->get_remote_address());
    addr::addr::network_type_t const network_type(remote_addr.get_network_type());
    if(f_local)
    {
        if(network_type != addr::addr::network_type_t::NETWORK_TYPE_LOOPBACK)
        {
            // TODO: look into making this an ERROR() again and return, in
            //       effect viewing the error as a problem and refusing the
            //       connection (we had a problem with the IP detection
            //       which should be resolved now that we use the `addr`
            //       class
            //
            SNAP_LOG_WARNING
                << "received what should be a local connection from \""
                << service->get_remote_address().to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_PORT)
                << "\"."
                << SNAP_LOG_SEND;
            //return;
        }

        // set a default name in each new connection, this changes
        // whenever we receive a REGISTER message from that connection
        //
        service->set_name("client connection");

        service->set_server_name(f_server_name);
    }
    else
    {
        if(network_type == addr::addr::network_type_t::NETWORK_TYPE_LOOPBACK)
        {
            SNAP_LOG_ERROR
                << "received what should be a remote connection from \""
                << service->get_remote_address().to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_PORT)
                << "\"."
                << SNAP_LOG_SEND;
            return;
        }

        // set a name for remote connections
        //
        // the following name includes a space which prevents someone
        // from send to such a connection, which is certainly a good
        // thing since there can be duplicate and that name is not
        // sensible as a destination
        //
        // we will change the name once we receive the CONNECT message
        // and as we send the ACCEPT message
        //
        service->set_name(
                  std::string("remote connection from: ")
                + service->get_remote_address().to_ipv4or6_string(addr::addr::string_ip_t::STRING_IP_PORT));
        service->mark_as_remote();
    }

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

// Copyright (c) 2011-2024  Made to Order Software Corp.  All Rights Reserved
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


// communicatord
//
#include    <communicatord/names.h>


// snaplogger
//
#include    <snaplogger/message.h>


// libaddr
//
#include    <libaddr/addr.h>
#include    <libaddr/addr_parser.h>


// last include
//
#include    <snapdev/poison.h>



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
 * \param[in] cs  The communicator server pointer.
 * \param[in] address  The address:port to listen on. Most often it is
 * 0.0.0.0:4040 (plain connection) or 0.0.0.0:4041 (secure connection).
 * \param[in] certificate  The filename of a PEM file with a certificate.
 * \param[in] private_key  The filename of a PEM file with a private key.
 * \param[in] max_connections  The maximum number of connections to keep
 *                             waiting; if more arrive, refuse them until
 *                             we are done with some existing connections.
 * \param[in] local  Whether this connection expects local services only.
 * \param[in] server_name  The name of the server running this instance.
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
                : ed::mode_t::MODE_ALWAYS_SECURE)
            , max_connections
            , true)
    , f_server(cs)
    , f_local(local)
    , f_server_name(server_name)
{
    //set_name(...) -- this is done in the server.cpp because the listener
    //                 is used for many different type of listening
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
            << "somehow accept() of a tcp connection failed with errno: "
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

    service->set_username(f_username);
    service->set_password(f_password);

    // TBD: is that a really weak test?
    //
    addr::addr const remote_addr(service->get_remote_address());
    addr::network_type_t const network_type(remote_addr.get_network_type());
    if(f_local)
    {
        if(network_type != addr::network_type_t::NETWORK_TYPE_LOOPBACK)
        {
            // TODO: look into making this an ERROR() again and return, in
            //       effect viewing the error as a problem and refusing the
            //       connection (we had a problem with the IP detection
            //       which should be resolved now that we use the `addr`
            //       class)
            //
            SNAP_LOG_WARNING
                << "received what should be a local connection from \""
                << service->get_remote_address().to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT)
                << "\"."
                << SNAP_LOG_SEND;
            //return;
        }

        // set a default name in each new connection, this changes
        // whenever we receive a REGISTER message from that connection
        //
        service->set_name("client tcp connection");

        service->set_server_name(f_server_name);
    }
    else
    {
        if(network_type == addr::network_type_t::NETWORK_TYPE_LOOPBACK)
        {
            SNAP_LOG_ERROR
                << "received what should be a remote tcp connection from \""
                << service->get_remote_address().to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT)
                << "\"."
                << SNAP_LOG_SEND;
            return;
        }

        // set a name for remote connections
        //
        // the following name includes a colon and a space which prevents
        // someone from specifically sending messages to that connection,
        // which is a good thing since there can be multiple such connections
        // and that name is not sensible as a destination for a local service
        //
        // we set the server name from the CONNECT and ACCEPT messages
        // (but that does not affect the service name)
        //
        service->set_name(
                  std::string(communicatord::g_name_communicatord_connection_remote_communicator_in)
                + ": "
                + service->get_remote_address().to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
        service->mark_as_remote();
    }

    if(!ed::communicator::instance()->add_connection(service))
    {
        // this should never happen here since each new creates a
        // new pointer
        //
        SNAP_LOG_ERROR
            << "new client tcp connection could not be added to the ed::communicator list of connections."
            << SNAP_LOG_SEND;
    }
}


/** \brief Set the \p username required to connect on this TCP connection.
 *
 * When accepting connections from remote communicatord, it is best to
 * assign a user name and password to that connection. This protects
 * your connection from hackers without such credentials.
 *
 * \param[in] username  The name of the user that can connect to this listener.
 *
 * \sa set_password()
 * \sa get_username()
 */
void listener::set_username(std::string const & username)
{
    f_username = username;
}


/** \brief Retrieve the user name of this connection.
 *
 * This function returns the user name as set by the set_username().
 * Some connections may require a username and password to fully
 * work.
 *
 * \return A copy of the username.
 *
 * \sa set_username()
 */
std::string listener::get_username() const
{
    return f_username;
}


/** \brief Set the password to use to connect to a remote communicator daemon.
 *
 * Some remote connections require a login name and password to accept a
 * connection. This function can be used to define the password.
 *
 * The password will be sent unencrypted. You want to use an SSL connection
 * for such remote connections.
 *
 * \param[in] password  The passowrd to use to connect to a communicator daemon.
 *
 * \sa set_username()
 * \sa get_password()
 */
void listener::set_password(std::string const & password)
{
    f_password = password;
}


/** \brief Return the password assigned to this connection.
 *
 * Each listener may include a password to prevent unwanted connections
 * from hackers on public facing connections.
 *
 * \return A copy of the password.
 *
 * \sa set_password()
 */
std::string listener::get_password() const
{
    return f_password;
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

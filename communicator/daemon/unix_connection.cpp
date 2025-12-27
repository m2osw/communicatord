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
 * \brief Implementation of the Unix service connection.
 *
 * A local service can connect to the communicator daemon via this Unix
 * connection. This is practical since a Unix connection is safe from
 * remote attacks and also it is really fast and much less likely to
 * inadvertently be blocked by the firewall.
 */

// self
//
#include    "unix_connection.h"



// communicator
//
#include    <communicator/names.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{



/** \class unix_connection
 * \brief Listen for messages.
 *
 * The communicatord Unix connection simply listen for process_message()
 * callbacks and processes those messages by calling the process_message()
 * of the connection class.
 *
 * It also listens for disconnections so it can send a new STATUS command
 * whenever the connection goes down.
 */


/** \brief Create a service connection and assigns \p socket to it.
 *
 * The constructor of the service connection expects a socket that
 * was just accept()'ed.
 *
 * The communicatord daemon listens through a socket files created under
 * `/run/communicator/...`. By default, the socket is named
 * `communicatord.sock`. It uses that specific folder because it gets
 * created with the correct user and group ownership and that allows the
 * daemon to make use of that folder for writing.
 *
 * \param[in] s  The communicator server (i.e. parent).
 * \param[in] client  The socket that was just returned by the accept()
 *                    command.
 * \param[in] server_name  The name of the server we are running on
 *                         (i.e. generally your hostname).
 */
unix_connection::unix_connection(
          communicatord * s
        , snapdev::raii_fd_t client
        , std::string const & server_name)
    : local_stream_server_client_message_connection(std::move(client))
    , base_connection(s, false)
    , f_server_name(server_name)
{
}


/** \brief Connection lost.
 *
 * When a connection goes down it gets deleted. This is when we can
 * send a new STATUS event to all the other STATUS hungry connections.
 */
unix_connection::~unix_connection()
{
    // save when it is ending in case we did not get a DISCONNECT
    // or an UNREGISTER event
    //
    try
    {
        connection_ended();
    }
    catch(std::runtime_error const &)
    {
    }

    // clearly mark this connection as down
    //
    set_connection_type(connection_type_t::CONNECTION_TYPE_DOWN);

    // make sure that if we had a connection understanding STATUS
    // we do not send that status
    //
    remove_command(communicator::g_name_communicator_cmd_status);

    // now ask the server to send a new STATUS to all connections
    // that understand that message; we pass our pointer since we
    // want to send the info about this connection in that STATUS
    // message
    //
    // TODO: we cannot use shared_from_this() in the destructor,
    //       it's too late since when we reach here the pointer
    //       was already destroyed so we get a bad_weak_ptr
    //       exception; we need to find a different way if we
    //       want this event to be noticed and a STATUS sent...
    //
    //f_communicator_server->send_status(shared_from_this());
}


int unix_connection::get_socket() const
{
    return local_stream_server_client_message_connection::get_socket();
}


void unix_connection::process_message(ed::message & msg)
{
    // make sure the destination knows who sent that message so it
    // is possible to directly reply to that specific instance of
    // a service
    //
    if(f_named)
    {
        msg.set_sent_from_server(f_server_name);
        msg.set_sent_from_service(get_name());
    }
    //f_server->process_message(shared_from_this(), msg, false);
    base_connection::pointer_t conn(std::dynamic_pointer_cast<base_connection>(shared_from_this()));
    msg.user_data(conn);
    f_server->dispatch_message(msg);
}


/** \brief We are losing the connection, send a STATUS message.
 *
 * This function is called in all cases where the connection is
 * lost so we can send a STATUS message with information saying
 * that the connection is gone.
 */
void unix_connection::send_status()
{
    // mark connection as down before we call the send_status()
    //
    set_connection_type(connection_type_t::CONNECTION_TYPE_DOWN);

    f_server->send_status(shared_from_this());
}


/** \brief Remove ourselves when we receive a timeout.
 *
 * Whenever we receive a shutdown, we have to remove everything but
 * we still want to send some messages and to do so we need to use
 * the timeout, which happens after we finalize all read and write
 * callbacks.
 */
void unix_connection::process_timeout()
{
    remove_from_communicator();

    send_status();
}


void unix_connection::process_error()
{
    local_stream_server_client_message_connection::process_error();

    send_status();
}


/** \brief Process a hang up.
 *
 * It is important for some processes to know when a remote connection
 * is lost (i.e. for dynamic QUORUM calculations in snaplock, for
 * example). So we handle the process_hup() event and send a
 * HANGUP if this connection is a remote connection.
 */
void unix_connection::process_hup()
{
    local_stream_server_client_message_connection::process_hup();

    if(is_remote()
    && !get_server_name().empty())
    {
        // TODO: this is nice, but we would probably need such in the
        //       process_invalid(), process_error(), process_timeout()?
        //
        ed::message hangup;
        hangup.set_command(communicator::g_name_communicator_cmd_hangup);
        hangup.set_service(communicator::g_name_communicator_service_local_broadcast);
        hangup.add_parameter(communicator::g_name_communicator_param_server_name, get_server_name());
        f_server->broadcast_message(hangup);

        f_server->cluster_status(shared_from_this());
    }

    send_status();
}


void unix_connection::process_invalid()
{
    local_stream_server_client_message_connection::process_invalid();

    send_status();
}


/** \brief Tell that the connection was given a real name.
 *
 * Whenever we receive an event through this connection,
 * we want to mark the message as received from the service.
 *
 * However, by default the name of the service is on purpose
 * set to an "invalid value" (i.e. a name with a space.) That
 * value is not expected to be used when forwarding the message
 * to another service.
 *
 * Once a system properly registers with the REGISTER message,
 * we receive a valid name then. That name is saved in the
 * connection and the connection is marked as having a valid
 * name.
 *
 * This very function must be called once the proper name was
 * set in this connection.
 */
void unix_connection::properly_named()
{
    f_named = true;
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

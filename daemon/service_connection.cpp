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
 * \brief Implementation of the Communicator service connection.
 *
 * A service is a local daemon offering a service to our system. Such
 * as service connections to the communicatord daemon via the local
 * TCP connection and uses that connection to register itself and
 * then send messages to other services wherever they are in the network.
 */

// self
//
#include    "service_connection.h"



// included last
//
#include <snapdev/poison.h>



namespace communicator_daemon
{



namespace
{

struct hits_t
{
    int         f_count = 1;
    time_t      f_last_hit = time(nullptr);
};

std::map<addr::addr, hits_t>    g_blocked_ips = {};

} // no name namespace


/** \class service_connection
 * \brief Listen for messages.
 *
 * The communicatord TCP connection simply listen for process_message()
 * callbacks and processes those messages by calling the process_message()
 * of the connections class.
 *
 * It also listens for disconnections so it can send a new STATUS command
 * whenever the connection goes down.
 */


/** \brief Create a service connection and assigns \p socket to it.
 *
 * The constructor of the service connection expects a socket that
 * was just accept()'ed.
 *
 * The communicatord daemon listens on to two different ports
 * and two different addresses on those ports:
 *
 * \li TCP 127.0.0.1:4040 -- this address is expected to be used by all the
 * local services
 *
 * \li TCP 0.0.0.0:4040 -- this address is expected to be used by remote
 * communicators; it is often changed to a private network IP
 * address such as 192.168.0.1 to increase safety. However, if your
 * cluster spans multiple data centers, it will not be possible to
 * use a private network IP address.
 *
 * \li UDP 127.0.0.1:4041 -- this special port is used to accept UDP
 * signals sent to the communicatord; UDP signals are most often
 * used to very quickly send signals without having to have a full
 * TCP connection to a daemon
 *
 * The connections happen on 127.0.0.1 are fully trusted. Connections
 * happening on 0.0.0.0 are generally viewed as tainted.
 *
 * \param[in] cs  The communicator server (i.e. parent)
 * \param[in] client  The socket that was just created by the accept()
 *                    command.
 * \param[in] server_name  The name of the server we are running on
 *                         (i.e. generally your hostname.)
 */
service_connection::service_connection(
            server::pointer_t cs
          , ed::tcp_bio_client::pointer_t client
          , std::string const & server_name)
    : tcp_server_client_message_connection(client)
    , base_connection(cs, false)
    , f_server_name(server_name)
    , f_address(client->get_address())  // peer address:port (computer on the other side)
{
}


/** \brief Connection lost.
 *
 * When a connection goes down it gets deleted. This is when we can
 * send a new STATUS event to all the other STATUS hungry connections.
 */
service_connection::~service_connection()
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
    remove_command("STATUS");

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


void service_connection::process_message(ed::message & msg)
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


bool service_connection::send_message(ed::message & msg, bool cache)
{
    return tcp_server_client_message_connection::send_message(msg, cache);
}



/** \brief We are losing the connection, send a STATUS message.
 *
 * This function is called in all cases where the connection is
 * lost so we can send a STATUS message with information saying
 * that the connection is gone.
 */
void service_connection::send_status()
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
void service_connection::process_timeout()
{
    remove_from_communicator();

    send_status();
}


void service_connection::process_error()
{
    tcp_server_client_message_connection::process_error();

    send_status();
}


/** \brief Process a hang up.
 *
 * It is important for some processes to know when a remote connection
 * is lost (i.e. for dynamic QUORUM calculations in snaplock, for
 * example.) So we handle the process_hup() event and send a
 * HANGUP if this connection is a remote connection.
 */
void service_connection::process_hup()
{
    tcp_server_client_message_connection::process_hup();

    if(is_remote()
    && !get_server_name().empty())
    {
        // TODO: this is nice, but we would probably need such in the
        //       process_invalid(), process_error(), process_timeout()?
        //
        ed::message hangup;
        hangup.set_command("HANGUP");
        hangup.set_service(".");
        hangup.add_parameter("server_name", get_server_name());
        f_server->broadcast_message(hangup);

        f_server->cluster_status(shared_from_this());
    }

    send_status();
}


void service_connection::process_invalid()
{
    tcp_server_client_message_connection::process_invalid();

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
void service_connection::properly_named()
{
    f_named = true;
}


/** \brief Return the type of address this connection has.
 *
 * This function determines the type of address of the connection.
 *
 * \return A reference to the remote address of this connection.
 */
addr::addr const & service_connection::get_address() const
{
    return f_address;
}


/** \brief Mark the IP address of this connection as blocked.
 *
 * This function marks that IP address as blocked.
 *
 * The first 2 times, it just registers the IP. If the IP is still marked
 * as blocked the 3 time (i.e. it eventually times out), then it blocks
 * the IP completely by sending a message to the firewall.
 *
 * \todo
 * Move the elapsed time limit (15 min. for now) to the configuration.
 *
 * \todo
 * Move the limit of hits (3 for now) to the configuration.
 *
 * \todo
 * Added a timeout timer so we can check old entries and remove them from
 * memory. Otherwise we could end up filling up memory (albeit after quite
 * a long time, but that's a potential issue).
 */
void service_connection::block_ip()
{
    addr::addr a(get_remote_address());
    a.set_port(0); // ignore the port in this case
    auto it(g_blocked_ips.find(a));
    int r(1);
    if(it == g_blocked_ips.end())
    {
        g_blocked_ips[a] = hits_t();
    }
    else
    {
        time_t const now(time(nullptr));
        if(it->second.f_last_hit + 60 * 15 < now)
        {
            // reset counter back to 1 and date to "now"
            //
            it->second.f_count = 1;
        }
        else
        {
            ++it->second.f_count;
            r = it->second.f_count;
        }
        it->second.f_last_hit = now;
    }

    // TODO: make "3" a config parameter
    //
    if(r >= 3)
    {
        ed::message block;
        block.set_command("BLOCK");
        block.set_service("*");
        block.add_parameter("uri", a.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS));
        block.add_parameter("period", "1h");
        block.add_parameter("profile", "system-login-attempts");
        block.add_parameter("reason", "Three or more attempts at connecting to communicator daemon with the wrong credentials");
        f_server->broadcast_message(block);
    }
}


} // namespace communicator_daemon
// vim: ts=4 sw=4 et

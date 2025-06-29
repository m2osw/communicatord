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
 * \brief Implementation of the Gossip connection.
 *
 * The Communicator has a rule, if its IP address is smaller than the
 * IP address of another communicator, then it connects to it normally. That
 * creates the web of communicators in your Snap! Websites network.
 *
 * When a communicator has a larger IP address, then it instead creates a
 * Gossip connection. That allows that communicator to send its IP address
 * to that other communicator to make sure it is aware of it.
 *
 * \note
 * With the fluid settings, this is likely going to be dropped. It won't
 * be required.
 */

// self
//
#include    "gossip_connection.h"


// communicatord
//
#include    <communicatord/names.h>


// snaplogger
//
#include    <snaplogger/message.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{


namespace
{



std::int64_t    g_max_gossip_timeout = gossip_connection::MAX_TIMEOUT;



}


/** \class gossip_connection
 * \brief To send a GOSSIP to a remote communicatord.
 *
 * This class defines a connection used to send a GOSSIP message
 * to a remote communicator. Once the GOSSIP worked at least once,
 * this connection gets deleted.
 *
 * This connection is a timer, it manages an actual TCP/IP connection
 * which it attempts to create every now and then. This is because
 * we do not want to use too many resources to attempt to connect
 * to a computer which is down. (i.e. we use a thread to attempt
 * the connection since it can take forever if it does not work; i.e.
 * inter-computer socket connections may timeout after a minute or
 * two!)
 *
 * For the feat we use our 'permanent message connection.' This is
 * very well adapted. We just need to make sure to remove the
 * connection once we received confirmation the the GOSSIP message
 * was received by the remote host.
 */


/** \brief Initialize the gossip remote communicator connection.
 *
 * This object is actually a timer. Each time we get a tick
 * (i.e. process_timeout() callback gets called), a connection
 * is attempted against the remote communicatord daemon
 * specified by the addr and port parameters.
 *
 * The addr and port are both mandatory to this constructor.
 *
 * \todo
 * We need to correctly select PLAIN or SECURE. It's pretty secure to
 * use a PLAIN connection for a GOSSIP message, but if the IP:port we
 * were given represent a SECURE port, then it won't work.
 *
 * \param[in] rcs  The remote communicators object which we contact
 *                 whenever the GOSSIP message was confirmed by the
 *                 remote connection.
 * \param[in] address  The IP:port address of the remote communicator.
 */
gossip_connection::gossip_connection(
                  remote_communicators::pointer_t rcs
                , addr::addr const & address)
    : tcp_client_permanent_message_connection(
                  address
                , ed::mode_t::MODE_PLAIN //rcs->connection_mode()
                , -FIRST_TIMEOUT  // must be negative so first timeout is active (otherwise we get an immediately attempt, which we do not want in this case)
                , true)
    , f_address(address)
    , f_remote_communicators(rcs)
{
    std::string const addr_str(address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
    set_name(communicatord::g_name_communicatord_connection_gossip + (": " + addr_str));
}


/** \brief Process one timeout.
 *
 * We do not really have anything to do when a timeout happens. The
 * connection attempts are automatically done by the permanent
 * connection in the communicatord library.
 *
 * However, we want to increase the delay between attempts. For that,
 * we use this function and double the delay on each timeout until
 * it reaches about 1h. Then we stop doubling that delay. If the
 * remote communicatord never makes it, we won't swamp the network
 * by false attempts to connect to a dead computer.
 *
 * \todo
 * We need to let the watchdog know that such remote connections
 * fail for X amount of time. This is important to track what's
 * missing in the cluster (Even if we likely will have other means
 * to know of the problem.)
 */
void gossip_connection::process_timeout()
{
    tcp_client_permanent_message_connection::process_timeout();

    // increase the delay on each timeout until we reach
    // 'g_max_gossip_timeout'
    //
    std::int64_t const next_wait(std::min(f_wait * 2, g_max_gossip_timeout));
    if(next_wait != f_wait)
    {
        f_wait = next_wait;
        set_timeout_delay(f_wait);
    }
}


/** \brief Process the reply from our GOSSIP message.
 *
 * This function processes any messages received from the remote
 * system.
 *
 * We currently really only expect RECEIVED as a reply.
 *
 * \param[in] msg  The message received from the remote communicatord.
 */
void gossip_connection::process_message(ed::message & msg)
{
    SNAP_LOG_TRACE
        << "gossip connection received message: ["
        << msg.to_message()
        << "]"
        << SNAP_LOG_SEND;

    std::string const & command(msg.get_command());
    if(command == communicatord::g_name_communicatord_cmd_received)
    {
        // we got confirmation that the GOSSIP went across
        //
        f_remote_communicators->gossip_received(f_address);
    }
}


/** \brief The remote connection failed, we cannot gossip with it.
 *
 * This function gets called if a connection to a remote communicator fails.
 *
 * In case of a gossip, this is because that other computer is expected to
 * connect with us, but it may not know about us so we tell it hello for
 * that reason.
 *
 * We have this function because on a failure we want to mark that
 * computer as being down. This is important for the snapmanagerdaemon.
 *
 * \param[in] error_message  The error that occurred.
 */
void gossip_connection::process_connection_failed(std::string const & error_message)
{
    // make sure the default function does its job.
    //
    tcp_client_permanent_message_connection::process_connection_failed(error_message);

    // now let people know about the fact that this other computer is
    // unreachable
    //
    f_remote_communicators->server_unreachable(f_address);
}


/** \brief Once connected send the GOSSIP message.
 *
 * This function gets called whenever the connection is finally up.
 * This gives us the opportunity to send the GOSSIP message to the
 * remote host.
 *
 * Note that at this time this happens in the main thread. The
 * secondary thread was used to call the connect() function, but
 * it is not used to send or receive any messages.
 */
void gossip_connection::process_connected()
{
    // TODO:
    // The default process_connected() function disables the timer
    // of the gossip connection. This means that we will not get
    // any further process_timeout() calls until we completely
    // lose the connection. This is possibly not what we want, or
    // at least we should let the sitter know that we were
    // connected to a communicatord, yes, sent the GOSSIP,
    // all good up to here, but never got a reply! Not getting
    // a reply is likely to mean that the connection we establish
    // is somehow bogus even if it does not Hang Up on us.
    //
    // You may read the Byzantine fault tolerance in regard to
    // supporting a varied set of processes to detect the health
    // of many different nodes in a cluster.
    //
    // https://en.wikipedia.org/wiki/Byzantine_fault_tolerance
    //
    tcp_client_permanent_message_connection::process_connected();

    // we are connected so we can send the GOSSIP message
    // (each time we reconnect!)
    //
    ed::message gossip;
    gossip.set_command(communicatord::g_name_communicatord_cmd_gossip);
    gossip.add_parameter(
              "my_address"
            , f_remote_communicators->get_connection_address().to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
    send_message(gossip); // do not cache, if we lose the connection, we lose the message and that's fine in this case
}


void gossip_connection::set_max_gossip_timeout(std::int64_t max_gossip_timeout)
{
    g_max_gossip_timeout = max_gossip_timeout;
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

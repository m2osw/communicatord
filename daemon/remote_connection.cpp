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
 * \brief Implementation of the remote communicatord connection.
 *
 * This is the implementation of the remote communicatord connection.
 * Connection used to communicate with other communicators running
 * on other servers.
 */

// self
//
#include    "remote_connection.h"



// communicatord
//
#include    <communicatord/flags.h>
#include    <communicatord/names.h>


// libaddr
//
#include    <libaddr/exception.h>


// snaplogger
//
#include    <snaplogger/message.h>


// C++
//
#include    <iomanip>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{



/** \class remote_connection
 * \brief Describe a remove communicatord by IP address, etc.
 *
 * This class defines a communicatord server. Mainly we include
 * the IP address of the server to connect to.
 *
 * The object also maintains the status of that server. Whether we
 * can connect to it (because if not the connection stays in limbo
 * and we should not try again and again forever. Instead we can
 * just go to sleep and try again "much" later saving many CPU
 * cycles.)
 *
 * It also gives us a way to quickly track communicatord objects
 * that REFUSE our connection.
 */


/** \brief Setup a remote_connection object.
 *
 * This initialization function sets up the attached ed::timer
 * to 1 second delay before we try to connect to this remote
 * communicatord. The timer is reused later when the connection
 * is lost, a communicatord returns a REFUSE message to our
 * CONNECT message, and other similar errors.
 *
 * \param[in] cs  The communicator server shared pointer.
 * \param[in] addr  The address to connect to.
 */
remote_connection::remote_connection(
              server::pointer_t cs
            , addr::addr const & address
            , bool secure)
    : tcp_client_permanent_message_connection(
              address
            , (secure
                ? ed::mode_t::MODE_SECURE
                : ed::mode_t::MODE_PLAIN)
            , REMOTE_CONNECTION_DEFAULT_TIMEOUT)
    , base_connection(cs, false)
    , f_address(address)
{
    std::string const addr_str(address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
    set_name(communicatord::g_name_communicatord_connection_remote_communicator_out + (": " + addr_str));
}


remote_connection::~remote_connection()
{
    try
    {
        SNAP_LOG_DEBUG
            << "deleting remote_connection connection: "
            << f_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT)
            << SNAP_LOG_SEND;
    }
    catch(addr::addr_invalid_argument const &)
    {
    }
}


int remote_connection::get_socket() const
{
    return tcp_client_permanent_message_connection::get_socket();
}


void remote_connection::process_message(ed::message & msg)
{
    if(f_server_name.empty())
    {
        f_server_name = msg.get_sent_from_server();
    }

    base_connection::pointer_t conn(std::dynamic_pointer_cast<base_connection>(shared_from_this()));
    msg.user_data(conn);
    f_server->dispatch_message(msg);
}


void remote_connection::process_connection_failed(std::string const & error_message)
{
    tcp_client_permanent_message_connection::process_connection_failed(error_message);

    SNAP_LOG_ERROR
        << "the connection to a remote communicator failed: \""
        << error_message
        << "\"."
        << SNAP_LOG_SEND;

    // were we connected? if so this is a hang up
    //
    if(f_connected
    && !f_server_name.empty())
    {
        f_connected = false;

        ed::message hangup;
        hangup.set_command(communicatord::g_name_communicatord_cmd_hangup);
        hangup.set_service(communicatord::g_name_communicatord_service_local_broadcast);
        hangup.add_parameter(communicatord::g_name_communicatord_param_server_name, f_server_name);
        f_server->broadcast_message(hangup);
    }

    // we count the number of failures, after a certain number we raise a
    // flag so that way the administrator is warned about the potential
    // problem; we take the flag down if the connection comes alive
    //
    if(f_failures <= 0)
    {
        f_failure_start_time = time(nullptr);
        f_failures = 1;
    }
    else if(f_failures < std::numeric_limits<decltype(f_failures)>::max())
    {
        ++f_failures;
    }

    // a remote connection can have one of three timeouts at this point:
    //
    // 1. one minute between attempts, this is the default
    // 2. five minutes between attempts, this is used when we receive a
    //    DISCONNECT, leaving time for the remote computer to finish
    //    an update or reboot
    // 3. one whole day between attemps, when the remote computer sent
    //    us a "Too Busy" error
    //
    // so... we want to generate an error when:
    //
    // (a) we made at least 20 attempts
    // (b) at least one hour went by
    // (c) each attempt resulted in an error
    //
    // note that 20 attempts in the default case [(1) above] represents
    // about 20 min.; and 20 attempts in the seconds case [(2) above]
    // represents 100 min. which is nearly two hours; however, the
    // "Too Busy" error means we'd wait 20 days before flagging that
    // computer as dead, this may be of concern?
    //
    time_t const time_elapsed(time(nullptr) - f_failure_start_time);
    if(!f_flagged
    && f_failures >= 20
    && time_elapsed > 60LL * 60LL)
    {
        f_flagged = true;

        std::stringstream ss;

        ss << "connecting to "
           << f_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT)
           << ", failed "
           << std::to_string(f_failures)
           << " times in a row for "
           << std::setfill('0')
           << std::setw(2)
           << ((time_elapsed / 1'000'000LL / 60 / 60) % 24)
           << ":"
           << std::setw(2)
           << ((time_elapsed / 1'000'000LL / 60) % 60)
           << ":"
           << std::setw(2)
           << ((time_elapsed / 1'000'000LL) % 60)
           << " (HH:MM:SS), please verify this IP address"
              " and that it is expected that the computer"
              " fails connecting. If not, please remove that IP address"
              " from the list of neighbors AND THE FIREWALL if it is there too.";

        communicatord::flag::pointer_t flag(COMMUNICATORD_FLAG_UP(
                      "communicatord"
                    , "remote-connection"
                    , "connection-failed"
                    , ss.str()
                ));
        flag->set_priority(95);
        flag->add_tag("security");
        flag->add_tag("data-leak");
        flag->add_tag("network");
        flag->save();
    }
}


void remote_connection::process_connected()
{
    f_connected = true;

    // take the remote connection failure flag down
    //
    // Note: by default we set f_failures to -1 so when we reach here we
    //       get the flag down once; after that, we take the flag down
    //       only if we raised it in between, that way we save some time
    //
    if(f_failures != 0
    || f_failure_start_time != 0
    || f_flagged)
    {
        f_failure_start_time = 0;
        f_failures = 0;
        f_flagged = false;

        communicatord::flag::pointer_t flag(COMMUNICATORD_FLAG_DOWN(
                           "communicatord"
                         , "remote-connection"
                         , "connection-failed")
                     );
        flag->save();
    }

    tcp_client_permanent_message_connection::process_connected();

    f_server->process_connected(shared_from_this());

    // reset the wait to the default 5 minutes
    //
    // (in case we had a shutdown event from that remote communicator
    // and changed the timer to 15 min.)
    //
    // later we probably want to change the mechanism if we want to
    // slowdown over time
    //
    set_timeout_delay(REMOTE_CONNECTION_DEFAULT_TIMEOUT);
}


bool remote_connection::send_message(ed::message & msg, bool cache)
{
    return tcp_client_permanent_message_connection::send_message(msg, cache);
}


addr::addr const & remote_connection::get_address() const
{
    return f_address;
}


} // namespace communicator_daemon
// vim: ts=4 sw=4 et

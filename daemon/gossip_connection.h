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
#pragma once

/** \file
 * \brief Definition of the Gossip connection.
 *
 * The Gossip connection is used to let another communicator know about
 * us when it is expected that this other communicator connect to us
 * (based of our respective IP addresses).
 */

// self
//
#include    "remote_communicators.h"
#include    "server.h"


// eventdispatcher
//
#include    <eventdispatcher/tcp_client_permanent_message_connection.h>



namespace communicator_daemon
{


class gossip_connection
    : public ed::tcp_client_permanent_message_connection
{
public:
    typedef std::shared_ptr<gossip_connection> pointer_t;

    static std::int64_t const   FIRST_TIMEOUT = 5LL * 1'000'000LL;  // 5 seconds before first attempt
    static std::int64_t const   MAX_TIMEOUT = 3600LL * 1'000'000LL; // wait up to 1h for the next attempt

                                gossip_connection(
                                          remote_communicators::pointer_t rcs
                                        , addr::addr const & address);

    // tcp_client_permanent_message_connection implementation
    virtual void                process_timeout() override;
    virtual void                process_message(ed::message & msg) override;
    virtual void                process_connection_failed(std::string const & error_message) override;
    virtual void                process_connected() override;

    static void                 set_max_gossip_timeout(std::int64_t max_gossip_timeout);

private:
    addr::addr const            f_address;
    std::int64_t                f_wait = FIRST_TIMEOUT;
    remote_communicators::pointer_t
                                f_remote_communicators = remote_communicators::pointer_t();
};


} // namespace communicator_daemon
// vim: ts=4 sw=4 et

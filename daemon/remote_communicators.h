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

/** \file
 * \brief Declaration of the remote communicators manager.
 *
 * The remote communicators is in charge of managing the connections
 * to the other communicators on your network.
 *
 * When the IP address is smaller than ours, then we connect to that
 * communicatord. When the IP address is larger than ours, then we
 * instead connect to send a GOSSIP message. The GOSSIP is a mean to
 * connect to new servers without the need to define them on all your
 * servers. Defining it on one server is enough to get things started.
 */

// self
//
#include    "server.h"


// eventdispatcher
//
#include    <eventdispatcher/tcp_bio_client.h>



namespace communicator_daemon
{



class remote_connection;
class gossip_connection;


class remote_communicators
    : public std::enable_shared_from_this<remote_communicators>
{
public:
    typedef std::shared_ptr<remote_communicators>    pointer_t;

                                            remote_communicators(
                                                  server::pointer_t communicator
                                                , addr::addr const & my_addr);

    addr::addr const &                      get_my_address() const;
    void                                    add_remote_communicator(std::string const & addr_port);
    void                                    stop_gossiping();
    void                                    too_busy(addr::addr const & address);
    void                                    shutting_down(addr::addr const & address);
    void                                    server_unreachable(addr::addr const & address);
    void                                    gossip_received(addr::addr const & address);
    void                                    forget_remote_connection(addr::addr const & address);
    size_t                                  count_live_connections() const;

private:
    typedef std::map<addr::addr, std::shared_ptr<remote_connection>>
                                            sorted_remote_connections_by_address_t;
    typedef std::map<addr::addr, std::shared_ptr<gossip_connection>>
                                            sorted_gossip_connections_by_address_t;

    server::pointer_t                       f_server = server::pointer_t();
    addr::addr const &                      f_my_address;
    int64_t                                 f_last_start_date = 0;
    sorted_list_of_addresses_t              f_all_ips = sorted_list_of_addresses_t();
    sorted_remote_connections_by_address_t  f_smaller_ips = sorted_remote_connections_by_address_t();   // we connect to smaller IPs
    sorted_gossip_connections_by_address_t  f_gossip_ips = sorted_gossip_connections_by_address_t();    // we gossip with larger IPs

    // larger IPs connect so they end up in the list with the local connections
    //service_connection_list_t               f_larger_ips = service_connection_list_t();       // larger IPs connect to us
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

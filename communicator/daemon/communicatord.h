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
#pragma once

/** \file
 * \brief Declaration of the main server.
 *
 * The definition of the Communicator server. This class is the
 * one which drives everything else in the Communicator server.
 */

// self
//
#include    "cache.h"
#include    "utils.h"


// communicator
//
#include    <communicator/communicator.h>


// eventdispatcher
//
#include    <eventdispatcher/connection.h>
#include    <eventdispatcher/connection_with_send_message.h>
#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/dispatcher.h>
#include    <eventdispatcher/dispatcher_support.h>


// serverplugins
//
#include    <serverplugins/collection.h>
#include    <serverplugins/server.h>
#include    <serverplugins/signals.h>


// advgetopt
//
#include    <advgetopt/advgetopt.h>


// libaddr
//
#include    <libaddr/addr.h>



namespace communicator_daemon
{


class base_connection;
class remote_communicators;


SERVERPLUGINS_VERSION(communicatord, 1, 0)


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
class communicatord
    : public ed::dispatcher_support
    , public ed::connection_with_send_message
    , public serverplugins::server
{
public:
    typedef std::shared_ptr<communicatord>     pointer_t;

    static std::size_t const    COMMUNICATORD_MAX_CONNECTIONS = 100;

                                communicatord(int argc, char * argv[]);
                                communicatord(communicatord const & src) = delete;
    virtual                     ~communicatord() override;
    communicatord &             operator = (communicatord const & rhs) = delete;

    int                         run();
    std::string const &         get_server_name() const;

    // ed::dispatcher_support implementation
    //
    virtual bool                dispatch_message(ed::message & msg) override;

    // ed::connection_with_send_message implementation
    //
    virtual bool                send_message(ed::message & msg, bool cache = false) override;
    virtual void                stop(bool quitting) override;

    void                        send_status(
                                          ed::connection::pointer_t connection
                                        , ed::connection::pointer_t * reply_connection = nullptr);
    std::string                 get_local_services() const;
    addr::addr                  get_connection_address() const;
    std::string                 get_services_heard_of() const;
    void                        add_neighbors(std::string const & new_neighbors);
    void                        remove_neighbor(std::string const & neighbor);
    void                        read_neighbors();
    void                        save_neighbors();
    bool                        verify_command(
                                          std::shared_ptr<base_connection> connection
                                        , ed::message const & msg);
    void                        process_connected(ed::connection::pointer_t connection);
    void                        connection_lost(addr::addr const & remote_addr);
    bool                        forward_message(ed::message & msg);
    void                        broadcast_message(
                                          ed::message & message
                                        , std::vector<std::shared_ptr<base_connection>> const & accepting_remote_connections = std::vector<std::shared_ptr<base_connection>>());
    void                        process_load_balancing();
    void                        cluster_status(ed::connection::pointer_t reply_connection);
    bool                        is_debug() const;
    bool                        is_tcp_connection(ed::message & msg); // connection defined in message is TCP (or Unix) opposed to UDP

    PLUGIN_SIGNAL_WITH_MODE(initialize, (advgetopt::getopt & opts), (opts), NEITHER);
    PLUGIN_SIGNAL_WITH_MODE(terminate, (), (), NEITHER);
    PLUGIN_SIGNAL_WITH_MODE(new_connection, (std::shared_ptr<base_connection> conn), (conn), NEITHER);

    void                        msg_accept(ed::message & msg);
    void                        msg_clock_status(ed::message & msg);
    void                        msg_cluster_status(ed::message & msg);
    void                        msg_commands(ed::message & msg);
    void                        msg_connect(ed::message & msg);
    void                        msg_disconnect(ed::message & msg);
    void                        msg_forget(ed::message & msg);
    void                        msg_gossip(ed::message & msg);
    void                        msg_list_services(ed::message & msg);
    virtual void                msg_log_unknown(ed::message & msg); // reimplementation to indicate the name of the connection when available
    void                        msg_public_ip(ed::message & msg);
    void                        msg_quitting(ed::message & msg);
    void                        msg_refuse(ed::message & msg);
    void                        msg_register(ed::message & msg);
    void                        msg_service_status(ed::message & msg);
    void                        msg_shutdown(ed::message & msg);
    void                        msg_unregister(ed::message & msg);

private:
    int                         init();
    void                        init_server_name();
    void                        init_server_ownership();
    bool                        init_max_connections();
    void                        init_max_gossip_timeout();
    void                        load_list_of_local_services();
    void                        init_interrupt();
    bool                        init_local_tcp_listener();
    bool                        init_unix_listener();
    bool                        init_plain_remote_listener();
    bool                        init_secure_remote_listener();
    void                        init_ping_listener();
    bool                        init_connection_address();
    void                        init_neighbors();
    void                        load_plugins();
    void                        drop_privileges();
    void                        refresh_heard_of();
    void                        register_for_loadavg(std::string const & ip);
    bool                        shutting_down(ed::message & msg);
    bool                        check_broadcast_message(ed::message const & msg);
    bool                        communicator_message(ed::message & msg);
    void                        transmission_report(ed::message & msg, bool cached);

    advgetopt::getopt               f_opts;
    ed::dispatcher::pointer_t       f_dispatcher = ed::dispatcher::pointer_t();
    std::string                     f_server_name = std::string();
    std::string                     f_neighbors_cache_filename = std::string();
    std::string                     f_user_name = std::string();
    std::string                     f_group_name = std::string();
    std::string                     f_public_ip = std::string();        // f_listener IP address for plain connections
    std::string                     f_secure_ip = std::string();        // f_listener IP address with TLS
    ed::communicator::pointer_t     f_communicator = ed::communicator::pointer_t();
    ed::connection::weak_pointer_t  f_interrupt = ed::connection::pointer_t();        // signalfd
    ed::connection::pointer_t       f_stable_clock = ed::connection::pointer_t();     // timer (also generates cppprocess objects)
    ed::connection::pointer_t       f_local_listener = ed::connection::pointer_t();   // TCP/IP
    ed::connection::pointer_t       f_remote_listener = ed::connection::pointer_t();  // TCP/IP
    ed::connection::pointer_t       f_secure_listener = ed::connection::pointer_t();  // TCP/IP
    ed::connection::pointer_t       f_unix_listener = ed::connection::pointer_t();    // Unix socket
    ed::connection::pointer_t       f_ping = ed::connection::pointer_t();             // UDP/IP
    addr::addr                      f_connection_address = addr::addr();
    std::string                     f_local_services = std::string();
    advgetopt::string_set_t         f_local_services_list = advgetopt::string_set_t();
    std::string                     f_services_heard_of = std::string();
    advgetopt::string_set_t         f_services_heard_of_list = advgetopt::string_set_t();
    std::string                     f_explicit_neighbors = std::string();
    addr::addr::set_t               f_all_neighbors = addr::addr::set_t();
    std::shared_ptr<remote_communicators>
                                    f_remote_communicators = std::shared_ptr<remote_communicators>();
    std::size_t                     f_max_connections = COMMUNICATORD_MAX_CONNECTIONS;
    std::size_t                     f_max_pending_connections = COMMUNICATORD_MAX_CONNECTIONS;
    std::size_t                     f_total_count_sent = 0; // f_all_neighbors.size() sent along CLUSTERUP/DOWN/COMPLETE/INCOMPLETE
    int                             f_default_remote_port = communicator::REMOTE_PORT;
    bool                            f_shutdown = false;
    bool                            f_debug_all_messages = false;
    bool                            f_force_restart = false;
    cache                           f_local_message_cache = cache();
    std::map<std::string, time_t>   f_received_broadcast_messages = (std::map<std::string, time_t>());
    std::string                     f_cluster_status = std::string();
    std::string                     f_cluster_complete = std::string();
    serverplugins::collection::pointer_t
                                    f_plugins = serverplugins::collection::pointer_t();
};
#pragma GCC diagnostic pop



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

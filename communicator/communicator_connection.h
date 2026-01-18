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


// advgetopt
//
#include    <advgetopt/advgetopt.h>


// eventdispatcher
//
#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/connection.h>
#include    <eventdispatcher/connection_with_send_message.h>
#include    <eventdispatcher/dispatcher.h>
#include    <eventdispatcher/dispatcher_support.h>
#include    <eventdispatcher/message.h>
#include    <eventdispatcher/timer.h>


// snapdev
//
#include    <snapdev/join_strings.h>
#include    <snapdev/to_string_literal.h>


// C++
//
#include    <string_view>


namespace communicator
{



constexpr int const             LOCAL_PORT = 4040;      // cd://<loopback-ip>
constexpr int const             UDP_PORT = 4041;        // cdu://<loopback-ip> (any IP is accepted at the moment, but it's expected to be local)
constexpr int const             REMOTE_PORT = 4042;     // cd://<private-ip>
constexpr int const             SECURE_PORT = 4043;     // cds://<public-ip>
constexpr std::string_view      g_communicator_default_ip = "127.0.0.1";
constexpr std::string_view      g_communicator_any_ip = "0.0.0.0";
constexpr std::string_view      g_communicator_default_port = snapdev::integer_to_string_literal<LOCAL_PORT>.data();
constexpr std::string_view      g_communicator_colon = ":";
constexpr std::string_view      g_communicator_default_ip_port = snapdev::join_string_views<g_communicator_default_ip, g_communicator_colon, g_communicator_default_port>;
constexpr std::string_view      g_communicator_any_ip_port = snapdev::join_string_views<g_communicator_any_ip, g_communicator_colon, g_communicator_default_port>;




class communicator_connection
    : public ed::timer
    , public ed::dispatcher_support
    , public ed::connection_with_send_message
{
public:
    typedef std::shared_ptr<communicator_connection>   pointer_t;

                                communicator_connection(
                                      advgetopt::getopt & opts
                                    , std::string const & service_name);
                                communicator_connection(communicator_connection const &) = delete;
    virtual                     ~communicator_connection() override;
    communicator_connection &   operator = (communicator_connection const &) = delete;

    advgetopt::getopt &         get_options();
    std::string const &         service_name() const;
    void                        process_communicator_options();
    void                        unregister_communicator(bool quitting);
    bool                        is_connected() const;

    // connection_with_send_message implementation
    //
    virtual bool                send_message(ed::message & msg, bool cache = false) override;

    // new callbacks
    //
    virtual void                service_status(std::string const & service, std::string const & status);

private:
    void                        msg_status(ed::message & msg);

    advgetopt::getopt &         f_opts;
    ed::communicator::pointer_t f_communicator = ed::communicator::pointer_t();
    std::string                 f_service_name = std::string();
    ed::dispatcher::pointer_t   f_dispatcher = ed::dispatcher::pointer_t();
    ed::connection::pointer_t   f_communicator_connection = ed::connection::pointer_t();
};


void request_failure(ed::message & msg);



} // namespace communicator
// vim: ts=4 sw=4 et

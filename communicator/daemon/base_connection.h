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
 * \brief Declaration of the base_connection class.
 *
 * All the client connections are derived from this base connection class
 * which allows us to manage many function in one place instead of having
 * them duplicated in three or more places.
 */



// self
//
#include    "communicatord.h"


// eventdispatcher
//
#include    <eventdispatcher/connection.h>



namespace communicator_daemon
{



enum class connection_type_t
{
    CONNECTION_TYPE_DOWN,   // not connected
    CONNECTION_TYPE_LOCAL,  // a service on this computer
    CONNECTION_TYPE_REMOTE  // another communicatord on another computer
};



class base_connection
{
public:
    typedef std::shared_ptr<base_connection>    pointer_t;
    typedef std::vector<pointer_t>              vector_t;

                                base_connection(
                                      communicatord * s
                                    , bool is_udp);
                                base_connection(base_connection const &) = delete;
    virtual                     ~base_connection();

    base_connection &           operator = (base_connection const &) = delete;

    std::string                 get_connection_name() const;
    void                        connection_started();
    time_t                      get_connection_started() const;
    void                        connection_ended();
    time_t                      get_connection_ended() const;
    void                        set_server_name(std::string const & server_name);
    std::string                 get_server_name() const;
    void                        set_connection_address(addr::addr const & my_address);
    addr::addr                  get_connection_address() const;
    void                        set_connection_type(connection_type_t type);
    connection_type_t           get_connection_type() const;
    void                        set_username(std::string const & username);
    std::string                 get_username() const;
    void                        set_password(std::string const & password);
    std::string                 get_password() const;
    void                        set_services(std::string const & services);
    void                        get_services(advgetopt::string_set_t & services);
    bool                        has_service(std::string const & name);
    void                        set_services_heard_of(std::string const & services);
    void                        get_services_heard_of(advgetopt::string_set_t & services);
    void                        add_commands(std::string const & commands);
    bool                        understand_command(std::string const & command);
    bool                        has_commands() const;
    void                        remove_command(std::string const & command);
    void                        mark_as_remote();
    bool                        is_remote() const;
    bool                        is_udp() const;
    void                        set_wants_loadavg(bool wants_loadavg);
    bool                        wants_loadavg() const;

    // allows us to send messages directly from the base_connection class
    bool                        send_message_to_connection(ed::message & msg, bool cache = false);

    virtual int                 get_socket() const = 0;

protected:
    communicatord *             f_server = nullptr;

private:
    advgetopt::string_set_t     f_understood_commands = advgetopt::string_set_t();
    time_t                      f_started_on = -1;
    time_t                      f_ended_on = -1;
    connection_type_t           f_type = connection_type_t::CONNECTION_TYPE_DOWN;
    std::string                 f_server_name = std::string();
    addr::addr                  f_connection_address = addr::addr();
    advgetopt::string_set_t     f_services = advgetopt::string_set_t();
    advgetopt::string_set_t     f_services_heard_of = advgetopt::string_set_t();
    std::string                 f_username = std::string();
    std::string                 f_password = std::string();
    bool                        f_remote_connection = false;
    bool                        f_wants_loadavg = false;
    bool                        f_is_udp = false;
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

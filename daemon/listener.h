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
#pragma once

/** \file
 * \brief Implementation of the listener object.
 *
 * The listener object is the one that accepts connections from the outside.
 */



// self
//
#include    "server.h"


// eventdispatcher
//
#include    <eventdispatcher/tcp_server_connection.h>



namespace communicator_daemon
{


class listener
    : public ed::tcp_server_connection
{
public:
    typedef std::shared_ptr<listener>       pointer_t;

                        listener(
                              server::pointer_t cs
                            , addr::addr const & addr
                            , std::string const & certificate
                            , std::string const & private_key
                            , int max_connections
                            , bool local
                            , std::string const & server_name);

    // ed::tcp_server_connection
    virtual void        process_accept() override;

    void                set_username(std::string const & username);
    std::string         get_username() const;
    void                set_password(std::string const & password);
    std::string         get_password() const;

private:
    server::pointer_t   f_server = server::pointer_t();
    bool const          f_local = false;
    std::string const   f_server_name;
    std::string         f_username = std::string();
    std::string         f_password = std::string();
};


} // namespace communicator_daemon
// vim: ts=4 sw=4 et

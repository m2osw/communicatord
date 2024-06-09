// Copyright (c) 2011-2024  Made to Order Software Corp.  All Rights Reserved
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
 * \brief The declaration of the service class.
 *
 * The service class is the one used whenever a service connects to the
 * communicatord daemon.
 */

// self
//
#include    "base_connection.h"
#include    "server.h"


// eventdispatcher
//
#include    <eventdispatcher/tcp_server_client_message_connection.h>



namespace communicator_daemon
{


class service_connection
    : public ed::tcp_server_client_message_connection
    , public base_connection
{
public:
    typedef std::shared_ptr<service_connection>    pointer_t;

                        service_connection(
                                  server::pointer_t cs
                                , ed::tcp_bio_client::pointer_t client
                                , std::string const & server_name);
    virtual             ~service_connection() override;

    virtual int         get_socket() const;

    // ed::tcp_server_client_message_connection implementation
    virtual void        process_message(ed::message & msg) override;
    virtual bool        send_message(ed::message & msg, bool cache = false) override;
    virtual void        process_timeout() override;
    virtual void        process_error() override;
    virtual void        process_hup() override;
    virtual void        process_invalid() override;
    virtual void        connection_removed() override;

    void                send_status();
    void                properly_named();
    addr::addr const &  get_address() const;
    void                block_ip();

private:
    std::string const   f_server_name;
    addr::addr          f_address = addr::addr();
    bool                f_named = false;
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

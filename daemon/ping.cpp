// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/snapcommunicator
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
 * \brief Implementation of the ping connection.
 *
 */

// self
//
#include    "ping.h"


//// snapwebsites lib
////
//#include <snapwebsites/chownnm.h>
//#include <snapwebsites/flags.h>
//#include <snapwebsites/glob_dir.h>
//#include <snapwebsites/loadavg.h>
//#include <snapwebsites/log.h>
//#include <snapwebsites/qcompatibility.h>
//#include <snapwebsites/snap_communicator.h>
//#include <snapwebsites/snapwebsites.h>
//
//
//// snapdev lib
////
//#include <snapdev/not_used.h>
//#include <snapdev/tokenize_string.h>
//
//
//// libaddr lib
////
//#include <libaddr/addr_exception.h>
//#include <libaddr/addr_parser.h>
//#include <libaddr/iface.h>
//
//
//// Qt lib
////
//#include <QFile>
//
//
//// C++ lib
////
//#include <atomic>
//#include <cmath>
//#include <fstream>
//#include <iomanip>
//#include <sstream>
//#include <thread>
//
//
//// C lib
////
//#include <grp.h>
//#include <pwd.h>
//#include <sys/resource.h>









namespace scd
{



/** \class ping
 * \brief Handle UDP messages from clients.
 *
 * This class is an implementation of the snap server connection so we can
 * handle new connections from various clients.
 */



/** \brief The messenger initialization.
 *
 * The messenger receives UDP messages from various sources (mainly
 * backends at this point).
 *
 * \param[in] cs  The snap communicator server we are listening for.
 * \param[in] address  The address and port to listen on. Most often it is
 *                     127.0.0.1 for the UDP because we currently only allow
 *                     for local messages.
 */
ping::ping(server::pointer_t cs, addr::addr const & address)
    : udp_server_message_connection(address)
    , base_connection(cs, true)
{
}


void ping::process_message(ed::message & msg)
{
    //f_server->process_message(shared_from_this(), msg, true);
    msg.user_data(shared_from_this());
    f_server->dispatch_message(msg);
}



} // namespace scd
// vim: ts=4 sw=4 et

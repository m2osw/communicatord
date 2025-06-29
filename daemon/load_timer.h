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
 * \brief Definition of the load_timer class.
 *
 * The load average of the computer is collected on all computers and
 * shared between all the Communicators. This is used to know
 * whether a computer is overloaded and make use of another in that
 * case.
 */

// self
//
#include    "server.h"


// eventdispatcher
//
#include    "eventdispatcher/timer.h"



namespace communicator_daemon
{



class load_timer
    : public ed::timer
{
public:
                        load_timer(server::pointer_t cs);

    // ed::timer implementation
    virtual void        process_timeout() override;

private:
    server::pointer_t   f_server = server::pointer_t();
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

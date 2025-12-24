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

/** \file
 * \brief Implementation of load_timer object.
 *
 * We use a timer to know when to check the load average of the computer.
 * This is used to know whether a computer is too heavily loaded and
 * so whether it should or not be accessed.
 */

// self
//
#include    "load_timer.h"

#include    "loadavg.h"


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{
namespace loadavg
{



/** \class load_timer
 * \brief Provide a tick to offer load balancing information.
 *
 * This class is an implementation of a timer to offer load balancing
 * information between various front and backend computers in the cluster.
 */


/** \brief The timer initialization.
 *
 * The timer ticks once per second to retrieve the current load of the
 * system and forward it to whichever computer that requested the
 * information.
 *
 * \param[in] l  The loadavg plugin class we are listening for.
 */
load_timer::load_timer(loadavg * l)
    : timer(1'000'000LL)  // 1 second in microseconds
    , f_loadavg(l)
{
    set_enable(false);
}


load_timer::~load_timer()
{
}


void load_timer::process_timeout()
{
    f_loadavg->process_load_balancing();
}



} // namespace loadavg
} // namespace communicator_daemon
// vim: ts=4 sw=4 et

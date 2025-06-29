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

/** \file
 * \brief Implementation of an interrupt handler.
 *
 * This class is used to allow for a clean exit on an Ctrl-C.
 */

// self
//
#include    "interrupt.h"



// C
//
#include    <signal.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{



/** \class interrupt
 * \brief Handle the SIGINT that is expected to stop the server.
 *
 * This class is an implementation of the ed::signal that listens
 * on the SIGINT.
 */



/** \brief The interrupt initialization.
 *
 * The interrupt uses the signalfd() function to obtain a way to listen on
 * incoming Unix signals.
 *
 * Specifically, it listens on the SIGINT signal, which is the equivalent
 * to the Ctrl-C.
 *
 * \param[in] cs  The communicatord we are listening for.
 */
interrupt::interrupt(server::pointer_t cs)
    : signal(SIGINT)
    , f_server(cs)
{
    unblock_signal_on_destruction();
    set_name("communicatord interrupt");
}


/** \brief Call the stop function of the communicatord object.
 *
 * When this function is called, the signal was received and thus we are
 * asked to quit as soon as possible.
 */
void interrupt::process_signal()
{
    // we simulate the STOP, so pass 'false' (i.e. not quitting)
    //
    f_server->stop(false);
    ed::communicator::instance()->remove_connection(shared_from_this());
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

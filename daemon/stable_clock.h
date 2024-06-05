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
 * \brief Definition of the clock verification on startup.
 *
 * Your clock may not be up to date on a reboot. This class starts a
 * background process to run ntp-wait and create a file under
 * /run/communicatord and send a message once the clock is known to
 * be adjusted.
 *
 * \note
 * On a VPN, this is likely not useful (i.e. VPNs time is generally
 * set to their host time and the hosts are already synchronized as
 * expected).
 */

// self
//
#include    "server.h"


// eventdispatcher
//
#include    <eventdispatcher/thread_done_signal.h>


// cppthread
//
#include    <cppthread/runner.h>
#include    <cppthread/thread.h>



namespace communicator_daemon
{



namespace detail
{
class ntp_wait;
typedef std::shared_ptr<ntp_wait>   ntp_wait_pointer_t;
}


class stable_clock
    : public ed::thread_done_signal
{
public:
    typedef std::shared_ptr<stable_clock>   pointer_t;
    typedef std::weak_ptr<stable_clock>     weak_pointer_t;

                            stable_clock(server::pointer_t cs);

    virtual void            process_read() override;

private:
    server::pointer_t               f_server = server::pointer_t();
    detail::ntp_wait_pointer_t      f_runner = detail::ntp_wait_pointer_t();
    cppthread::thread::pointer_t    f_thread = cppthread::thread::pointer_t();
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

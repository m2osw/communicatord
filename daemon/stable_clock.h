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
 * \brief Check the clock.
 *
 * A computer clock may not be up to date on a reboot. This class starts
 * a background process to either run ntp-wait or timedatectl and as a
 * result change the clock status from UNKNOWN to (hopefully) STABLE.
 *
 * On newer system, the timedatectl works as expected. Although less
 * precise than the old ntp system, it uses very little resources and
 * you normally have no setup to do. If you install ntp, then we expect
 * you also want to install ntp-wait to verify the ntp service instead.
 *
 * The class determines one of the following statuses:
 *
 * \li CLOCK_STATUS_UNKNOWN -- the state has not yet been determined
 * \li CLOCK_STATUS_NO_NTP -- we could not find ntp-wait nor timedatectl
 * \li CLOCK_STATUS_STABLE -- we ran one of the NTP tools and determine
 *                            that the clock was properly synchronized
 * \li CLOCK_STATUS_INVALID -- the tool could not see the clock as being
 *                             synchronized; at the moment the clock should
 *                             be considered invalid (it could be completely
 *                             out of date)
 *
 * Note that in most cases the CLOCK_STATUS_NO_NTP should be viewed as
 * "the clock is probably okay." It certainly depends on the project using
 * the stable clock signal:
 *
 * \li CLOCK_STABLE -- the clock is considered stable, however, it may be
 *                     in the "no NTP" state, check the clock_resolution
 *                     parameter
 * \li CLOCK_UNSTABLE -- the clock was not yet checked or it the tool timed
 *                       out, the clock_error parameter is set to "checking"
 *                       or "invalid"
 *
 * \note
 * On a VPN, the ntp service is likely not useful (i.e. VPNs time is
 * generally set to their host time and the hosts are already synchronized
 * as expected).
 *
 * \todo
 * If it looks like ntp-wait is installed, we should also check whether
 * the ntp service is enabled. If not, then fallback to timedatectl.
 */

// self
//
#include    "server.h"


// eventdispatcher
//
#include    <eventdispatcher/signal_child.h>
#include    <eventdispatcher/timer.h>


// cppprocess
//
#include    <cppprocess/process.h>



namespace communicator_daemon
{



enum process_state_t
{
    PROCESS_STATE_IDLE,
    PROCESS_STATE_NTP_WAIT,
    PROCESS_STATE_TIMEDATE_WAIT,
};


class stable_clock
    : public ed::timer
{
public:
    typedef std::shared_ptr<stable_clock>   pointer_t;
    typedef std::weak_ptr<stable_clock>     weak_pointer_t;

                            stable_clock(server::pointer_t cs);

    // implement ed::timer
    virtual void            process_timeout() override;

private:
    bool                    has_ntp_wait() const;
    void                    start_ntp_wait();
    bool                    ntp_wait_exited(
                                  ed::child_status const & status
                                , cppprocess::process::pointer_t p);

    bool                    has_timedatectl() const;
    void                    start_timedate_wait();
    bool                    timedate_wait_exited(
                                  ed::child_status const & status
                                , cppprocess::process::pointer_t p);

    server::pointer_t       f_server = server::pointer_t();
    process_state_t         f_process_state = process_state_t::PROCESS_STATE_IDLE;
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

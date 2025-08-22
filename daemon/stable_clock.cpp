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
 * \brief Implementation of the clock verification process.
 *
 * \todo
 * At this time, we try once for 1 minute. If that fails, the clock will
 * permanently be considered unstable. We want to look at a way to try
 * again and again. Also, we could look at firing a process that would
 * try to restart ntp if installed and crashed.
 */

// self
//
#include    "stable_clock.h"

// communicatord
//
#include    <communicatord/exception.h>


// cppprocess
//
#include    <cppprocess/io_capture_pipe.h>


// snapdev
//
#include    <snapdev/safe_object.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{



namespace
{



constexpr char const * const    g_ntp_wait_command = "/usr/sbin/ntp-wait";
constexpr char const * const    g_timedatectl_command = "/usr/bin/timedatectl";
constexpr char const * const    g_timedate_wait_command = "/usr/bin/timedate-wait"; // this is our script (see under tools)


void reset_state(process_state_t * state)
{
    *state = process_state_t::PROCESS_STATE_IDLE;
}



}
// no name namespace


/** \class stable_clock
 * \brief Verify that the clock is ready.
 *
 * This class starts a background process to run one of the ntp-wait or
 * timedate-wait commands. These commands are used to know whether the
 * clock is up to date ("stable").
 *
 * If the NTP package is not installed, the system tries with the
 * timedatectl instead. If everything is in good shape (the command exits
 * with 0), then the server is told to send a CLOCK_STATUS message with a
 * "clock_stable" status.
 *
 * If there is a problem with the clock, then the server is asked to
 * send the CLOCK_NOT_READY message instead. The server has a third
 * state which is: CLOCK_UNKNOWN_STATE. A service can check the state
 * of the clock using the CLOCK_STATE message, which it will generally
 * do once it received the READY message and wants to make sure that
 * the clock is valid before starting its processing.
 */



/** \brief The clock verification process initialization.
 *
 * The constructor creates a runner and a thread and then start the thread.
 * The runner is in charge of everything after that.
 *
 * \param[in] cs  The communicator server we are listening for.
 */
stable_clock::stable_clock(server::pointer_t cs)
    : timer(60LL * 60LL * 1'000'000LL)  // wake up every hour, in microseconds
    , f_server(cs)
{
    // get a first tick immediately (once the run() loop starts)
    //
    set_timeout_date(snapdev::now());
}


bool stable_clock::has_ntp_wait() const
{
    struct stat s;
    if(stat(g_ntp_wait_command, &s) == 0)
    {
        return true;
    }

    if(errno != ENOENT)
    {
        int const e(errno);
        SNAP_LOG_ERROR
            << "stat() of \""
            << g_ntp_wait_command
            << "\" failed with error: "
            << e
            << ", "
            << strerror(e)
            << "."
            << SNAP_LOG_SEND;
    }

    return false;
}


void stable_clock::start_ntp_wait()
{
    // make sure that on any error the state gets reset to IDLE
    //
    snapdev::safe_object<process_state_t *, reset_state> safe_state;
    safe_state.make_safe(&f_process_state);
    f_process_state = process_state_t::PROCESS_STATE_NTP_WAIT;

    // run ntp-wait until NTP is started and the time was adjusted
    // here we wait for up to 600 seconds (10 min.) and check the
    // status once per second
    //
    cppprocess::process::pointer_t p(
            std::make_shared<cppprocess::process>("check ntp service status"));
    p->set_command(g_ntp_wait_command);
    p->add_argument("--tries=600");
    p->add_argument("--sleep=1");

    cppprocess::io_capture_pipe::pointer_t output_pipe(std::make_shared<cppprocess::io_capture_pipe>());
    p->set_output_io(output_pipe);

    cppprocess::io_capture_pipe::pointer_t error_pipe(std::make_shared<cppprocess::io_capture_pipe>());
    p->set_error_io(error_pipe);

    int const r(p->start());
    if(r != 0)
    {
        SNAP_LOG_ERROR
            << "process \""
            << p->get_command_line()
            << "\" failed to start."
            << SNAP_LOG_SEND;
        return;
    }

    ed::connection * c(dynamic_cast<ed::connection *>(this));
    if(c == nullptr)
    {
        throw communicatord::logic_error("the stable_clock class must be used with a connection class."); // LCOV_EXCL_LINE
    }

    ed::signal_child::pointer_t child_signal(ed::signal_child::get_instance());
    child_signal->add_listener(
              p->process_pid()
            , std::bind(
                      &stable_clock::ntp_wait_exited
                    //, std::dynamic_pointer_cast<stable_clock>(c->shared_from_this())
                    , std::dynamic_pointer_cast<stable_clock>(shared_from_this())
                    , std::placeholders::_1
                    , p));

    // it worked, keep the state as it was on entry
    //
    safe_state.release();
}


bool stable_clock::ntp_wait_exited(
      ed::child_status const & status
    , cppprocess::process::pointer_t p)
{
    f_process_state = process_state_t::PROCESS_STATE_IDLE;

    int const r(p->get_result(status));

    clock_status_t s(r == 0
                ? clock_status_t::CLOCK_STATUS_STABLE
                : clock_status_t::CLOCK_STATUS_INVALID);
    f_server->set_clock_status(s);

    return true;
}


bool stable_clock::has_timedatectl() const
{
    struct stat s;
    if(stat(g_timedatectl_command, &s) == 0)
    {
        return true;
    }

    if(errno != ENOENT)
    {
        int const e(errno);
        SNAP_LOG_ERROR
            << "stat() of \""
            << g_timedatectl_command
            << "\" failed with error: "
            << e
            << ", "
            << strerror(e)
            << "."
            << SNAP_LOG_SEND;
    }

    return false;
}


void stable_clock::start_timedate_wait()
{
    // make sure that on any error the state gets reset to IDLE
    //
    snapdev::safe_object<process_state_t *, reset_state> safe_state;
    safe_state.make_safe(&f_process_state);
    f_process_state = process_state_t::PROCESS_STATE_TIMEDATE_WAIT;

    // run timedate-wait until NTP is started and the time was adjusted
    // here we wait for up to 600 seconds (10 min.) and check the
    // status once per second
    //
    cppprocess::process::pointer_t systemctl_process(
            std::make_shared<cppprocess::process>("check systemd time service status"));
    systemctl_process->set_command(g_timedate_wait_command);
    systemctl_process->add_argument("--tries=600");
    systemctl_process->add_argument("--sleep=1");

    cppprocess::io_capture_pipe::pointer_t output_pipe(std::make_shared<cppprocess::io_capture_pipe>());
    systemctl_process->set_output_io(output_pipe);

    cppprocess::io_capture_pipe::pointer_t error_pipe(std::make_shared<cppprocess::io_capture_pipe>());
    systemctl_process->set_error_io(error_pipe);

    int const r(systemctl_process->start());
    if(r != 0)
    {
        SNAP_LOG_ERROR
            << "process \""
            << systemctl_process->get_command_line()
            << "\" failed to start."
            << SNAP_LOG_SEND;
        return;
    }

    ed::connection * c(dynamic_cast<ed::connection *>(this));
    if(c == nullptr)
    {
        throw communicatord::logic_error("the stable_clock class must be used with a connection class."); // LCOV_EXCL_LINE
    }

    ed::signal_child::pointer_t child_signal(ed::signal_child::get_instance());
    child_signal->add_listener(
              systemctl_process->process_pid()
            , std::bind(
                      &stable_clock::timedate_wait_exited
                    //, std::dynamic_pointer_cast<stable_clock>(c->shared_from_this())
                    , std::dynamic_pointer_cast<stable_clock>(shared_from_this())
                    , std::placeholders::_1
                    , systemctl_process));

    // it worked, keep the state as it was on entry
    //
    safe_state.release();
}


bool stable_clock::timedate_wait_exited(
      ed::child_status const & status
    , cppprocess::process::pointer_t p)
{
    f_process_state = process_state_t::PROCESS_STATE_IDLE;

    int const r(p->get_result(status));

    clock_status_t s(r == 0
                ? clock_status_t::CLOCK_STATUS_STABLE
                : clock_status_t::CLOCK_STATUS_INVALID);
    f_server->set_clock_status(s);

    return true;
}


void stable_clock::process_timeout()
{
    // our timer is 1h and the process should take under 10 minutes, so here
    // the process state should really always be IDLE
    //
    if(f_process_state != process_state_t::PROCESS_STATE_IDLE)
    {
        // still running
        //
        return;
    }

    if(has_ntp_wait())
    {
        start_ntp_wait();
    }
    else if(has_timedatectl())
    {
        start_timedate_wait();
    }
    else
    {
        // we did not find a way to check the clock,
        // tell the user we do not have an NTP service
        //
        f_server->set_clock_status(clock_status_t::CLOCK_STATUS_NO_NTP);
    }
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

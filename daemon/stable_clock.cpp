// Copyright (c) 2011-2023  Made to Order Software Corp.  All Rights Reserved
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
 */

// self
//
#include    "stable_clock.h"


// cppthread
//
#include    <cppthread/guard.h>
#include    <cppthread/mutex.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{
namespace detail
{



class ntp_wait
    : public cppthread::runner
{
public:
    typedef std::shared_ptr<ntp_wait>   pointer_t;

                        ntp_wait(stable_clock::pointer_t c);

    clock_status_t      get_status() const;

    // implementation of runner
    //
    virtual void        run() override;

private:
    stable_clock::weak_pointer_t    f_clock = stable_clock::weak_pointer_t();
    mutable cppthread::mutex        f_mutex = cppthread::mutex();
    clock_status_t                  f_status = clock_status_t::CLOCK_STATUS_UNKNOWN;
};


ntp_wait::ntp_wait(stable_clock::pointer_t c)
    : runner("ntp-wait")
    , f_clock(c)
{
}


clock_status_t ntp_wait::get_status() const
{
    cppthread::guard lock(f_mutex);
    return f_status;
}


void ntp_wait::run()
{
    int r(0);

    struct stat s;
    if(stat("/usr/sbin/ntp-wait", &s) != 0)
    {
        if(errno != ENOENT)
        {
            int const e(errno);
            SNAP_LOG_ERROR
                << "stat() of /usr/sbin/ntp-wait failed with error: "
                << e
                << ", "
                << strerror(e)
                << "."
                << SNAP_LOG_SEND;
            // continue -- pretend the clock is okay...
        }
    }
    else
    {
        // TODO: use our cppprocess and save the output (if any) as logs
        //
        r = system("/usr/sbin/ntp-wait --tries=600 --sleep=1");
    }

    {
        cppthread::guard lock(f_mutex);
        f_status = r == 0
                    ? clock_status_t::CLOCK_STATUS_STABLE
                    : clock_status_t::CLOCK_STATUS_INVALID;
    }

    stable_clock::pointer_t clock(f_clock.lock());
    if(clock != nullptr)
    {
        clock->thread_done();
    }
}



} // namespace detail



/** \class stable_clock
 * \brief Verify that the clock is ready.
 *
 * This class starts a background thread to run the ntp-wait command.
 * This command is used to know whether the clock is up to date.
 *
 * If NTP package is not installed or everything is in good shape (the
 * ntp-wait exits with 0), then the server is told to send a CLOCK_STATUS
 * message with a "clock_stable" status.
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
    : f_server(cs)
    , f_runner(std::make_shared<detail::ntp_wait>(std::dynamic_pointer_cast<stable_clock>(shared_from_this())))
    , f_thread(std::make_shared<cppthread::thread>("stable-clock", f_runner))
{
    f_thread->start();
}


void stable_clock::process_read()
{
    f_server->set_clock_status(f_runner->get_status());
    remove_from_communicator();
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

// Copyright (c) 2011-2026  Made to Order Software Corp.  All Rights Reserved.
//
// https://snapwebsites.org/project/sitter
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

// communicatord/check_clock
//
#include    "stable_clock.h"


// eventdispatcher
//
#include    <eventdispatcher/dispatcher.h>


// serverplugins
//
#include    <serverplugins/factory.h>
#include    <serverplugins/plugin.h>



namespace communicator_daemon
{
namespace check_clock
{



enum clock_status_t
{
    CLOCK_STATUS_UNKNOWN,       // i.e. we did not yet receive an answer from ntp-wait or timedate-wait

    CLOCK_STATUS_NO_NTP,
    CLOCK_STATUS_STABLE,
    CLOCK_STATUS_INVALID,
};


SERVERPLUGINS_VERSION(check_clock, 1, 0)


class check_clock
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(check_clock);

    // serverplugins::plugin implementation
    //
    virtual void                    bootstrap() override;

    // signals
    //
    void                            on_initialize(advgetopt::getopt & opts);
    void                            on_terminate();

    void                            set_clock_status(clock_status_t status);
    void                            send_clock_status(ed::connection::pointer_t reply_connection);

    void                            msg_clock_status(ed::message & msg);

private:
    ed::communicator::pointer_t     f_communicator = ed::communicator::pointer_t();
    stable_clock::pointer_t         f_check_clock_status = stable_clock::pointer_t();
    ed::dispatcher_match::tag_t     f_tag = ed::dispatcher_match::DISPATCHER_MATCH_NO_TAG;
    advgetopt::string_set_t         f_registered_neighbors_for_loadavg = advgetopt::string_set_t();
    clock_status_t                  f_clock_status = CLOCK_STATUS_UNKNOWN;
};



} // namespace check_clock
} // namespace communicator_daemon
// vim: ts=4 sw=4 et

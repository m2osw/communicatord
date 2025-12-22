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

// communicatord/loadavg
//
#include    "load_timer.h"


// serverplugins
//
#include    <serverplugins/plugin.h>



namespace communicator_daemon
{
namespace apt
{



SERVERPLUGINS_VERSION(loadavg, 1, 0)


class loadavg
    : public serverplugins::plugin
{
public:
    SERVERPLUGINS_DEFAULTS(loadavg);

    // serverplugins::plugin implementation
    //
    virtual void                    bootstrap() override;

    // server signal
    //
    void                            on_initilization(server * s);
    void                            on_terminate(server * s);

    void                            process_load_balancing();

private:
    load_timer::pointer_t           f_loadavg_timer = load_timer::pointer_t();    // a 1 second timer to calculate load (used to load balance)
    ed::dispatcher_match::tag_t     f_tag = ed::dispatcher_match::DISPATCHER_MATCH_NO_TAG;
    advgetopt::string_set_t         f_registered_neighbors_for_loadavg = advgetopt::string_set_t();
};



} // namespace loadavg
} // namespace communicator_daemon
// vim: ts=4 sw=4 et

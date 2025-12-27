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
#pragma once

/** \file
 * \brief Definition of the interrupt implementation class.
 *
 * The interrupt object is used to catch the SIGINT (Ctrl-C) Unix signals
 * so we can cleanup stop the daemon.
 */

// self
//
#include    "communicatord.h"


// eventdispatcher
//
#include    <eventdispatcher/signal.h>



namespace communicator_daemon
{



class interrupt
    : public ed::signal
{
public:
    typedef std::shared_ptr<interrupt>     pointer_t;

                        interrupt(communicatord * s);
                        interrupt(interrupt const &) = delete;
    virtual             ~interrupt() override {}

    interrupt           operator = (interrupt const &) = delete;

    // ed::signal implementation
    //
    virtual void        process_signal() override;

private:
    communicatord *     f_server = nullptr;
};



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/snapcommunicatord
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
 * \brief Declaration of the message cache facility.
 *
 * The Snap! Communicator is able to memorize messages it receives when the
 * destination is not yet available. The class here is used to manage that
 * cache.
 */

// eventdispatcher
//
#include    <eventdispatcher/message.h>


// C++
//
#include    <functional>
#include    <list>



namespace scd
{



class cache
{
public:
    void                cache_message(ed::message & msg);
    void                remove_old_messages();
    void                process_messages(std::function<bool(ed::message & msg)> callback);

private:
    class message_cache
    {
    public:
        typedef std::list<message_cache>  list_t;

        time_t              f_timeout_timestamp = 0;            // when that message is to be removed from the cache even if it wasn't sent to its destination
        ed::message         f_message = ed::message();          // the message
    };

    message_cache::list_t
                        f_message_cache = message_cache::list_t();
};



} // namespace scd
// vim: ts=4 sw=4 et

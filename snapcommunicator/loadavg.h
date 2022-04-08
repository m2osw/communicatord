// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
//
// https://snapwebsites.org/project/snapcommunicator
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

// C++ lib
//
#include    <string>
#include    <vector>


// C lib
//
#include    <arpa/inet.h>



namespace sc
{


// the sequential file is an array of these items
//
struct loadavg_item
{
    typedef std::vector<loadavg_item>       vector_t;

    int64_t                     f_timestamp = 0;
    struct sockaddr_in6         f_address = sockaddr_in6();
    float                       f_avg = 0.0f;
};



class loadavg_file
{
public:
    bool                        load();
    bool                        save() const;

    void                        add(loadavg_item const & item);
    bool                        remove_old_entries(int how_old);
    loadavg_item const *        find(struct sockaddr_in6 const & addr) const;
    loadavg_item const *        find_least_busy() const;

private:
    loadavg_item::vector_t      f_items = loadavg_item::vector_t();
};


void        set_loadavg_path(std::string const & path);
std::string get_loadavg_path();


} // namespace sc
// vim: ts=4 sw=4 et

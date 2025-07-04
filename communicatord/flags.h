// Copyright (c) 2013-2025  Made to Order Software Corp.  All Rights Reserved.
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

// libexcept
//
#include    <libexcept/exception.h>


// C++
//
#include    <memory>
#include    <set>
#include    <source_location>


namespace communicatord
{



typedef int                         priority_t;

constexpr priority_t const          DEFAULT_PRIORITY = 5;



class flag
{
public:
    typedef std::shared_ptr<flag>               pointer_t;
    typedef std::list<pointer_t>                list_t;

    typedef std::set<std::string>               tag_list_t;

    static constexpr std::size_t                FLAGS_LIMIT = 100;

    enum class state_t
    {
        STATE_UP,       // something is in error
        STATE_DOWN      // delete error file
    };

                                flag(
                                      std::string const & unit
                                    , std::string const & section
                                    , std::string const & name
                                    , std::source_location const & location = std::source_location::current());
                                flag(std::string const & filename);

    flag &                      set_from_raise_flag(); // only raise-flag tool should call this
    flag &                      set_state(state_t state);
    flag &                      set_source_file(std::string const & source_file);
    flag &                      set_function(std::string const & function);
    flag &                      set_line(std::uint_least32_t line);
    flag &                      set_column(std::uint_least32_t line);
    flag &                      set_message(std::string const & message);
    flag &                      set_priority(priority_t priority);
    flag &                      set_manual_down(bool manual);
    flag &                      add_tag(std::string const & tag);

    std::string const &         get_filename() const;

    state_t                     get_state() const;
    std::string const &         get_unit() const;
    std::string const &         get_section() const;
    std::string const &         get_name() const;
    std::string const &         get_source_file() const;
    std::string const &         get_function() const;
    std::uint_least32_t         get_line() const;
    std::uint_least32_t         get_column() const;
    std::string const &         get_message() const;
    priority_t                  get_priority() const;
    bool                        get_manual_down() const;
    time_t                      get_date() const;
    time_t                      get_modified() const;
    tag_list_t const &          get_tags() const;
    std::string const &         get_hostname() const;
    int                         get_count() const;
    std::string const &         get_version() const;
    std::string                 to_string() const;

    bool                        save();

    static list_t               load_flags();

private:
    static void                 valid_name(std::string & name);
    bool                        remove(std::string const & filename);

    state_t                     f_state             = state_t::STATE_UP;
    std::string                 f_unit              = std::string();
    std::string                 f_section           = std::string();
    std::string                 f_name              = std::string();
    mutable std::string         f_filename          = std::string();
    std::string                 f_source_file       = std::string();
    std::string                 f_function          = std::string();
    std::uint_least32_t         f_line              = 0;
    std::uint_least32_t         f_column            = 0;
    std::string                 f_message           = std::string();
    priority_t                  f_priority          = DEFAULT_PRIORITY;
    bool                        f_manual_down       = false;
    bool                        f_from_raise_flag   = false;
    time_t                      f_date              = -1;
    time_t                      f_modified          = -1;
    tag_list_t                  f_tags              = tag_list_t();
    std::string                 f_hostname          = std::string();
    int                         f_count             = 0;
    std::string                 f_version           = std::string();
};




#define COMMUNICATORD_FLAG_UP(unit, section, name, message)   \
            std::make_shared<communicatord::flag>( \
                communicatord::flag(unit, section, name) \
                    .set_message(message))

#define COMMUNICATORD_FLAG_DOWN(unit, section, name) \
            std::make_shared<communicatord::flag>( \
                communicatord::flag(unit, section, name) \
                    .set_state(communicatord::flag::state_t::STATE_DOWN))



} // namespace communicatord
// vim: ts=4 sw=4 et

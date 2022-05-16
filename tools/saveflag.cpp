// Copyright (c) 2011-2022  Made to Order Software Corp.  All Rights Reserved
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

// communicatord
//
#include    <communicatord/flags.h>
#include    <communicatord/version.h>


// snapdev
//
#include    <snapdev/tokenize_string.h>


// snaplogger
//
#include    <snaplogger/options.h>
#include    <snaplogger/message.h>


// advgetopt
//
#include    <advgetopt/advgetopt.h>
#include    <advgetopt/validator_integer.h>
#include    <advgetopt/exception.h>


// boost
//
#include    <boost/preprocessor/stringize.hpp>


// C
//
#include    <pwd.h>
#include    <string.h>


// last include
//
#include    <snapdev/poison.h>



namespace
{





advgetopt::option const g_options[] =
{
    advgetopt::define_option(
          advgetopt::Name("unit")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("name of the unit that is raising this flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("section")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("name of the specific section raising this flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("name")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("name of the flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("state")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("state of the flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("source")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("filename with the source.")
    ),
    advgetopt::define_option(
          advgetopt::Name("function")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("name of function raising the flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("line")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("line in source file.")
    ),
    advgetopt::define_option(
          advgetopt::Name("message")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("message describing the reason for raising this flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("priority")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("flag priority.")
    ),
    advgetopt::define_option(
          advgetopt::Name("manual-down")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("whether the flag will have to be taken down manually.")
    ),
    advgetopt::define_option(
          advgetopt::Name("tags")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("a comma separated list of tags.")
    ),
    advgetopt::define_option(
          advgetopt::Name("owner")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("communicatord")
        , advgetopt::Help("the name of the user running communicatord.")
    ),
    advgetopt::end_options()
};








// until we have C++20 remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "communicatord",
    .f_group_name = "communicatord",
    .f_options = g_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = nullptr,
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = nullptr,
    .f_configuration_directories = nullptr,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = COMMUNICATORD_VERSION_STRING,
    .f_license = "GNU GPL v3",
    .f_copyright = "Copyright (c) 2011-"
                   BOOST_PP_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    //.f_build_date = UTC_BUILD_DATE,
    //.f_build_time = UTC_BUILD_TIME
};
#pragma GCC diagnostic pop







class save_flag
{
public:
                                save_flag(int argc, char * argv[]);

    int                         save();

private:
    int                         switch_user();

    advgetopt::getopt           f_opts;
    communicatord::flag::pointer_t
                                f_flag = communicatord::flag::pointer_t();
};


save_flag::save_flag(int argc, char * argv[])
    : f_opts(g_options_environment)
{
    snaplogger::add_logger_options(f_opts);
    f_opts.finish_parsing(argc, argv);
    if(!snaplogger::process_logger_options(f_opts, "/etc/communicatord/logger"))
    {
        // exit on any error
        throw advgetopt::getopt_exit("logger options generated an error.", 1);
    }
}


int save_flag::save()
{
    if(!f_opts.is_defined("unit")
    || !f_opts.is_defined("section")
    || !f_opts.is_defined("name"))
    {
        SNAP_LOG_FATAL
            << "the --unit, --section, and --name are mandatory options."
            << SNAP_LOG_SEND;
        return 1;
    }

    f_flag = std::make_shared<communicatord::flag>(
          f_opts.get_string("unit")
        , f_opts.get_string("section")
        , f_opts.get_string("name"));

    if(f_opts.is_defined("state"))
    {
        if(f_opts.get_string("state") == "DOWN")
        {
            f_flag->set_state(communicatord::flag::state_t::STATE_DOWN);
        }
        else if(f_opts.get_string("state") == "UP")
        {
            f_flag->set_state(communicatord::flag::state_t::STATE_UP);
        }
        else
        {
            SNAP_LOG_FATAL
                << "the --state can be set to UP or DOWN only."
                << SNAP_LOG_SEND;
            return 1;
        }
    }

    if(f_opts.is_defined("source"))
    {
        f_flag->set_source_file(f_opts.get_string("source"));
    }

    if(f_opts.is_defined("function"))
    {
        f_flag->set_function(f_opts.get_string("function"));
    }

    if(f_opts.is_defined("line"))
    {
        std::string const line(f_opts.get_string("line"));
        std::int64_t l(0);
        advgetopt::validator_integer::convert_string(line, l);
        f_flag->set_line(static_cast<int>(l));
    }

    if(f_opts.is_defined("message"))
    {
        f_flag->set_message(f_opts.get_string("message"));
    }

    if(f_opts.is_defined("priority"))
    {
        std::string const priority(f_opts.get_string("priority"));
        std::int64_t p(0);
        advgetopt::validator_integer::convert_string(priority, p);
        f_flag->set_priority(static_cast<int>(p));
    }

    if(f_opts.is_defined("manual-down"))
    {
        std::string const manual_down(f_opts.get_string("manual-down"));
        if(manual_down == "true")
        {
            f_flag->set_priority(true);
        }
        else if(manual_down == "false")
        {
            f_flag->set_priority(false);
        }
        else
        {
            SNAP_LOG_FATAL
                << "the --manual-down must be followed by either \"true\" or \"false\"."
                << SNAP_LOG_SEND;
            return 1;
        }
    }

    if(f_opts.is_defined("tags"))
    {
        std::string const tags(f_opts.get_string("tags"));
        std::vector<std::string> tag_list;
        snapdev::tokenize_string(tag_list
                      , tags
                      , ","
                      , true
                      , " \n\r\t");
        for(auto const & t : tag_list)
        {
            f_flag->add_tag(t);
        }
    }

    // become communicatord so we can save the file as expected
    //
    if(switch_user() != 0)
    {
        return 1;
    }

    f_flag->save();

    return 0;
}


int save_flag::switch_user()
{
    std::string const owner(f_opts.get_string("owner"));
    passwd const * pswd(getpwnam(owner.c_str()));
    if(pswd == nullptr)
    {
        SNAP_LOG_FATAL
            << "Cannot locate user \""
            << owner
            << "\". Create it first then run the command again."
            << SNAP_LOG_SEND;
        return 1;
    }

    if(seteuid(pswd->pw_uid) != 0)
    {
        int const e(errno);
        SNAP_LOG_FATAL
            << "Cannot drop privileges to user \""
            << owner
            << "\". Create it first then run the command again. errno: "
            << e
            << ", "
            << strerror(e)
            << SNAP_LOG_SEND;
        return 1;
    }

    return 0;
}




}
// no name namespace


int main(int argc, char * argv[])
{
    try
    {
        save_flag sf(argc, argv);
        return sf.save();
    }
    catch(advgetopt::getopt_exit const & e)
    {
        return e.code();
    }
    catch(std::exception const & e)
    {
        // clean error on exception
        std::cerr << "saveflag: an exception occurred: " << e.what() << '\n';
        return 1;
    }
    catch(...)
    {
        std::cerr << "saveflag: an unknown exception occurred.\n";
        return 1;
    }
}


// vim: ts=4 sw=4 et

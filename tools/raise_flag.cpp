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

// communicator
//
#include    <communicator/flags.h>
#include    <communicator/version.h>


// as2js
//
#include    <as2js/json.h>


// edhttp
//
#include    <edhttp/http_date.h>


// snapdev
//
#include    <snapdev/as_root.h>
#include    <snapdev/join_strings.h>
#include    <snapdev/stringize.h>
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


// C++
//
#include    <iomanip>


// C
//
#include    <pwd.h>
#include    <string.h>


// last include
//
#include    <snapdev/poison.h>



namespace
{


constexpr char const * const g_tag_separators[] =
{
    " ",
    ",",

    nullptr
};


advgetopt::option const g_command_line_options[] =
{
    // COMMANDS
    advgetopt::define_option(
          advgetopt::Name("count")
        , advgetopt::ShortName(U'c')
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_GROUP_COMMANDS>())
        , advgetopt::Help("print the number of raised flags in stdout.")
    ),
    advgetopt::define_option(
          advgetopt::Name("up")
        , advgetopt::ShortName(U'u')
        , advgetopt::Flags(advgetopt::standalone_command_flags<
              advgetopt::GETOPT_FLAG_GROUP_COMMANDS>())
        , advgetopt::Help("raise the flag (default if no other command is specified).")
    ),
    advgetopt::define_option(
          advgetopt::Name("down")
        , advgetopt::ShortName(U'd')
        , advgetopt::Flags(advgetopt::standalone_command_flags<
              advgetopt::GETOPT_FLAG_GROUP_COMMANDS>())
        , advgetopt::Help("lower the flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("list")
        , advgetopt::ShortName(U'l')
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_COMMANDS>())
        , advgetopt::DefaultValue("text")
        , advgetopt::Help("list the currently raised flags in plain \"text\" or \"json\".")
    ),
    advgetopt::define_option(
          advgetopt::Name("raised")
        , advgetopt::ShortName(U'r')
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_GROUP_COMMANDS>())
        , advgetopt::Help("check whether any flag is raised, exit with 1 if so, otherwise exit with 0.")
    ),

    // OPTIONS
    advgetopt::define_option(
          advgetopt::Name("automatic")
        , advgetopt::ShortName(U'a')
        , advgetopt::Flags(advgetopt::standalone_command_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("mark the flag has to be taken down automatically.")
    ),
    advgetopt::define_option(
          advgetopt::Name("function")
        , advgetopt::ShortName(U'f')
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("name of function raising the flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("line")
        , advgetopt::ShortName(U'n')
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("line in source file calling --function.")
    ),
    advgetopt::define_option(
          advgetopt::Name("manual")
        , advgetopt::ShortName(U'm')
        , advgetopt::Flags(advgetopt::standalone_command_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("mark the flag has to be taken down manually.")
    ),
    advgetopt::define_option(
          advgetopt::Name("user")
        , advgetopt::ShortName(U'o')
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("communicator")  // TODO: the default should be changeable
        , advgetopt::Help("the name of the user managing the flags at the specified location.")
    ),
    advgetopt::define_option(
          advgetopt::Name("group")
        , advgetopt::ShortName(U'g')
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("communicator")  // TODO: the default should be changeable
        , advgetopt::Help("the name of the group managing the flags at the specified location.")
    ),
    advgetopt::define_option(
          advgetopt::Name("priority")
        , advgetopt::ShortName(U'p')
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("flag priority.")
    ),
    advgetopt::define_option(
          advgetopt::Name("source-file")
        , advgetopt::ShortName(U's')
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("filename with the source raising this flag.")
    ),
    advgetopt::define_option(
          advgetopt::Name("tags")
        , advgetopt::ShortName(U't')
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_MULTIPLE
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("a comma separated list of tags.")
        , advgetopt::Separators(g_tag_separators)
    ),
    advgetopt::define_option(
          advgetopt::Name("--")
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_MULTIPLE
            , advgetopt::GETOPT_FLAG_DEFAULT_OPTION
            , advgetopt::GETOPT_FLAG_SHOW_USAGE_ON_ERROR
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("<unit> <section> <flag> [<message>]")
    ),
    advgetopt::end_options()
};


advgetopt::group_description const g_group_descriptions[] =
{
    advgetopt::define_group(
          advgetopt::GroupNumber(advgetopt::GETOPT_FLAG_GROUP_COMMANDS)
        , advgetopt::GroupName("command")
        , advgetopt::GroupDescription("Commands:")
    ),
    advgetopt::define_group(
          advgetopt::GroupNumber(advgetopt::GETOPT_FLAG_GROUP_OPTIONS)
        , advgetopt::GroupName("option")
        , advgetopt::GroupDescription("Options:")
    ),
    advgetopt::end_groups()
};


// until we have C++20 remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "communicator",
    .f_group_name = "communicatord",
    .f_options = g_command_line_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = nullptr,
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = nullptr,
    .f_configuration_directories = nullptr,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>] [<unit> <section> <flag> [<message>]]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = COMMUNICATOR_VERSION_STRING,
    .f_license = "GNU GPL v3",
    .f_copyright = "Copyright (c) 2018-"
                   SNAPDEV_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    .f_build_date = UTC_BUILD_DATE,
    .f_build_time = UTC_BUILD_TIME,
    .f_groups = g_group_descriptions
};
#pragma GCC diagnostic pop







class raise_flag
{
public:
                                raise_flag(int argc, char * argv[]);

    int                         save();

private:
    int                         down();
    int                         up();
    int                         build_flag();
    int                         switch_user();
    int                         count();
    int                         raised();
    int                         list_in_plain_text();
    int                         list_in_json();

    advgetopt::getopt           f_opts;
    communicator::flag::pointer_t
                                f_flag = communicator::flag::pointer_t();
    snapdev::as_root::pointer_t f_as_root = snapdev::as_root::pointer_t();
};


raise_flag::raise_flag(int argc, char * argv[])
    : f_opts(g_options_environment)
{
    snaplogger::add_logger_options(f_opts);
    f_opts.finish_parsing(argc, argv);

    // become `communicator` so we can save the flag file as expected
    // and also the logger works
    //
    if(switch_user() != 0)
    {
        // TODO: use the correct name, it may be changed to something other
        //       than communicator
        //
        throw advgetopt::getopt_exit("could not become `communicator` user.", 1);
    }

    if(!snaplogger::process_logger_options(
              f_opts
            , "/etc/communicator/logger"
            , std::cout
            , false))
    {
        // exit on any error
        throw advgetopt::getopt_exit("logger options generated an error.", 1);
    }
}


int raise_flag::save()
{
    {
        int const count(
              (f_opts.is_defined("down")   ? 1 : 0)
            + (f_opts.is_defined("list")   ? 1 : 0)
            + (f_opts.is_defined("count")  ? 1 : 0)
            + (f_opts.is_defined("raised") ? 1 : 0)
            + (f_opts.is_defined("up")     ? 1 : 0));
        if(count > 1)
        {
            SNAP_LOG_FATAL
                << "found more than one command; only one of --count, --down, --list, --raised, or --up can be specified."
                << SNAP_LOG_SEND;
            return 1;
        }
    }

    if(f_opts.is_defined("count"))
    {
        return count();
    }

    if(f_opts.is_defined("raised"))
    {
        return raised();
    }

    if(f_opts.is_defined("list"))
    {
        std::string const mode(f_opts.get_string("list"));
        if(mode == "text")
        {
            return list_in_plain_text();
        }
        else if(mode == "json")
        {
            return list_in_json();
        }
        SNAP_LOG_FATAL
            << "unknown output list mode \""
            << mode
            << "\"."
            << SNAP_LOG_SEND;
        return 1;
    }

    if(f_opts.is_defined("down"))
    {
        return down();
    }

    // up is the default so consider true even if not specified
    //if(f_opts.is_defined("up"))

    return up();
}


int raise_flag::down()
{
    if(f_opts.is_defined("automatic")
    || f_opts.is_defined("manual")
    || f_opts.is_defined("priority")
    || f_opts.is_defined("tags"))
    {
        SNAP_LOG_FATAL
            << "the --automatic, --manual, --priority, and --tags command line options are not compatible with --down."
            << SNAP_LOG_SEND;
        return 1;
    }

    int const name_count(f_opts.size("--"));
    if(name_count != 3
    && name_count != 4)
    {
        SNAP_LOG_FATAL
            << "--down expects 3 or 4 parameters: <unit> <section> <flag> [<message>]."
            << SNAP_LOG_SEND;
        return 1;
    }

    build_flag();
    f_flag->set_state(communicator::flag::state_t::STATE_DOWN);
    f_flag->save();

    return 0;
}


int raise_flag::up()
{
    int const name_count(f_opts.size("--"));
    if(name_count != 4)
    {
        SNAP_LOG_FATAL
            << "--up expects 4 parameters: <unit> <section> <flag> <message>."
            << SNAP_LOG_SEND;
        return 1;
    }

    build_flag();
    f_flag->set_state(communicator::flag::state_t::STATE_UP);
    f_flag->save();

    return 0;
}


int raise_flag::build_flag()
{
    f_flag = std::make_shared<communicator::flag>(
              f_opts.get_string("--", 0)    // unit
            , f_opts.get_string("--", 1)    // section
            , f_opts.get_string("--", 2));  // name
    f_flag->set_from_raise_flag();

    if(f_opts.size("--") == 4)
    {
        f_flag->set_message(f_opts.get_string("--", 3));
    }

    if(f_opts.is_defined("source-file"))
    {
        f_flag->set_source_file(f_opts.get_string("source-file"));
    }

    if(f_opts.is_defined("function"))
    {
        f_flag->set_function(f_opts.get_string("function"));
    }

    if(f_opts.is_defined("line"))
    {
        f_flag->set_line(f_opts.get_long("line", 0, 1));
    }

    if(f_opts.is_defined("priority"))
    {
        f_flag->set_priority(f_opts.get_long("priority", 0, 0, 100));
    }

    if(f_opts.is_defined("manual"))
    {
        if(f_opts.is_defined("automatic"))
        {
            SNAP_LOG_FATAL
                << "only one of --manual or --automatic is allowed;"
                   " default is --automatic when neither is specified."
                << SNAP_LOG_SEND;
            return 1;
        }
        f_flag->set_manual_down(true);
    }
    else if(f_opts.is_defined("automatic"))
    {
        f_flag->set_manual_down(false);
    }

    if(f_opts.is_defined("tags"))
    {
        std::size_t const max(f_opts.size("tags"));
        for(std::size_t idx(0); idx < max; ++idx)
        {
            f_flag->add_tag(f_opts.get_string("tags", idx));
        }
    }

    return 0;
}


int raise_flag::switch_user()
{
    std::string const communicator_user(f_opts.get_string("user"));
    std::string const communicator_group(f_opts.get_string("group"));
    f_as_root = std::make_shared<snapdev::as_root>(communicator_user, communicator_group);
    if(!f_as_root->is_switched())
    {
        SNAP_LOG_FATAL
            << "wrong user running raise-flag ("
            << getuid()
            << ':'
            << getgid()
            << ") and could not switch to \""
            << communicator_user
            << ':'
            << communicator_group
            << "\". Please verify that the executable permissions are properly set."
            << SNAP_LOG_SEND;
        return 1;
    }
    return 0;
}


int raise_flag::count()
{
    communicator::flag::list_t flags(communicator::flag::load_flags());
    std::cout << flags.size() << std::endl;
    return flags.empty() ? 0 : 1;
}


int raise_flag::raised()
{
    communicator::flag::list_t flags(communicator::flag::load_flags());
    return flags.empty() ? 0 : 1;
}


int raise_flag::list_in_plain_text()
{
    communicator::flag::list_t flags(communicator::flag::load_flags());

    std::map<std::string, std::string::size_type> widths;

    // note: the date width is static (Fri Aug 24, 2018  12:29:23)

    widths["unit"]        = strlen("unit");
    widths["section"]     = strlen("section");
    widths["name"]        = strlen("name");
    widths["count"]       = strlen("count");
    widths["source-file"] = strlen("source_file");
    widths["function"]    = strlen("function");
    widths["line"]        = strlen("line");
    widths["message"]     = strlen("message");
    widths["priority"]    = strlen("priority");
    widths["manual"]      = strlen("manual");
    widths["date"]        = std::max(strlen("date"), 31UL);
    widths["modified"]    = std::max(strlen("modified"), 31UL);
    widths["hostname"]    = strlen("hostname");
    widths["version"]     = strlen("version");
    widths["tags"]        = strlen("tags");

    // run a first time through the list so we can gather the width of each field
    //
    for(auto const & f : flags)
    {
        // TODO: std::string().length() is wrong for UTF8
        //
        widths["unit"]        = std::max(widths["unit"],        f->get_unit()                    .length());
        widths["section"]     = std::max(widths["section"],     f->get_section()                 .length());
        widths["name"]        = std::max(widths["name"],        f->get_name()                    .length());
        widths["count"]       = std::max(widths["count"],       std::to_string(f->get_count())   .length());
        widths["source-file"] = std::max(widths["source-file"], f->get_source_file()             .length());
        widths["function"]    = std::max(widths["function"],    f->get_function()                .length());
        widths["line"]        = std::max(widths["line"],        std::to_string(f->get_line())    .length());
        widths["message"]     = std::max(widths["message"],     f->get_message()                 .length());
        widths["priority"]    = std::max(widths["priority"],    std::to_string(f->get_priority()).length());
        widths["manual"]      = std::max(widths["manual"],      f->get_manual_down() ? strlen("yes") : strlen("no"));
        widths["hostname"]    = std::max(widths["hostname"],    f->get_hostname()                .length());
        widths["version"]     = std::max(widths["version"],     f->get_version()                 .length());

        // no need for the dates, they always fit in 31 chars.
        //widths["date"]        = std::max(widths["date"],        ...);
        //widths["modified"]    = std::max(widths["modified"],    ...);

        communicator::flag::tag_list_t const & tags(f->get_tags());
        std::string const tags_string(snapdev::join_strings(tags, ", "));
        widths["tags"] = std::max(widths["tags"], tags_string.length());
    }

    // now we have the widths so we can write the output mapped properly
    //
    std::cout << std::left
              << std::setw(widths["unit"]        + 2) << "unit"
              << std::setw(widths["section"]     + 2) << "section"
              << std::setw(widths["name"]        + 2) << "name"
              << std::setw(widths["count"]       + 2) << "count"
              << std::setw(widths["source-file"] + 2) << "source_file"
              << std::setw(widths["function"]    + 2) << "function"
              << std::setw(widths["line"]        + 2) << "line"
              << std::setw(widths["message"]     + 2) << "message"
              << std::setw(widths["priority"]    + 2) << "priority"
              << std::setw(widths["manual"]      + 2) << "manual"
              << std::setw(widths["date"]        + 2) << "date"
              << std::setw(widths["modified"]    + 2) << "modified"
              << std::setw(widths["hostname"]    + 2) << "hostname"
              << std::setw(widths["version"]     + 2) << "version"
              << std::setw(widths["tags"]           ) << "tags"
              << std::endl;

    std::cout << std::left
              << std::setw(widths["unit"]        + 2) << "----"
              << std::setw(widths["section"]     + 2) << "-------"
              << std::setw(widths["name"]        + 2) << "----"
              << std::setw(widths["count"]       + 2) << "-----"
              << std::setw(widths["source-file"] + 2) << "-----------"
              << std::setw(widths["function"]    + 2) << "--------"
              << std::setw(widths["line"]        + 2) << "----"
              << std::setw(widths["message"]     + 2) << "-------"
              << std::setw(widths["priority"]    + 2) << "--------"
              << std::setw(widths["manual"]      + 2) << "------"
              << std::setw(widths["date"]        + 2) << "----"
              << std::setw(widths["modified"]    + 2) << "--------"
              << std::setw(widths["hostname"]    + 2) << "--------"
              << std::setw(widths["version"]     + 2) << "-------"
              << std::setw(widths["tags"]           ) << "----"
              << std::endl;

    for(auto const & f : flags)
    {
        std::cout << std::left  << std::setw(widths["unit"]        + 2) << f->get_unit()
                  << std::left  << std::setw(widths["section"]     + 2) << f->get_section()
                  << std::left  << std::setw(widths["name"]        + 2) << f->get_name()
                  << std::right << std::setw(widths["count"]          ) << f->get_count() << "  "
                  << std::left  << std::setw(widths["source-file"] + 2) << f->get_source_file()
                  << std::left  << std::setw(widths["function"]    + 2) << f->get_function()
                  << std::right << std::setw(widths["line"]           ) << f->get_line() << "  "
                  << std::left  << std::setw(widths["message"]     + 2) << f->get_message()
                  << std::right << std::setw(widths["priority"]       ) << f->get_priority() << "  "
                  << std::left  << std::setw(widths["manual"]      + 2) << (f->get_manual_down() ? "yes" : "no")
                  << std::left  << std::setw(widths["date"]        + 2) << edhttp::date_to_string(f->get_date()     * 1000000, edhttp::date_format_t::DATE_FORMAT_HTTP)
                  << std::left  << std::setw(widths["modified"]    + 2) << edhttp::date_to_string(f->get_modified() * 1000000, edhttp::date_format_t::DATE_FORMAT_HTTP)
                  << std::left  << std::setw(widths["hostname"]    + 2) << f->get_hostname()
                  << std::left  << std::setw(widths["version"]     + 2) << f->get_version()
                  << std::left  << std::setw(widths["tags"]           ) << snapdev::join_strings(f->get_tags(), ", ")
                  << std::endl;
    }

    std::cout << "----------------------" << std::endl
              << "Found " << flags.size() << " raised flag" << (flags.size() == 1 ? "" : "s") << std::endl;

    return 0;
}



int raise_flag::list_in_json()
{
    as2js::json json;
    as2js::json::json_value_ref root(json["flags"]);

    communicator::flag::list_t flags(communicator::flag::load_flags());
    for(auto const & f : flags)
    {
        as2js::json::json_value_ref item(json["flag"][-1]);

        item["unit"] =        f->get_unit();
        item["section"] =     f->get_section();
        item["name"] =        f->get_name();
        item["source-file"] = f->get_source_file();
        item["function"] =    f->get_function();
        item["line"] =        f->get_line();
        item["message"] =     f->get_message();
        item["priority"] =    f->get_priority();
        item["manual"] =      f->get_manual_down();

        communicator::flag::tag_list_t const & tags(f->get_tags());
        if(!tags.empty())
        {
            as2js::json::json_value_ref tag_list(item["tags"][-1]);

            for(auto const & t : tags)
            {
                tag_list["tag"] = t;
            }
        }
    }

    std::cout << json.get_value()->to_string() << std::endl;

    return 0;
}



}
// no name namespace


int main(int argc, char * argv[])
{
    try
    {
        raise_flag sf(argc, argv);
        return sf.save();
    }
    catch(advgetopt::getopt_exit const & e)
    {
        return e.code();
    }
    catch(std::exception const & e)
    {
        // clean error on exception
        std::cerr << "raise-flag: an exception occurred: " << e.what() << '\n';
        SNAP_LOG_FATAL
            << "raise-flag: an exception occurred: "
            << e.what()
            << SNAP_LOG_SEND;
        return 1;
    }
    catch(...)
    {
        std::cerr << "raise-flag: an unknown exception occurred.\n";
        SNAP_LOG_FATAL
            << "raise-flag: an unknown exception occurred."
            << SNAP_LOG_SEND;
        return 1;
    }
}


// vim: ts=4 sw=4 et

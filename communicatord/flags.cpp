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

/** \file
 * \brief The `flag` class  is used to raise flags when an error occurs.
 *
 * On a server, some errors occur which need to be looked at. One issue,
 * in many cases, is that service A detects and error and either an
 * administrator or server B has to take care of it. Until then, the
 * error has to persist.
 *
 * This class handles such situations by creating a flag file. That file
 * can include all sorts of parameters such as tags and the exact
 * location of the process that created the error (i.e. line number,
 * function name, filename).
 */

// self
//
#include    <communicatord/flags.h>

#include    <communicatord/exception.h>
#include    <communicatord/names.h>
#include    <communicatord/version.h>


// advgetopt
//
#include    <advgetopt/conf_file.h>


// cppprocess
//
#include    <cppprocess/process.h>


// snaplogger
//
#include    <snaplogger/message.h>


// snapdev
//
#include    <snapdev/as_root.h>
#include    <snapdev/chownnm.h>
#include    <snapdev/gethostname.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/join_strings.h>
#include    <snapdev/mkdir_p.h>
#include    <snapdev/not_used.h>
#include    <snapdev/tokenize_string.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicatord
{


namespace
{


constexpr char const * const        g_communicatord_flags = "/etc/communicatord/flags.conf";


std::string get_config_param(std::string const & name, std::string const & default_value)
{
    // TODO: fix this load, we need to support a sub-directory
    //
    advgetopt::conf_file_setup setup(g_communicatord_flags);
    advgetopt::conf_file::pointer_t server_config(advgetopt::conf_file::get_conf_file(setup));
    if(server_config->has_parameter(name))
    {
        return server_config->get_parameter(name);
    }

    return default_value;
}


/** \brief Get the path where flag files are created.
 *
 * This function returns the path where we expect flag files to be created.
 * It has a default if the configuration file cannot be accessed.
 *
 * \todo
 * We need to have the full getopt() functionality to retrieve the correct
 * value (including sub-directories configuration files) instead of just
 * one conf_file.
 *
 * \return The path where to create flag files or an empty string if the
 *         directory does not exist.
 */
std::string get_path_to_flag_files()
{
    static std::string g_path_to_flag_files = std::string();

    if(g_path_to_flag_files.empty())
    {
        // get the path (once unless the directory does not exist)
        //
        std::string const path(get_config_param("path", "/var/lib/communicatord/flags"));

        // make sure the directory exists
        //
        struct stat s;
        int r(stat(path.c_str(), &s));
        if(r != 0)
        {
            if(errno != ENOENT)
            {
                int const e(errno);
                SNAP_LOG_ERROR
                    << "found a flags file \""
                    << path
                    // TODO: fix the message, we try to create it here!
                    << "\" but something failed: "
                    << e
                    << ", "
                    << strerror(e)
                    << SNAP_LOG_SEND;
                return std::string();
            }

            // try to create the directory if missing
            //
            r = snapdev::mkdir_p(
                  path.c_str()
                , false
                , 0775
                , "communicatord"   // TODO: make these names come from flags.conf
                , "communicatord");
            if(r != 0)
            {
                int const e(errno);
                SNAP_LOG_ERROR
                    << "could not create the flags directory \""
                    << path
                    << "\"; errno: "
                    << e
                    << ", "
                    << strerror(errno)
                    << SNAP_LOG_SEND;
                return std::string();
            }

            r = stat(path.c_str(), &s);
            if(r != 0)
            {
                SNAP_LOG_ERROR
                    << "could not find the flags directory \""
                    << path
                    << "\"; did you start communicatord yet? (it creates it if not yet present)"
                    << SNAP_LOG_SEND;
            }
        }

        if(!S_ISDIR(s.st_mode))
        {
            SNAP_LOG_ERROR
                << "the flags path \""
                << path
                << "\" is not a directory as expected."
                << SNAP_LOG_SEND;
            return std::string();
        }

        g_path_to_flag_files = path;
    }

    return g_path_to_flag_files;
}



}




/** \brief Initialize a "new" flag.
 *
 * This function creates a new flag in memory.
 *
 * New flags are generally created using one of the COMMUNICATORD_FLAG_UP()
 * or the COMMUNICATORD_FLAG_DOWN() macros, which will automatically
 * initialize the flag, especially the source filename, the function name,
 * and the line number where the flag is being created, and the status
 * which the macro describes.
 *
 * All the names must match the following macro:
 *
 * \code
 *      [a-zA-Z][-a-zA-Z0-9]*
 * \endcode
 *
 * The underscore is not included in a name because we want to be able to
 * separate multiple names using the underscore, which is what is used
 * when building the filename from this information.
 *
 * \param[in] unit  The name of the unit creating this flag. For example,
 *            the core plugins would use "core-plugin".
 * \param[in] section  The name of the section creating this flag. In case
 *            of the core plugin, this should be the name of the plugin.
 * \param[in] name  The actual name of the flag. This is expected to
 *            somewhat describe what the flag is being for.
 * \param[in] location  The location of the flag creation so we can quickly
 *            find that place and fix the issue if necessary/possible. The
 *            default is fine 99.99% of the time.
 */
flag::flag(
          std::string const & unit
        , std::string const & section
        , std::string const & name
        , std::source_location const & location)
    : f_unit(unit)
    , f_section(section)
    , f_name(name)
    , f_source_file(location.file_name())
    , f_function(location.function_name())
    , f_line(location.line())
    , f_column(location.column())
    , f_date(time(nullptr))
{
    valid_name(f_unit);
    valid_name(f_section);
    valid_name(f_name);
}


/** \brief Load a flag from file.
 *
 * When this constructor is used, the flag gets loaded from file.
 * Flags use a advgetopt::conf_file to handle their permanent data.
 *
 * The fields offered in the flag object are translated
 * in a configuration field. This function reads the data with
 * an advgetopt::conf_file object and converts it to flag data.
 *
 * \note
 * In this case, the get_filename() function returns \p filename
 * whether the file exists or not.
 *
 * \exception invalid_parameter
 * This exception is raised if the input \p filename is an empty string.
 * It also gets raised if the file is considered invalid (i.e. is missing
 * one of the mandatory fields: unit, section, name, message).
 *
 * \param[in] filename  The name of the file to load.
 */
flag::flag(std::string const & filename)
    : f_filename(filename)
{
    if(f_filename.empty())
    {
        throw invalid_parameter("the filename must be defined (i.e. not empty) when using the flag constructor with a filename");
    }

    advgetopt::conf_file_setup setup(get_filename());
    advgetopt::conf_file::pointer_t file(advgetopt::conf_file::get_conf_file(setup));

    if(!file->has_parameter(communicatord::g_name_communicatord_param_unit)
    || !file->has_parameter(communicatord::g_name_communicatord_param_section)
    || !file->has_parameter(communicatord::g_name_communicatord_param_name)
    || !file->has_parameter(communicatord::g_name_communicatord_param_message))
    {
        throw invalid_parameter("a flag file is expected to include a unit, section, and name field, along with a message field. Other fields are optional.");
    }

    f_unit = file->get_parameter(communicatord::g_name_communicatord_param_unit);
    f_section = file->get_parameter(communicatord::g_name_communicatord_param_section);
    f_name = file->get_parameter(communicatord::g_name_communicatord_param_name);

    if(file->has_parameter(communicatord::g_name_communicatord_param_source_file))
    {
        f_source_file = file->get_parameter(communicatord::g_name_communicatord_param_source_file);
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_function))
    {
        f_function = file->get_parameter(communicatord::g_name_communicatord_param_function);
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_line))
    {
        f_line = std::stol(file->get_parameter(communicatord::g_name_communicatord_param_line));
    }

    f_message = file->get_parameter(communicatord::g_name_communicatord_param_message);

    if(file->has_parameter(communicatord::g_name_communicatord_param_priority))
    {
        f_priority = std::stol(file->get_parameter(communicatord::g_name_communicatord_param_priority));
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_manual_down))
    {
        f_manual_down = file->get_parameter(communicatord::g_name_communicatord_param_manual_down)
                                                    == communicatord::g_name_communicatord_value_yes;
    }

    time_t const now(time(nullptr));
    if(file->has_parameter(communicatord::g_name_communicatord_param_date))
    {
        f_date = std::stol(file->get_parameter(communicatord::g_name_communicatord_param_date));
    }
    else
    {
        f_date = now;
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_modified))
    {
        f_modified = std::stol(file->get_parameter(communicatord::g_name_communicatord_param_modified));
    }
    else
    {
        f_modified = now;
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_tags))
    {
        // here we use an intermediate tag_list vector so the tokenize_string
        // works then add those string in the f_tags parameter
        //
        std::string const tags(file->get_parameter(communicatord::g_name_communicatord_param_tags));
        std::vector<std::string> tag_list;
        snapdev::tokenize_string(tag_list
                      , tags
                      , ","
                      , true
                      , " \n\r\t");
        f_tags.insert(tag_list.begin(), tag_list.end());
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_hostname))
    {
        f_hostname = file->get_parameter(communicatord::g_name_communicatord_param_hostname);
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_count))
    {
        f_count = std::stol(file->get_parameter(communicatord::g_name_communicatord_param_count));
    }

    if(file->has_parameter(communicatord::g_name_communicatord_param_version))
    {
        f_version = file->get_parameter(communicatord::g_name_communicatord_param_version);
    }
}


/** \brief Mark this flag as beeing set by the raise-flag tool.
 *
 * The raise-flag tool calls this function to avoid an infinite loop.
 * The save() function tries to create the files as "communicatord". If that
 * fails, it invokes the raise-flag tool instead. That invocation does not
 * happen if this very function was called before save() is called.
 *
 * \return a reference to this flag object.
 */
flag & flag::set_from_raise_flag()
{
    f_from_raise_flag = true;
    return *this;
}


/** \brief Set the state of the flag.
 *
 * At the moment, the flag can be UP or DOWN. By default it is UP meaning
 * that there is an error, something the administrator has to take care of
 * to make sure the system works as expected. For example, the antivirus
 * backend will detect that the clamav package is not installed and install
 * it as required.
 *
 * \param[in] state  The new state.
 *
 * \return A reference to this.
 */
flag & flag::set_state(state_t state)
{
    f_state = state;

    return *this;
}


/** \brief Set the name of the source file.
 *
 * The name of the source where the flag is being raised is added using
 * this function.
 *
 * \param[in] source_file  The source file name.
 *
 * \return A reference to this.
 */
flag & flag::set_source_file(std::string const & source_file)
{
    f_source_file = source_file;

    return *this;
}


/** \brief Set the name of the function raising the flag.
 *
 * For debug purposes, we save the name of the function that called
 * the manager function to save the function. It should help us,
 * long term, to find flags and maintain them as required.
 *
 * \param[in] function  The name of the concerned function.
 *
 * \return A reference to this.
 */
flag & flag::set_function(std::string const & function)
{
    f_function = function;

    return *this;
}


/** \brief Set the line number at which the event happened.
 *
 * This parameter is used to save the line at which the function
 * used one of the flag macros.
 *
 * By default the value is set to zero. If never called, then this
 * is a way to know that no line number was defined.
 *
 * \param[in] line  The new line number at which this flag is being raised.
 *
 * \return A reference to this.
 */
flag & flag::set_line(std::uint_least32_t line)
{
    f_line = line;

    return *this;
}


/** \brief Set the column number at which the event happened.
 *
 * This parameter is used to save the column at which the function
 * used one of the flag macros.
 *
 * By default the value is set to zero. If never called, then this
 * is a way to know that no column number was defined.
 *
 * \param[in] column  The new column number at which this flag is being raised.
 *
 * \return A reference to this.
 */
flag & flag::set_column(std::uint_least32_t column)
{
    f_column = column;

    return *this;
}


/** \brief Set the error message.
 *
 * A flag is always accompagned by an error message of some sort.
 * For example, the sendmail backend checks whether postfix is
 * installed on that computer. If not, it raises a flag with an
 * error message saying something like this: "The sendmail backend
 * expects Postfix to be installed on the same computer. snapmta
 * is not good enough to support the full mail server."
 *
 * \param[in] message  The message explaining why the flag is raised.
 *
 * \return A reference to this.
 */
flag & flag::set_message(std::string const & message)
{
    f_message = message;

    return *this;
}


/** \brief Set the error message.
 *
 * A flag is always accompagned by an error message of some sort.
 * For example, the sendmail backend checks whether postfix is
 * installed on that computer. If not, it raises a flag with an
 * error message saying something like this: "The sendmail backend
 * expects Postfix to be installed on the same computer. snapmta
 * is not good enough to support the full mail server."
 *
 * The default priority is 5. It can be reduced or increased. It
 * is expected to be between 0 and 100.
 *
 * \param[in] priority  The error priority.
 *
 * \return A reference to this.
 */
flag & flag::set_priority(int priority)
{
    if(priority < 0)
    {
        f_priority = 0;
    }
    else if(priority > 100)
    {
        f_priority = 100;
    }
    else
    {
        f_priority = priority;
    }

    return *this;
}


/** \brief Mark whether a manual down is required for this flag.
 *
 * Some flags may be turned ON but never turned OFF. These are called
 * _manual flags_, because you have to turn them off manually.
 *
 * \todo
 * At some point, the Watchdog interface in the snapmanager.cgi will
 * allow you to click a link to manually take a flag down.
 *
 * \param[in] manual  Whether the flag is considered manual or not.
 *
 * \return A reference to this.
 */
flag & flag::set_manual_down(bool manual)
{
    f_manual_down = manual;

    return *this;
}


/** \brief Add a tag to the list of tags of this flag.
 *
 * You can assign tags to a flag so as to group it with other flags
 * that reuse the same tag.
 *
 * The names must be valid names (as the unit, section, and flag names.)
 * Your name must validate against this regular expression:
 *
 * \code
 *      [a-zA-Z][-a-zA-Z0-9]*
 * \endcode
 *
 * So a-z, A-Z, 0-9, and dash. The first character must be a letter.
 *
 * Note that the underscore (_) is not included because we use those
 * to separate each word in a filename.
 *
 * \param[in] tag  The name of a tag to add to this flag.
 *
 * \return A reference to this.
 */
flag & flag::add_tag(std::string const & tag)
{
    f_tags.insert(tag);

    return *this;
}


/** \brief Get the current state.
 *
 * Get the state of the flag file.
 *
 * A flag file can be UP or DOWN. When DOWN, a save() will delete the
 * file. When UP, the file gets created.
 *
 * \return The current state of the flag.
 */
flag::state_t flag::get_state() const
{
    return f_state;
}


/** \brief Get the unit name.
 *
 * Flags are made unique by assigning them unit and section names.
 *
 * The unit name would be something such as "core-plugins" for the
 * main snapserver core plugins.
 *
 * \return The unit name.
 *
 * \sa get_section()
 * \sa get_name()
 */
std::string const & flag::get_unit() const
{
    return f_unit;
}


/** \brief Get the section name.
 *
 * The section name defines the part of the software that has a problem.
 * For example, for the core plugins, you may want to use the name of the
 * plugin.
 *
 * \return The name of the section raising the flag.
 *
 * \sa get_unit()
 * \sa get_name()
 */
std::string const & flag::get_section() const
{
    return f_section;
}


/** \brief Name of the flag.
 *
 * This parameter defines the name of the flag. The reason for the error
 * is often what is used here. The name must be short and ASCII only, but
 * should still properly define why the error occurs.
 *
 * A more detailed error message is returned by get_message().
 *
 * The get_unit() and get_section() define more generic names than this
 * one.
 *
 * For example, the attachment plugin checks for virus infected attachments.
 * This requires the clamav package to be installed. If not installed, it
 * raises a flag. That flag is part of the "core-plugins" (unit name),
 * and it gets raised in the "attachment" (section name) plugin, and it gets
 * raised because of "clamav-missing" (flag name)
 *
 * \return The name of the flag.
 *
 * \sa get_message()
 */
std::string const & flag::get_name() const
{
    return f_name;
}


/** \brief Retrieve the name of the source file.
 *
 * This function retrieves the source filename. This is set using the
 * macros. It helps finding the reason for the falg being raised if
 * the message is not clear enough.
 *
 * \return The source file name.
 */
std::string const & flag::get_source_file() const
{
    return f_source_file;
}


/** \brief Get the filename.
 *
 * This function returns the filename used to access this flag.
 *
 * If you loaded the flag files, then this is defined from the constructor.
 *
 * If you created a flag object from scratch, then the filename
 * is built from the unit, section, and flag names as follow:
 *
 * \code
 *      <unit> + '_' + <section> + '_' + <flag name> + ".flag"
 * \endcode
 *
 * \return The filename to use for this flag or empty on error.
 */
std::string const & flag::get_filename() const
{
    if(f_filename.empty())
    {
        f_filename = get_path_to_flag_files();
        if(!f_filename.empty())
        {
            f_filename += "/" + f_unit + "_" + f_section + "_" + f_name + ".flag";
        }
    }
    return f_filename;
}


/** \brief Retrieve the function name.
 *
 * The function name defines the name of the function where the macro
 * was used. It can be useful for debugging where aproblem happens.
 *
 * \return The name of the function we are that was tike
 */
std::string const & flag::get_function() const
{
    return f_function;
}


/** \brief Retrieve the line number at which it was first called.
 *
 * This is for debug purposes so one can easily find exactly what code
 * generated which flag.
 *
 * \return The line number where the flag object is created.
 */
std::uint_least32_t flag::get_line() const
{
    return f_line;
}


/** \brief Retrieve the column number at which it was first called.
 *
 * This is for debug purposes so one can easily find exactly what code
 * generated which flag.
 *
 * \return The column number where the flag object is created.
 */
std::uint_least32_t flag::get_column() const
{
    return f_column;
}


/** \brief The actual error message of this flag.
 *
 * A flag is used to tell the sitter flag plugin that something
 * is wrong and requires the administrator to fix it up.
 *
 * The message should be plain text. It may include newline characters.
 *
 * \return The message of this flag.
 */
std::string const & flag::get_message() const
{
    return f_message;
}


/** \brief Retrieve the flag priority.
 *
 * The function returns the priority of the flag. By default it is set to 5.
 * If you want to increase the priority so the error shows up in an email,
 * increase the priority to at least 50. Remember that a really high priority
 * (close to 100) will increase the number of emails. Watch out as it could
 * both the admninistrators.
 *
 * When displaying the flags, the highest priority is used and a single
 * message is sent for all the priorities.
 *
 * \return This flag priority.
 */
int flag::get_priority() const
{
    return f_priority;
}


/** \brief Check whether the flag is considered manual or automatic.
 *
 * A _manual down_ flag is a flag that the administrator has to turn
 * off manually once the problem was taken cared off.
 *
 * By default, a flag is considered automatic, which means
 * that the process that raises the flag up for some circumstances
 * will also know how to bring that flag down once the circumstances
 * disappear.
 *
 * This function returns true if the process never brings its
 * flag down automatically. This is particularly true if the process
 * use the COMMUNICATORD_FLAG_UP() macro but never uses the corresponding
 * COMMUNICATORD_FLAG_DOWN()--corresponding as in with the same first
 * three strings (unit, section, name).
 *
 * \return true if the flag has to be taken down (deleted) manually.
 *
 * \sa set_manual_down()
 */
bool flag::get_manual_down() const
{
    return f_manual_down;
}


/** \brief Retrieve the date when the flag was first raised.
 *
 * The function returns the date when the flag was first raised. A flag
 * that often goes up and down will have the date when it last went up.
 *
 * See get_modified() to get the date when the flag was last checked.
 * In some cases, checks are done once each time a command is run.
 * In other cases, checks are performed by the startup code of a
 * daemon so the modification date is likely to not change for a while.
 *
 * \return This date when this flag was last raised.
 */
time_t flag::get_date() const
{
    return f_date;
}


/** \brief Retrieve the date when the flag was last checked.
 *
 * The function returns the date when the code raising this flag was last
 * run.
 *
 * This indicates when the flag was last updated. If very recent then we
 * know that the problem that the flag raised is likely still in force.
 * A modified date which is really old may mean that the code testing
 * this problem does not automatically take the flag down (a bug in itself).
 *
 * Note that some flags are checked only once at boot time, or once
 * when a service starts. So it is not abnormal to see a raised flag
 * modification date remain the same for a very long time.
 *
 * \return This date when this flag's problem was last checked.
 */
time_t flag::get_modified() const
{
    return f_modified;
}


/** \brief Return a reference to the list of tags.
 *
 * A flag can be given a list of tags in order to group it with other
 * flags that may not be of the same unit or section.
 *
 * \return The set of tags attached to this flag.
 */
flag::tag_list_t const & flag::get_tags() const
{
    return f_tags;
}


/** \brief The name of the computer on which this flag was generated.
 *
 * In order to be able to distinguish on which computer the flag was
 * raised, the save() function includes the hostname of the computer
 * in the flag file.
 *
 * \return The hostname of the computer that saved this flag file.
 */
std::string const & flag::get_hostname() const
{
    return f_hostname;
}


/** \brief Retrieve the number of times this flag was raised.
 *
 * Each time a flag gets raised this counter is increased by 1. It starts
 * at 0 so the very first time it gets saved, the counter will be 1.
 *
 * This is an indicator of how many times the flag situation was found to
 * be true. In most cases this should be a really small number.
 *
 * \return The number of times the flag file was saved.
 */
int flag::get_count() const
{
    return f_count;
}


/** \brief Get the version used to create this flag file.
 *
 * When the flag file gets saved, the current version of the communicatord
 * project gets saved in the file as the "version" field. This function
 * returns that value. You can compare the value against:
 *
 * \code
 *     // dynamically get the version of the library at run time
 *     //
 *     communicatord::get_version_string()
 *
 *     // statically use the version of the library at compile time
 *     //
 *     COMMUNICATORD_VERSION_STRING
 * \endcode
 *
 * Note that if the file gets updated, then the version of the newest write
 * is used in the file.
 *
 * \return The version used to save the flag file.
 */
std::string const & flag::get_version() const
{
    return f_version;
}


/** \brief Transform the flag in a string.
 *
 * This function transforms the flag object in a string. In most cases, we
 * use this to print errors if the flag cannot be saved to disk.
 */
std::string flag::to_string() const
{
    std::stringstream ss;

    bool const down(get_state() == state_t::STATE_DOWN);
    bool const no_message(get_message().empty());
    snapdev::timespec_ex date(get_modified(), 0);
    ss << date.to_string("%Y/%m/%d %T")
       << ": "
       << (down ? "un" : "")
       << "flag("
       << get_unit()
       << "/"
       << get_section()
       << "/"
       << get_name()
       << "):"
       << get_source_file()
       << ":"
       << get_function()
       << ":"
       << get_line()
       << ": "
       << (down && no_message ? std::string("unflag error") : get_message())
       << " (priority: "
       << get_priority()
       << (get_manual_down() ? ", manual-down" : "")
       << ", count: "
       << get_count() + 1
       << ")";

    // field not yet included above
    //state_t                     get_state() const;
    //tag_list_t const &          get_tags() const;
    //std::string const &         get_hostname() const;
    //std::string const &         get_version() const;

    return ss.str();
}


/** \brief Save the data to file.
 *
 * This function is used to save the flag to file.
 *
 * Note that if the status is DOWN, then the file gets deleted.
 *
 * The file format used is the same as our configuration files:
 *
 * \code
 *      <varname>=<value>
 * \endcode
 *
 * These files should not be edited by administrators, though, since
 * they are just handled automatically by the code that generates this
 * data.
 *
 * \attention
 * Your implementation of the flags must make sure to use the
 * COMMUNICATORD_FLAG_UP() when an error is detected and use
 * the COMMUNICATORD_FLAG_DOWN() when the error is not detected
 * anymore. This is important since the file needs to disappear
 * once the problem was resolved.
 *
 * \return true if the save succeeded.
 */
bool flag::save()
{
    std::string const filename(get_filename());
    if(filename.empty())
    {
        SNAP_LOG_ERROR
            << "flag output filename could not be generated -- "
            << to_string()
            << SNAP_LOG_SEND;
        return false;
    }

    std::string communicator_user(get_config_param("user", "communicatord"));
    std::string communicator_group(get_config_param("group", "communicatord"));
    if(communicator_user.empty())
    {
        communicator_user = "communicatord";
    }
    if(communicator_group.empty())
    {
        communicator_group = "communicatord";
    }

    snapdev::as_root::pointer_t safe_user;
    uid_t const uid(geteuid());
    if(uid != 0)
    {
        // if the flag doesn't exist and it is to be taken down, we may be
        // able to ignore the safe_user request altogether
        //
        if(f_state == state_t::STATE_DOWN)
        {
            if(remove(filename))
            {
                return true;
            }
        }

        passwd * user(getpwuid(uid));
        if(user == nullptr)
        {
            // this is really not expected, stop here
            //
            int const e(errno);
            SNAP_LOG_ERROR
                << "could not find myself as a user in /etc/passwd database: "
                << e
                << ", "
                << strerror(e)
                << " -- "
                << to_string()
                << SNAP_LOG_SEND;
            return false;
        }
        if(user->pw_name != communicator_user)
        {
            if(f_from_raise_flag)
            {
                SNAP_LOG_ERROR
                    << "user \""
                    << user->pw_name
                    << "\" does not match the expected user \""
                    << communicator_user
                    << "\" -- "
                    << to_string()
                    << SNAP_LOG_SEND;
                return false;
            }

            // try to become communicatord although we do not expect
            // that to ever happen here
            //
            try
            {
                safe_user = std::make_shared<snapdev::as_root>(communicator_user);
            }
            catch(snapdev::unknown_user const & e)
            {
                // there is no reason to try to run the raise-flag command
                // in this case since it's probably not (properly) installed
                //
                SNAP_LOG_ERROR
                    << "Could not become communicator user: "
                    << e.what()
                    << "\" -- "
                    << to_string()
                    << SNAP_LOG_SEND;
                return false;
            }

            if(!safe_user->is_switched())
            {
                SNAP_LOG_VERBOSE
                    << "Could not become user \""
                    << communicator_user
                    << "\". Trying to run the `raise-flag` tool."
                    << SNAP_LOG_SEND;

                // that failed so far, so try with the raise-flag tool
                //
                cppprocess::process raise_flag("raise-flag");
                raise_flag.set_command("raise-flag");

                raise_flag.add_argument("--user");
                raise_flag.add_argument(communicator_user);
                raise_flag.add_argument("--group");
                raise_flag.add_argument(communicator_group);

                if(!f_source_file.empty())
                {
                    raise_flag.add_argument("--source-file");
                    raise_flag.add_argument(f_source_file);
                }
                if(!f_function.empty())
                {
                    raise_flag.add_argument("--function");
                    raise_flag.add_argument(f_function);
                }
                if(f_line != 0)
                {
                    raise_flag.add_argument("--line");
                    raise_flag.add_argument(std::to_string(f_line));
                }
                if(f_priority != DEFAULT_PRIORITY)
                {
                    raise_flag.add_argument("--priority");
                    raise_flag.add_argument(std::to_string(f_priority));
                }
                if(f_manual_down)
                {
                    raise_flag.add_argument("--manual");
                }
                if(!f_tags.empty())
                {
                    raise_flag.add_argument("--tags");
                    for(auto const & t : f_tags)
                    {
                        raise_flag.add_argument(t);
                    }
                }

                if(f_state == state_t::STATE_UP)
                {
                    raise_flag.add_argument("--up");
                }
                else
                {
                    raise_flag.add_argument("--down");
                }
                raise_flag.add_argument(f_unit);
                raise_flag.add_argument(f_section);
                raise_flag.add_argument(f_name);

                if(f_state == state_t::STATE_UP
                && !f_message.empty())
                {
                    // --up also expects a message
                    //
                    raise_flag.add_argument(f_message);
                }

                if(raise_flag.start() != 0)
                {
                    SNAP_LOG_ERROR
                        << "failed running the raise-flag command."
                        << SNAP_LOG_SEND;
                    return false;
                }

                if(!ed::communicator::instance()->is_running())
                {
                    int const r(raise_flag.wait());
                    if(r != 0)
                    {
                        SNAP_LOG_ERROR
                            << "raise-flag command return "
                            << r
                            << SNAP_LOG_SEND;
                        return false;
                    }
                }

                return true;
            }
        }
    }

    bool result(true);

    if(f_state == state_t::STATE_UP)
    {
        // create and config object
        //
        advgetopt::conf_file_setup setup(filename);
        advgetopt::conf_file::pointer_t file(advgetopt::conf_file::get_conf_file(setup));

        // if the file exists, check whether a "date" is defined
        //
        bool const exists(file->exists());
        bool has_date(false);
        bool has_count(false);
        if(exists)
        {
            has_date = file->has_parameter(communicatord::g_name_communicatord_param_date);
            has_count = file->has_parameter(communicatord::g_name_communicatord_param_count);
        }

        std::string const now(std::to_string(time(nullptr)));

        // setup all the fields as required
        // (note that setting up the first one will read the file if it exists...)
        //
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_unit,        f_unit);
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_section,     f_section);
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_name,        f_name);
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_message,     f_message);
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_priority,    std::to_string(f_priority));
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_manual_down, f_manual_down
                                                                                                    ? communicatord::g_name_communicatord_value_yes
                                                                                                    : communicatord::g_name_communicatord_value_no);
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_modified,    now);
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_hostname,    snapdev::gethostname());
        file->set_parameter(std::string(), communicatord::g_name_communicatord_param_version,     COMMUNICATORD_VERSION_STRING);

        if(!f_function.empty())
        {
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_function, f_function);
        }
        if(f_line > 0)
        {
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_line, std::to_string(f_line));
        }
        if(!f_source_file.empty())
        {
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_source_file, f_source_file);
        }
        if(!has_date)
        {
            // do not update the "date" parameter if already defined
            // see the "modified" parameter, which gets set to "now" each time
            //
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_date, std::to_string(f_date));
        }
        if(!f_tags.empty())
        {
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_tags, snapdev::join_strings(f_tags, ","));
        }

        if(has_count)
        {
            // increment existing counter by 1
            //
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_count,
                                std::to_string(std::stol(file->get_parameter(communicatord::g_name_communicatord_param_count)) + 1));
        }
        else
        {
            file->set_parameter(std::string(), communicatord::g_name_communicatord_param_count, "1");
        }

        // now save that data to file
        //
        result = file->save_configuration(".bak", true);

        // if we are root, we need to fix the ownership if we just created
        // the file
        //
        if(snapdev::chownnm(filename, communicator_user, communicator_group) != 0)
        {
            // the save worked, unfortunate that the chown() failed
            //
            int const e(errno);
            SNAP_LOG_ERROR
                << "could not properly setup the ownership of the file \""
                << filename
                << "\" to \""
                << communicator_user
                << ':'
                << communicator_group
                << "\" ("
                << e
                << ", "
                << strerror(e)
                << ") -- "
                << to_string()
                << SNAP_LOG_SEND;
        }
    }
    else
    {
        remove(filename);
    }

    return result;
}


bool flag::remove(std::string const & filename)
{
    // state is down, delete the file if it still exists
    //
    bool result(unlink(filename.c_str()) == 0);
    if(!result && errno == ENOENT)
    {
        // deleting a flag that does not exist "works" every time
        //
        result = true;
    }

    return result;
}


/** \brief Validate a name so we make sure they are as expected.
 *
 * Verify that the name is composed of letters (a-z, A-Z), digits (0-9),
 * and dashes (-) only.
 *
 * Also, it doesn't accept names that start with a digit or a dash.
 *
 * Note that the input is read/write because any upper case letters
 * will be transformed to lowercase (A-Z become a-z).
 *
 * Further, the name cannot have two dashes in a row nor a dash at
 * the end of the name.
 *
 * Finally, an empty name is also considered invalid.
 *
 * \param[in] name  The name to be checked.
 */
void flag::valid_name(std::string & name)
{
    if(name.empty())
    {
        throw invalid_name("unit, section, name, tags cannot be empty");
    }

    bool first(true);
    char last_char('\0');
    std::string::size_type const max(name.length());
    for(std::string::size_type idx(0); idx < max; ++idx)
    {
        char c(name[idx]);
        switch(c)
        {
        case '-':
            if(first)
            {
                throw invalid_name("unit, section, name, tags cannot start with a dash (-)");
            }
            if(last_char == '-')
            {
                throw invalid_name("unit, section, name, tags cannot have two dashes (--) in a row");
            }
            break;

        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            if(first)
            {
                throw invalid_name("unit, section, name, tags cannot start with a digit (-)");
            }
            break;

        default:
            if(c >= 'A' && c <= 'Z')
            {
                // force all to lowercase
                //
                c |= 0x20;
                name[idx] = c;
            }
            else if(c < 'a' || c > 'z')
            {
                throw invalid_name("name cannot include characters other than a-z, 0-9, and dashes (-)");
            }
            break;

        }
        first = false;
        last_char = c;
    }

    if(last_char == '-')
    {
        throw invalid_name("unit, section, name, tags cannot end with a dash (-)");
    }
}


/** \brief Load all the flag files.
 *
 * This function is used to load all the flag files from disk.
 *
 * \note
 * It is expected that the number of flags is always going to be relatively
 * small. The function make sure that if more than 100 are defined, only
 * the first 100 are read and another is created warning about the large
 * number of existing flags.
 *
 * \todo
 * At this time, the function returns an empty list_t if the function is
 * not able to list anything (i.e. the directory does not exist, for instance).
 * In that case, though, it should be considered that no flags are currently
 * raised.
 *
 * \return The list of flag files read from disk.
 */
flag::list_t flag::load_flags()
{
    list_t result;

    // read the list of files
    //
    std::string const path(get_path_to_flag_files());
    if(path.empty())
    {
        return result;
    }

    snapdev::glob_to_list<std::list<std::string>> flag_filenames;
    flag_filenames.read_path<
          snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE
        , snapdev::glob_to_list_flag_t::GLOB_FLAG_EMPTY>(
              path + "/*.flag");

    for(auto const & filename : flag_filenames)
    {
        if(result.size() >= flag::FLAGS_LIMIT)
        {
            // that error means we have over 100 flags raised
            //
            // we raise a "dynamic" flag about this error and ignore the
            // additional entries in the directory (the ignore happens because
            // of the throw in the load_flag() function.)
            //
            auto flag(COMMUNICATORD_FLAG_UP(
                      "communicatord"
                    , "flag"
                    , "too-many-flags"
                    , "too many flags were raised, showing only the first 100,"
                      " others can be viewed on this system at \"" + path + "\""));
            flag->set_priority(97);
            flag->add_tag("flag");
            flag->add_tag("too-many");
            result.push_back(flag); // passed to user, not saved
            break;
        }

        result.push_back(std::make_shared<flag>(filename));
    }

    return result;
}





} // namespace communicatord
// vim: ts=4 sw=4 et

// Copyright (c) 2011-2025  Made to Order Software Corp.  All Rights Reserved
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
#include    <communicatord/communicator.h>
#include    <communicatord/names.h>
#include    <communicatord/version.h>


// eventdispatcher
//
#include    <eventdispatcher/cui_connection.h>
#include    <eventdispatcher/local_stream_client_message_connection.h>
#include    <eventdispatcher/local_dgram_server_message_connection.h>
#include    <eventdispatcher/tcp_client_message_connection.h>
#include    <eventdispatcher/udp_server_message_connection.h>


// edhttp
//
#include    <edhttp/uri.h>


// snaplogger
//
#include    <snaplogger/message.h>
#include    <snaplogger/options.h>


// libaddr
//
#include    <libaddr/addr_parser.h>
#include    <libaddr/iface.h>


// snapdev
//
#include    <snapdev/gethostname.h>
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>
#include    <snapdev/stringize.h>


// advgetopt
//
#include    <advgetopt/advgetopt.h>
#include    <advgetopt/conf_file.h>
#include    <advgetopt/exception.h>


// C++
//
#include    <atomic>


// ncurses
//
#include    <ncurses.h>


// readline
//
#include    <readline/readline.h>
#include    <readline/history.h>


// last include
//
#include    <snapdev/poison.h>



/** \file
 * \brief A tool to send and receive messages to services to test them.
 *
 * This tool can be used to test various services and make sure they
 * work as expected, at least for their control feed. If they have
 * network connections that have nothing to do with communicator
 * messaging feeds, then it won't work well.
 *
 * The organization of this file is as follow:
 *
 * +------------------------+
 * |                        |
 * |        Base            |
 * |      (Connection)      |
 * |                        |
 * +------------------------+
 *        ^              ^
 *        |              |
 *        |              +----------------------------+
 *        |              |                            |
 *        |   +----------+-------------+   +----------+-------------+
 *        |   |                        |   |                        |
 *        |   |      GUI Object        |   |     CUI Object         |
 *        |   |                        |   |                        |
 *        |   +------------------------+   +------------------------+
 *        |        ^                                   ^
 *        |        |                                   |
 *        |        |       +---------------------------+
 *        |        |       |
 *     +--+--------+-------+----+
 *     |                        |
 *     |  Message Obj.          |
 *     |                        |
 *     +------------------------+
 */


namespace
{



constexpr char const * g_history_file = "~/.message_history";
constexpr char const * g_gui_command = "/var/lib/communicatord/sendmessage.gui";


const advgetopt::option g_command_line_options[] =
{
    advgetopt::define_option(
          advgetopt::Name("address")
        , advgetopt::ShortName('a')
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::Help("the address and port to connect to (i.e. \"127.0.0.1:4040\").")
    ),
    advgetopt::define_option(
          advgetopt::Name("cui")
        , advgetopt::Flags(advgetopt::any_flags<
              advgetopt::GETOPT_FLAG_GROUP_COMMANDS
            , advgetopt::GETOPT_FLAG_FLAG
            , advgetopt::GETOPT_FLAG_COMMAND_LINE>())
        , advgetopt::Help("start in interactive mode in your terminal.")
    ),
    advgetopt::define_option(
          advgetopt::Name("gui")
        , advgetopt::Flags(advgetopt::any_flags<
              advgetopt::GETOPT_FLAG_GROUP_COMMANDS
            , advgetopt::GETOPT_FLAG_FLAG
            , advgetopt::GETOPT_FLAG_COMMAND_LINE>())
        , advgetopt::Help("open a graphical window with an input and an output console.")
    ),
    advgetopt::define_option(
          advgetopt::Name("tcp")
        , advgetopt::Flags(advgetopt::any_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_FLAG
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE>())
        , advgetopt::Help("send a TCP message; use --wait to also wait for a reply and display it in your console; ignored in --gui or --cui mode.")
    ),
    advgetopt::define_option(
          advgetopt::Name("tls")
        , advgetopt::Flags(advgetopt::any_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_FLAG
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE>())
        , advgetopt::Help("when specified, attempt a secure connection with TLD encryption.")
    ),
    advgetopt::define_option(
          advgetopt::Name("udp")
        , advgetopt::Flags(advgetopt::any_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_FLAG
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE>())
        , advgetopt::Help("send a UDP message and quit.")
    ),
    advgetopt::define_option(
          advgetopt::Name("unix")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_FLAG>())
        , advgetopt::Help("use a Data Stream (a.k.a. Unix socket).")
    ),
    advgetopt::define_option(
          advgetopt::Name("verbose")
        , advgetopt::ShortName('v')
        , advgetopt::Flags(advgetopt::standalone_command_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("make the output verbose.")
    ),
    advgetopt::define_option(
          advgetopt::Name("wait")
        , advgetopt::Flags(advgetopt::any_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_FLAG
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE>())
        , advgetopt::Help("in case you used --tcp, this tells %p to wait for a reply before quiting.")
    ),
    // default (anything goes in this)
    advgetopt::define_option(
          advgetopt::Name("message")
        , advgetopt::Flags(advgetopt::command_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_DEFAULT_OPTION
            , advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_MULTIPLE>())
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


char const * const g_configuration_directories[] =
{
    "/etc/communicatord",
    nullptr
};


// until we have C++20, remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_command_line_options_environment =
{
    .f_project_name = "message",
    .f_group_name = nullptr,
    .f_options = g_command_line_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = "MESSAGE",
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = "message.conf",
    .f_configuration_directories = g_configuration_directories,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>] [<message> ...]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = COMMUNICATORD_VERSION_STRING,
    .f_license = "GNU GPL v3",
    .f_copyright = "Copyright (c) 2013-"
                   SNAPDEV_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    .f_build_date = UTC_BUILD_DATE,
    .f_build_time = UTC_BUILD_TIME,
    .f_groups = g_group_descriptions,
};
#pragma GCC diagnostic pop





}
// no name namespace


class connection_lost
{
public:
    virtual ~connection_lost() {}

    virtual void    lost_connection() = 0;
};




class tcp_message_connection
    : public ed::tcp_client_message_connection
{
public:
    typedef std::shared_ptr<tcp_message_connection> message_pointer_t;

    tcp_message_connection(
              connection_lost * cl
            , addr::addr const & address
            , ed::mode_t const mode)
        : tcp_client_message_connection(address, mode, false)
        , f_connection_lost(cl)
    {
    }

    tcp_message_connection(tcp_message_connection const &) = delete;

    virtual ~tcp_message_connection()
    {
    }

    tcp_message_connection operator = (tcp_message_connection const &) = delete;

    virtual void process_error() override
    {
        tcp_client_message_connection::process_error();

        std::cerr << "error: an error occurred while handling a message." << std::endl;

        f_connection_lost->lost_connection();
    }

    virtual void process_hup() override
    {
        tcp_client_message_connection::process_hup();

        std::cerr << "error: the connection hang up on us, while handling a message." << std::endl;

        f_connection_lost->lost_connection();
    }

    virtual void process_invalid() override
    {
        tcp_client_message_connection::process_invalid();

        std::cerr << "error: the connection is invalid." << std::endl;

        f_connection_lost->lost_connection();
    }

    virtual void process_message(ed::message & msg) override
    {
        std::cout << "success: received message: "
                  << msg.to_message()
                  << std::endl;
    }

private:
    connection_lost *       f_connection_lost = nullptr;
};



class local_message_connection
    : public ed::local_stream_client_message_connection
{
public:
    typedef std::shared_ptr<local_message_connection> message_pointer_t;

    local_message_connection(
              connection_lost * cl
            , addr::addr_unix const & address)
        : local_stream_client_message_connection(address, false, false)
        , f_connection_lost(cl)
    {
    }

    local_message_connection(local_message_connection const &) = delete;

    virtual ~local_message_connection() override
    {
    }

    local_message_connection operator = (local_message_connection const &) = delete;

    virtual void process_error() override
    {
        local_stream_client_message_connection::process_error();

        std::cerr << "error: an error occurred while handling a message." << std::endl;

        f_connection_lost->lost_connection();
    }

    virtual void process_hup() override
    {
        local_stream_client_message_connection::process_hup();

        std::cerr << "error: the connection hang up on us, while handling a message." << std::endl;

        f_connection_lost->lost_connection();
    }

    virtual void process_invalid() override
    {
        local_stream_client_message_connection::process_invalid();

        std::cerr << "error: the connection is invalid." << std::endl;

        f_connection_lost->lost_connection();
    }

    virtual void process_message(ed::message & msg) override
    {
        std::cout << "success: received message: "
                  << msg.to_message()
                  << std::endl;
    }

private:
    connection_lost *       f_connection_lost = nullptr;
};



class network_connection
    : public connection_lost
{
public:
    typedef std::shared_ptr<network_connection>     pointer_t;
    typedef std::weak_ptr<network_connection>       weak_pointer_t;

    enum class connection_t
    {
        NONE,
        TCP,            // local TCP
        REMOTE_TCP,     // non-secure, private TCP
        SECURE_TCP,     // secure private or public TCP
        UDP,            // local (or not) UDP
        BROADCAST_UDP,  // UDP in broadcast mode
        LOCAL_STREAM,   // local (unix) stream
        LOCAL_DGRAM,    // local (unix) datagram
    };

    virtual ~network_connection() override
    {
        // calling disconnect() is "too late" because the connection is
        // part of the ed::communicator and needs to be removed from
        // there before the destructor gets called...
        //disconnect();
    }

    void lost_connection()
    {
        disconnect();
    }

    void disconnect()
    {
        ed::communicator::instance()->remove_connection(f_tcp_connection);
        ed::communicator::instance()->remove_connection(f_unix_connection);
        f_tcp_connection = nullptr;
        f_connection_type = connection_t::NONE;
        f_prompt.clear();
    }

    bool set_address(std::string const & address)
    {
        // clear old connection first, just in case
        //
        disconnect();

        // we allow the user to connect to any one of the allowed socket
        // that the communicator listens on
        //
        // the type is 100% defined by the address which is expected to
        // include a scheme, when no scheme is defined, "cd:" is used
        // as the default.
        //
        //     cd://<ip>:<port> -- a plain TCP connection
        //     cd:///run/communicatod/stream.sock -- a plain local stream connection (Unix)
        //     cds://<ip>:<port> -- a secure TCP connection
        //     cdu://<ip>:<port> -- a plain UDP connection
        //     cdu:///run/communicatord/datagram.sock -- a plain local datagram connection (Unix)
        //     cdb://<ip>:<port> -- a broadcasting UDP connection
        //
        try
        {
            if(!f_uri.set_uri(address, true, true))
            {
                std::cerr
                    << "error: unsupported address \""
                    << address
                    << "\": "
                    << f_uri.get_last_error_message()
                    << std::endl;
                return false;
            }
        }
        catch(std::exception const & e)
        {
            std::cerr
                << "error: an exception occurred parsing URI \""
                << address
                << "\": "
                << e.what()
                << std::endl;
            return false;
        }
        std::string const & scheme(f_uri.scheme());
        if(scheme == "cd")
        {
            if(f_uri.domain().empty())
            {
                // Unix Stream
                //
                f_unix_address = addr::addr_unix(address);
                f_prompt = "local/stream> ";
                f_selected_connection_type = connection_t::LOCAL_STREAM;
            }
            else
            {
                // TCP in plain mode
                //
                addr::addr const a(addr::string_to_addr(
                          f_uri.domain()
                        , "127.0.0.1"
                        , communicatord::LOCAL_PORT
                        , "tcp"));
                switch(a.get_network_type())
                {
                case addr::network_type_t::NETWORK_TYPE_LOOPBACK:
                    f_selected_connection_type = connection_t::TCP;
                    break;

                case addr::network_type_t::NETWORK_TYPE_PUBLIC:
                    std::cerr << "warning: remote TCP without encryption is expected to use a private network IP address." << std::endl;
                    [[fallthrough]];
                case addr::network_type_t::NETWORK_TYPE_PRIVATE:
                    f_selected_connection_type = connection_t::REMOTE_TCP;
                    break;

                default:
                    std::cerr << "error: unsupported network type of a Plain TCP/IP connection." << std::endl;
                    return false;

                }
                f_prompt = "tcp/ip> ";
                f_ip_address = a;
            }
        }
        else if(scheme == "cds")
        {
            if(f_uri.domain().empty())
            {
                std::cerr << "error: invalid use of 'scs:' scheme; an IP address was expected." << std::endl;
                return false;
            }
            // TCP in secure mode
            //
            addr::addr const a(addr::string_to_addr(
                      address
                    , "127.0.0.1"
                    , communicatord::SECURE_PORT
                    , "tcp"));
            switch(a.get_network_type())
            {
            case addr::network_type_t::NETWORK_TYPE_LOOPBACK:
                std::cerr << "error: invalid use of 'scs:' scheme; it cannot work on a loopback address." << std::endl;
                return false;

            case addr::network_type_t::NETWORK_TYPE_PUBLIC:
            case addr::network_type_t::NETWORK_TYPE_PRIVATE:
                break;

            default:
                std::cerr << "error: unsupported network type for a Secure TCP/IP connection." << std::endl;
                return false;

            }
            f_prompt = "tcp/ip(tls)> ";
            f_ip_address = a;
            f_selected_connection_type = connection_t::SECURE_TCP;
        }
        else if(scheme == "cdu")
        {
            if(f_uri.domain().empty())
            {
                // Unix Datagram
                //
                f_unix_address = addr::addr_unix(address);
                f_prompt = "local/dgram> ";
                f_selected_connection_type = connection_t::LOCAL_DGRAM;
            }
            else
            {
                // UDP
                //
                addr::addr a(addr::string_to_addr(
                          address
                        , "127.0.0.1"
                        , communicatord::UDP_PORT
                        , "udp"));
                switch(a.get_network_type())
                {
                case addr::network_type_t::NETWORK_TYPE_PUBLIC:
                    std::cerr << "warning: UDP has no encryption, it should not be used with a public IP address." << std::endl;
                    [[fallthrough]];
                case addr::network_type_t::NETWORK_TYPE_LOOPBACK:
                case addr::network_type_t::NETWORK_TYPE_PRIVATE:
                    break;

                default:
                    std::cerr << "error: unsupported network type of a Plain UDP/IP connection." << std::endl;
                    return false;

                }
                f_ip_address = a;
                f_prompt = "udp/ip> ";
                f_selected_connection_type = connection_t::UDP;
            }
        }
        else if(scheme == "cdb")
        {
            if(f_uri.domain().empty())
            {
                // broadcasting on a Unix Datagram would be useless since we
                // have a single listener on such
                //
                std::cerr << "error: invalid use of 'scu:' scheme; an IP address was expected." << std::endl;
                return false;
            }
            addr::addr const a(addr::string_to_addr(
                      address
                    , "127.0.0.1"
                    , communicatord::UDP_PORT
                    , "udp"));
            switch(a.get_network_type())
            {
            case addr::network_type_t::NETWORK_TYPE_PUBLIC:
                std::cerr << "warning: UDP has no encryption, it should not be used with a public IP address." << std::endl;
                [[fallthrough]];
            case addr::network_type_t::NETWORK_TYPE_LOOPBACK:
            case addr::network_type_t::NETWORK_TYPE_PRIVATE:
                // verify that this is the broadcast address (i.e. 192.168.33.255/24)
                if(!addr::is_broadcast_address(f_ip_address))
                {
                    std::cerr
                        << "error: UDP/IP address "
                        << address
                        << " is not a valid broadcast address." << std::endl;
                    return false;
                }
                break;

            case addr::network_type_t::NETWORK_TYPE_MULTICAST: // in case you do not know the destination private network
                break;

            default:
                std::cerr
                    << "error: unsupported network type of a Plain UDP/IP address "
                    << address
                    << '.'
                    << std::endl;
                return false;

            }
            f_ip_address = a;
            f_prompt = "udp/ip(broadcast)> ";
            f_selected_connection_type = connection_t::UDP;
        }
        else
        {
            std::cerr << "error: unknown scheme '"
                << scheme
                << ":'; expected 'cd:', 'cds:', 'cdu:', or 'cdb:'." << std::endl;
            return false;
        }

        return true;
    }

    void create_tcp_connection()
    {
        // create new connection
        //
        ed::mode_t const mode(f_selected_connection_type == connection_t::SECURE_TCP
                            ? ed::mode_t::MODE_SECURE
                            : ed::mode_t::MODE_PLAIN);
        try
        {
            f_tcp_connection = std::make_shared<tcp_message_connection>(this, f_ip_address, mode);
        }
        catch(std::exception const & e)
        {
            std::cerr
                << "error: could not create a tcp_message_connection: "
                << e.what()
                << '.'
                << std::endl;
            return;
        }

        if(ed::communicator::instance()->add_connection(f_tcp_connection))
        {
            f_connection_type = f_selected_connection_type;
        }
        else
        {
            // keep NONE (not connected)
            //
            f_tcp_connection.reset();
            std::cerr
                << "error: could not connect--verify the IP, the port, and make sure that do or do not need to use the --ssl flag."
                << std::endl;
        }
    }

    void create_udp_connection()
    {
        // create new connection
        // -- at this point we only deal with client connections here and
        //    the following creates a UDP server; to send data we just use
        //    the send_message() which is a static function
        //
        //f_tcp_connection = std::make_shared<ed::udp_server_message_connection>(ip, f_mode);

        f_connection_type = f_selected_connection_type;
    }

    void create_local_stream_connection()
    {
        disconnect();

        try
        {
            f_unix_connection = std::make_shared<local_message_connection>(this, f_unix_address);
        }
        catch(std::exception const & e)
        {
            std::cerr
                << "error: could not create a tcp_message_connection: "
                << e.what()
                << '.'
                << std::endl;
            return;
        }

        if(ed::communicator::instance()->add_connection(f_unix_connection))
        {
            f_connection_type = f_selected_connection_type;
        }
        else
        {
            // keep NONE (not connected)
            //
            f_unix_connection.reset();
            std::cerr
                << "error: could not connect--verify the IP, the port, and make sure that do or do not need to use the --ssl flag."
                << std::endl;
        }
    }

    // if not yet connected, attempt a connection
    bool connect()
    {
        // currently disconnected?
        //
        if(f_connection_type != f_selected_connection_type)
        {
            // connect as selected by user
            //
            switch(f_selected_connection_type)
            {
            case connection_t::NONE:           // no type, we cannot connect
                break;

            case connection_t::TCP:            // local TCP
            case connection_t::REMOTE_TCP:     // non-secure, private TCP
            case connection_t::SECURE_TCP:     // secure private or public TCP
                create_tcp_connection();
                break;

            case connection_t::UDP:            // local (or not) UDP
            case connection_t::BROADCAST_UDP:  // UDP in broadcast mode
                create_udp_connection();
                break;

            case connection_t::LOCAL_STREAM:   // local (unix) stream
                create_local_stream_connection();
                break;

            case connection_t::LOCAL_DGRAM:    // local (unix) datagram
                break;

            }

            if(f_connection_type == connection_t::NONE)
            {
                std::cerr << "error: could not connect, can't send messages." << std::endl;
                f_prompt.clear();
                return false;
            }
        }

        return true;
    }

    bool send_message(std::string const & message)
    {
        // are we or can we connect?
        //
        if(!connect())
        {
            return false;
        }

        ed::message msg;
        if(!msg.from_message(message))
        {
            std::cerr
                << "error: message \""
                << message
                << "\" is invalid. It won't be sent."
                << std::endl;
            return false;
        }

        switch(f_connection_type)
        {
        case connection_t::NONE:
            return false;

        case connection_t::TCP:
        case connection_t::REMOTE_TCP:
        case connection_t::SECURE_TCP:
            f_tcp_connection->send_message(msg, false);
            break;

        case connection_t::UDP:
        case connection_t::BROADCAST_UDP:
            send_udp_message(msg);
            break;

        case connection_t::LOCAL_STREAM:
            f_unix_connection->send_message(msg, false);
            break;

        case connection_t::LOCAL_DGRAM:
            send_dgram_message(msg);
            break;

        }

        return true;
    }

    void send_udp_message(ed::message const & msg)
    {
        // TODO: use a command instead of reading that signal secret from
        //       the config file since we could be trying to send to a
        //       remote communicatord and the secret could be different
        //       there...
        //
        advgetopt::conf_file_setup const setup("communicatord");
        advgetopt::conf_file::pointer_t config(advgetopt::conf_file::get_conf_file(setup));
        ed::udp_server_message_connection::send_message(
                      f_ip_address
                    , msg
                    , config->get_parameter(communicatord::g_name_communicatord_config_signal_secret));
    }

    void send_dgram_message(ed::message & msg)
    {
        // TODO: see send_udp_message() -- replace signal-secret with
        //       message command line option...
        //
        advgetopt::conf_file_setup const setup("communicatord");
        advgetopt::conf_file::pointer_t config(advgetopt::conf_file::get_conf_file(setup));
        ed::local_dgram_server_message_connection::send_message(
                      f_unix_address
                    , msg
                    , config->get_parameter(communicatord::g_name_communicatord_config_signal_secret));
    }

    connection_t set_selected_connection_type() const
    {
        return f_selected_connection_type;
    }

    // call when you do /tcp and /udp in CUI/GUI
    void set_selected_connection_type(connection_t type)
    {
        if(f_selected_connection_type != type)
        {
            disconnect();
            f_selected_connection_type = type;
        }
    }

    bool has_prompt() const
    {
        return !f_prompt.empty();
    }

    std::string define_prompt() const
    {
        if(f_prompt.empty())
        {
            return std::string("not connected> ");
        }
        return f_prompt;
    }

private:
    // WARNING: The following variables are accessed by another process
    //          when running in GUI or CUI mode (i.e. we do a `fork()`.)
    //
    //          The only way to modify those values once the fork() happened
    //          is by sending messages to the child process.
    //
    edhttp::uri                             f_uri                       = edhttp::uri();
    addr::addr                              f_ip_address                = addr::addr();
    addr::addr_unix                         f_unix_address              = addr::addr_unix();
    connection_t                            f_selected_connection_type  { connection_t::UDP }; // never set to NONE; default to UDP unless user uses --tcp on command line
    connection_t                            f_connection_type           { connection_t::NONE };
    std::string                             f_prompt                    = std::string();
    tcp_message_connection::pointer_t       f_tcp_connection            = tcp_message_connection::pointer_t();
    local_message_connection::pointer_t     f_unix_connection           = local_message_connection::pointer_t();
};






class console_connection
    : public ed::cui_connection
{
public:
    typedef std::shared_ptr<console_connection>     pointer_t;

    console_connection(network_connection::pointer_t c)
        : cui_connection(g_history_file)
        , f_connection(c)
    {
        if(g_console != nullptr)
        {
            throw std::logic_error("there can be only one console_connection");
        }
        g_console = this;
    }

    console_connection(console_connection const & rhs) = delete;
    console_connection & operator = (console_connection const & rhs) = delete;

    void reset_prompt()
    {
        network_connection::pointer_t c(f_connection.lock());
        if(c != nullptr)
        {
            set_prompt(c->define_prompt());
        }
    }

    static int create_message(int count, int c)
    {
        snapdev::NOT_USED(count, c);

        g_console->open_message_dialog();
        return 0;
    }

    void open_message_dialog()
    {
        if(f_win_message != nullptr)
        {
            delwin(f_win_message);
            f_win_message = nullptr;
            refresh();
            return;
        }

        // Note:
        // We probably want to use the `dialog` library.
        // Try `man 3 dialog` for details about that library.
        // There is also an online version of that manual page:
        // https://www.freebsd.org/cgi/man.cgi?query=dialog&sektion=3

        // TODO: create function to gather the screen size
        int width = 80;
        int height = 15;
        f_win_message = newwin(height - 4, width - 4, 2, 2);
        if(f_win_message == nullptr)
        {
            std::cerr << "error: couldn't create message window." << std::endl;
            exit(1);
        }

        wborder(f_win_message, 0, 0, 0, 0, 0, 0, 0, 0);
        mvwprintw(f_win_message, 0, 2, " Create Message ");

        // TODO message...
        mvwprintw(f_win_message, 2, 2, "TODO: implement popup dialog to help creating a message without");
        mvwprintw(f_win_message, 3, 2, "      having to know the exact syntax.");

        if(wrefresh(f_win_message) != OK)
        {
            std::cerr << "error: wrefresh() to output message window failed." << std::endl;
            exit(1);
        }
    }

    void set_message_dialog_key_binding()
    {
        if(rl_bind_keyseq("\\eOQ" /* F2 */, &create_message) != 0)
        {
            std::cerr << "invalid key (^[OQ a.k.a. F2) sequence passed to rl_bind_keyseq.\n";
        }
    }

    virtual void process_command(std::string const & command) override
    {
        if(execute_command(command))
        {
            // reset the prompt
            //
            reset_prompt();
        }
    }

    virtual void process_quit() override
    {
        network_connection::pointer_t c(f_connection.lock());
        if(c != nullptr)
        {
            c->disconnect();
            f_connection.reset(); // should be useless
        }

        ed::communicator::instance()->remove_connection(shared_from_this());

        // remove the pipes for stdout and stderr
        //
        // WARNING: this must be done AFTER we disconnected from the
        //          ncurses which is done above (at this point the
        //          connection was deleted though! weird...)
        //
        cui_connection::process_quit(); 
    }

    virtual void process_help() override
    {
        help();
    }

    bool execute_command(std::string const & command)
    {
        // /quit
        //
        // request to quit the process, equivalent to Ctrl-D
        //
// that should be a statistic in a stats window...
//output("# of con: " + std::to_string(ed::communicator::instance()->get_connections().size()));
        if(command == "/quit")
        {
            // the "/quit" internal command
            //
            process_quit();
            return false;
        }

        // /help
        //
        // print out help screen
        //
        if(command == "/help"
        || command == "/?"
        || command == "?")
        {
            help();
            return false;
        }

        if(command == "/msg_help")
        {
            help_message();
            return false;
        }

        network_connection::pointer_t c(f_connection.lock());
        if(c == nullptr)
        {
            output("You are disconnected. Most commands will not work anymore.");
            return false;
        }

        // /connect <scheme>://<IP>:<port> | <scheme>:///<path>
        //
        // connect to service listening at <IP> on port <port>
        //
        if(command.compare(0, 9, "/connect ") == 0)
        {
            if(c->set_address(command.substr(9)))
            {
                c->connect();
            }
            return true;
        }

        // /disconnect
        //
        // remove the existing connection
        //
        if(command == "/disconnect")
        {
            c->disconnect();
            return true;
        }

        // "/.*" is not a valid message beginning, we suspect that the user
        // misstyped a command and thus generate an error instead
        //
        if(command[0] == '/')
        {
            output("error: unknown command: \"" + command + "\".");
            return false;
        }

        if(!c->has_prompt())
        {
            output("error: message not sent, we are not connected.");
            return false;
        }

        // by default, if not an internal command, we consider `command`
        // to be a message and therefore we just send it
        //
        c->send_message(command);
        return false;
    }

private:
    void help()
    {
        output(
            "Help:\n"
            "Internal commands start with a  slash (/). Supported commands:\n"
            "  /connect <scheme>://<ip>:<port> | <scheme>:///<path> -- connect to specified URI\n"
            "    i.e. /connect cd://192.168.2.1:4004\n"
            "  /disconnect -- explicitly disconnect any existing connection\n"
            "  /help or /? or ? or <F1> key -- print this help screen\n"
            "  /quit -- leave tool\n"
            "  <F2> key -- create a message in a popup window\n"
            "  ... -- message to send to current connection (/msg_help for more)\n"
            "    a message is composed of:\n"
            "      ['<'<server>:<service>' '][<server>:<service>'/']command[' '<name>=<value>';'...]\n"
            "    where the first <server>:<service> is the origin (\"sent from\")\n"
            "    where the second <server>:<service> is the destination\n"
            "    where <name>=<value> pairs are parameters (can be repeated)\n");
    }

    void help_message()
    {
        output(
            "Help:\n"
            "Commands/messages to work with the communicator daemon:\n"
            "  /connect cd://192.168.2.1:4004\n"
            "  REGISTER service=message;version=1\n"
            "  COMMANDS list=ACCEPT,HELP,QUITTING,READY,STOP,UNKNOWN,COMMANDS\n"
            "    add more messages as required for your test\n"
            "  <server_name:message server_name:other_service/...\n"
            "    server_name is set to `hostname` by default: " + snapdev::gethostname() + "\n"
            "  <server_name:message server_name:other_service/STOP"\n"
            "    ends other_service");
    }

    static console_connection *     g_console /* = nullptr; done below because it's static */;

    network_connection::weak_pointer_t
                        f_connection = network_connection::weak_pointer_t();
    WINDOW *            f_win_message = nullptr;
};

console_connection * console_connection::g_console = nullptr;





class message
{
public:
    message(int argc, char * argv[])
        : f_opts(g_command_line_options_environment)
    {
        snaplogger::add_logger_options(f_opts);
        f_opts.finish_parsing(argc, argv);
        if(!snaplogger::process_logger_options(
                              f_opts
                            , "/etc/communicatord/logger"
                            , std::cout
                            , false))
        {
            // exit on any error
            throw advgetopt::getopt_exit("logger options generated an error.", 1);
        }

        f_gui = f_opts.is_defined("gui");

        f_cui = f_opts.is_defined("cui")
            || (!f_opts.is_defined("message") && !f_gui);

        if(f_gui
        && f_cui)
        {
            std::cerr << "error: --gui and --cui are mutually exclusive." << std::endl;
            exit(1);
            snapdev::NOT_REACHED();
        }

        if(f_cui
        || f_gui)
        {
            if(f_opts.is_defined("message"))
            {
                std::cerr << "error: --message is not compatible with --cui or --gui." << std::endl;
                exit(1);
                snapdev::NOT_REACHED();
            }
        }
        else
        {
            if(!f_opts.is_defined("address"))
            {
                std::cerr << "error: --address is mandatory when not entering the CUI or GUI interface." << std::endl;
                exit(1);
                snapdev::NOT_REACHED();
            }
        }

        f_connection = std::make_shared<network_connection>();

        if(f_opts.is_defined("address"))
        {
            f_connection->set_address(f_opts.get_string("address"));
        }
    }

    int run()
    {
        if(f_gui)
        {
            return start_gui();
        }

        if(f_cui)
        {
            return enter_cui();
        }

        if(f_opts.is_defined("message"))
        {
            return f_connection->send_message(f_opts.get_string("message")) ? 0 : 1;
        }

        std::cerr << "error: no command specified, one of --gui, --cui, or --message is required; note that --message is implied if you just enter a message on the command line." << std::endl;
        return 1;
    }

    int start_gui()
    {
        // the GUI is a separate executable wchi is installed along the
        // communicatord-gui package so as to not impose Qt on all
        // servers; you probably don't need a GUI on your non-X servers
        // anyway--try the 'cui' instead
        //
        if(access(g_gui_command, R_OK | X_OK) != 0)
        {
            std::cerr << "error: the --gui is not currently available; did you install the communicatord-gui package? -- on a server, consider using --cui instead." << std::endl;
            return 1;
        }
        std::string cmd(g_gui_command);
        cmd += ' ';
        cmd += f_opts.options_to_string();
        return system(cmd.c_str());
    }

    int enter_cui()
    {
        // add a CUI connection to the ed::communicator
        //
        {
            console_connection::pointer_t cui(std::make_shared<console_connection>(f_connection));
            cui->ready();
            cui->reset_prompt();
            cui->set_message_dialog_key_binding();
            if(!ed::communicator::instance()->add_connection(cui))
            {
                std::cerr << "error: could not add CUI console to list of ed::communicator connections." << std::endl;
                return 1;
            }
        }

        // run until we are asked to exit
        //
        if(ed::communicator::instance()->run())
        {
            return 0;
        }

        // run() returned with an error
        //
        std::cerr << "error: something went wrong in the ed::communicator run() loop." << std::endl;
        return 1;
    }

private:
    advgetopt::getopt                       f_opts;
    bool                                    f_gui = false;
    bool                                    f_cui = false;
    network_connection::pointer_t           f_connection = network_connection::pointer_t();
    //connection_handler::pointer_t           f_connection_handler;
};


int main(int argc, char *argv[])
{
    try
    {
        message sm(argc, argv);

        return sm.run();
    }
    catch(advgetopt::getopt_exit const & e)
    {
        return e.code();
    }
    catch(std::exception const & e)
    {
        // clean error on exception
        //
        std::cerr << "message: exception: " << e.what() << std::endl;
    }
    catch(...)
    {
        // unknown/non-standard error type occurred
        //
        std::cerr << "message: an unknown exception occurred." << std::endl;
    }

    return 1;
}

// vim: ts=4 sw=4 et

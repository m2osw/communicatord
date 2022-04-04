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


// snapcommunicator
//
#include    <snapcommunicator/version.h>


// eventdispatcher
//
#include    <eventdispatcher/cui_connection.h>
#include    <eventdispatcher/tcp_client_message_connection.h>
#include    <eventdispatcher/udp_server_message_connection.h>


// snaplogger
//
#include    <snaplogger/message.h>
#include    <snaplogger/options.h>


// libaddr
//
#include    <libaddr/addr_parser.h>


// snapdev
//
#include    <snapdev/not_reached.h>
#include    <snapdev/not_used.h>


// advgetopt
//
#include    <advgetopt/advgetopt.h>
#include    <advgetopt/conf_file.h>
#include    <advgetopt/exception.h>


// boost
//
#include    <boost/preprocessor/stringize.hpp>


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
 * network connections that have nothing to do with snap_communicator
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
 *     |  Snap Message Obj.     |
 *     |                        |
 *     +------------------------+
 */


namespace
{



char const * history_file = "~/.snapmessage_history";


const advgetopt::option g_command_line_options[] =
{
    {
        'a',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_CONFIGURATION_FILE | advgetopt::GETOPT_FLAG_REQUIRED,
        "address",
        nullptr,
        "the address and port to connect to (i.e. \"127.0.0.1:4040\")",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_CONFIGURATION_FILE | advgetopt::GETOPT_FLAG_FLAG,
        "cui",
        nullptr,
        "start in interactive mode in your console",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_CONFIGURATION_FILE | advgetopt::GETOPT_FLAG_FLAG,
        "gui",
        nullptr,
        "open a graphical window with an input and an output console",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_FLAG,
        "ssl",
        nullptr,
        "if specified, make a secure connection (with encryption)",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_FLAG,
        "tcp",
        nullptr,
        "send a TCP message; use --wait to also wait for a reply and display it in your console",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_FLAG,
        "udp",
        nullptr,
        "send a UDP message and quit",
        nullptr
    },
    {
        'v',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE | advgetopt::GETOPT_FLAG_FLAG,
        "verbose",
        nullptr,
        "make the output verbose",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_FLAG | advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE,
        "wait",
        nullptr,
        "in case you used --tcp, this tells sendmessage to wait for a reply before quiting",
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_COMMAND_LINE | advgetopt::GETOPT_FLAG_REQUIRED | advgetopt::GETOPT_FLAG_MULTIPLE | advgetopt::GETOPT_FLAG_DEFAULT_OPTION,
        "message",
        nullptr,
        nullptr, // hidden argument in --help screen
        nullptr
    },
    {
        '\0',
        advgetopt::GETOPT_FLAG_END,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    }
};


char const * const g_configuration_directories[] =
{
    "/etc/snapwebsites",
    nullptr
};


// until we have C++20, remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_command_line_options_environment =
{
    .f_project_name = "snapwebsites",
    .f_group_name = nullptr,
    .f_options = g_command_line_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = "SNAPMESSAGE",
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = "snapmessage.conf",
    .f_configuration_directories = g_configuration_directories,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>] [<message> ...]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = SNAPCOMMUNICATOR_VERSION_STRING,
    .f_license = "GNU GPL v2",
    .f_copyright = "Copyright (c) 2013-"
                   BOOST_PP_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    //.f_build_date = UTC_BUILD_DATE,
    //.f_build_time = UTC_BUILD_TIME
};
#pragma GCC diagnostic pop





}
//namespace






class tcp_message_connection
    : public ed::tcp_client_message_connection
{
public:
    typedef std::shared_ptr<tcp_message_connection> message_pointer_t;

    tcp_message_connection(addr::addr const & address, ed::mode_t const mode)
        : tcp_client_message_connection(address, mode, false)
    {
    }

    virtual ~tcp_message_connection()
    {
    }

    virtual void process_error() override
    {
        tcp_client_message_connection::process_error();

        std::cerr << "error: an error occurred while handling a message." << std::endl;
    }

    virtual void process_hup() override
    {
        tcp_client_message_connection::process_hup();

        std::cerr << "error: the connection hang up on us, while handling a message." << std::endl;
    }

    virtual void process_invalid() override
    {
        tcp_client_message_connection::process_invalid();

        std::cerr << "error: the connection is invalid." << std::endl;
    }

    virtual void process_message(ed::message const & message) override
    {
        std::cout << "success: received message: "
                  << message.to_message()
                  << '\n';
    }

private:
};



class network_connection
{
public:
    typedef std::shared_ptr<network_connection>     pointer_t;
    typedef std::weak_ptr<network_connection>       weak_pointer_t;

    enum class connection_t
    {
        NONE,
        TCP,
        UDP
    };

    ~network_connection()
    {
        // calling disconnect() is "too late" because the connection is
        // part of the snap communicator and needs to be removed from
        // there before the destructor gets called...
        //disconnect();
    }

    void disconnect()
    {
        ed::communicator::instance()->remove_connection(f_tcp_connection);
        f_tcp_connection = nullptr;
        f_connection_type = connection_t::NONE;
    }

    void set_address(std::string const & addr)
    {
        f_address = addr;
    }

    void create_tcp_connection()
    {
        // clear old connection first, just in case
        //
        disconnect();

        // determine the IP address and port
        //
        f_addr = addr::string_to_addr(
                  f_address
                , "127.0.0.1"
                , 4041
                , "tcp");

        // create new connection
        //
        f_tcp_connection = std::make_shared<tcp_message_connection>(f_addr, f_mode);

        if(ed::communicator::instance()->add_connection(f_tcp_connection))
        {
            f_connection_type = connection_t::TCP;
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
        // clear old connection first, just in case
        //
        disconnect();

        // determine the IP address and port
        //
        f_addr = addr::string_to_addr(
                  f_address
                , "127.0.0.1"
                , 4041
                , "udp");

        // create new connection
        // -- at this point we only deal with client connections here
        //    and this is a UDP server; to send data we just use the
        //    send_message() which is static
        //
        //f_tcp_connection = std::make_shared<snap::snap_communicator::snap_udp_server_message_connection>(ip, f_mode);

        f_connection_type = connection_t::UDP;
    }

    // if not yet connected, attempt a connection
    bool connect()
    {
        // currently disconnected?
        //
        if(f_connection_type == connection_t::NONE)
        {
            // connect as selected by user
            //
            if(f_selected_connection_type == connection_t::TCP)
            {
                create_tcp_connection();
            }
            else
            {
                create_udp_connection();
            }

            if(f_connection_type == connection_t::NONE)
            {
                std::cerr << "error: could not connect, can't send message." << std::endl;
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
                << "\" is invalid. It won't be sent.\n";
            return false;
        }

        switch(f_connection_type)
        {
        case connection_t::NONE:
            return false;

        case connection_t::TCP:
            f_tcp_connection->send_message(msg, false);
            break;

        case connection_t::UDP:
            {
                advgetopt::conf_file_setup const setup("snapcommunicator");
                advgetopt::conf_file::pointer_t config(advgetopt::conf_file::get_conf_file(setup));
                ed::udp_server_message_connection::send_message(
                              f_addr
                            , msg
                            , config->get_parameter("signal_secret"));
            }
            break;

        }

        return true;
    }

    // only use at initialization time, otherwise use switch_mode()
    void set_mode(ed::mode_t mode)
    {
        f_mode = mode;
    }

    void switch_mode(ed::mode_t mode)
    {
        if(f_mode != mode)
        {
            disconnect();
            f_mode = mode;
        }
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

    void switch_connection_type(connection_t type)
    {
        if(f_connection_type != type)
        {
            switch(type)
            {
            case connection_t::NONE:
                disconnect();
                break;

            case connection_t::TCP:
                create_tcp_connection();
                break;

            case connection_t::UDP:
                create_udp_connection();
                break;

            }
        }
    }

    std::string define_prompt() const
    {
        std::string prompt;
        if(f_selected_connection_type == connection_t::TCP)
        {
            prompt = "tcp";
        }
        else
        {
            prompt = "udp";
        }
        if(f_mode == ed::mode_t::MODE_SECURE)
        {
            prompt += "(tls)";
        }
        prompt += "> ";
        return prompt;
    }

private:
    // WARNING: The following variables are accessed by another process
    //          when running in GUI or CUI mode (i.e. we do a `fork()`.)
    //
    //          The only way to modify those values once the fork() happened
    //          is by sending messages to the child process.
    //
    std::string                             f_address                   = std::string();
    addr::addr                              f_addr                      = addr::addr();
    ed::mode_t                              f_mode                      { ed::mode_t::MODE_PLAIN };
    connection_t                            f_selected_connection_type  { connection_t::UDP }; // never set to NONE; default to UDP unless user uses --tcp on command line
    connection_t                            f_connection_type           { connection_t::NONE };
    tcp_message_connection::pointer_t       f_tcp_connection            = tcp_message_connection::pointer_t();
};






class console_connection
    : public ed::cui_connection
{
public:
    typedef std::shared_ptr<console_connection>     pointer_t;

    console_connection(network_connection::pointer_t c)
        : cui_connection(history_file)
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
            std::cerr << "couldn't create message window." << std::endl;
            exit(1);
        }

        wborder(f_win_message, 0, 0, 0, 0, 0, 0, 0, 0);
        mvwprintw(f_win_message, 0, 2, " Create Message ");

        // TODO message...
        mvwprintw(f_win_message, 2, 2, "TODO: implement popup dialog to help creating a message without");
        mvwprintw(f_win_message, 3, 2, "      having to know the exact syntax.");

        if(wrefresh(f_win_message) != OK)
        {
            std::cerr << "wrefresh() to output message window failed." << std::endl;
            exit(1);
        }
    }

    void set_message_dialog_key_binding()
    {
        if(rl_bind_keyseq("\\eOQ" /* F2 */, &create_message) != 0)
        {
            std::cerr << "invalid key (^[OQ a.k.a. F2) sequence passed to rl_bind_keyseq";
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
//output("# of con: " + std::to_string(snap::snap_communicator::instance()->get_connections().size()));
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

        network_connection::pointer_t c(f_connection.lock());
        if(c == nullptr)
        {
            output("You are disconnected. Most commands will not work anymore.");
            return false;
        }

        // /connect <IP>:<port>
        //
        // connect to service listening at <IP> on port <port>
        //
        if(command.compare(0, 9, "/connect ") == 0)
        {
            c->set_address(command.substr(9));
            c->connect();
            return false;
        }

        // /disconnect
        //
        // remove the existing connection
        //
        if(command == "/disconnect")
        {
            c->disconnect();
            return false;
        }

        // /tcp
        //
        // switch to TCP
        //
        if(command == "/tcp")
        {
            c->set_selected_connection_type(network_connection::connection_t::TCP);
            return true;
        }

        // /udp
        //
        // switch to UDP
        //
        if(command == "/udp")
        {
            c->set_selected_connection_type(network_connection::connection_t::UDP);
            return true;
        }

        // /plain
        //
        // switch to plain mode (opposed to SSL)
        //
        if(command == "/plain")
        {
            c->switch_mode(ed::mode_t::MODE_PLAIN);
            return true;
        }

        // /ssl
        //
        // switch to SSL mode (opposed to plain, unencrypted)
        //
        if(command == "/ssl"
        || command == "/tls")
        {
            c->switch_mode(ed::mode_t::MODE_SECURE);
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

        // by default, if not an internal command, we consider the command
        // to be the content a message and therefore we just send it
        //
        c->send_message(command);
        return false;
    }

private:
    void help()
    {
        output("Help:");
        output("Internal commands start with a  slash (/). Supported commands:");
        output("  /connect <ip>:<port> -- connect to specified IP and port");
        output("  /disconnect -- explicitly disconnect any existing connection");
        output("  /help or /? or ? or F1 key -- print this help screen");
        output("  /plain -- get a plain connection");
        output("  /quit -- leave snapmessage");
        output("  /tcp -- send messages using our TCP connections");
        output("  /udp -- send messages using our UDP connections");
        output("  /ssl -- get an SSL connection");
        output("  F2 -- create a message in a popup window");
    }

    static console_connection *     g_console /* = nullptr; done below because it's static */;

    network_connection::weak_pointer_t
                        f_connection = network_connection::weak_pointer_t();
    WINDOW *            f_win_message = nullptr;
};

console_connection * console_connection::g_console = nullptr;





class snapmessage
{
public:
    snapmessage(int argc, char * argv[])
        : f_opts(g_command_line_options_environment)
    {
        snaplogger::add_logger_options(f_opts);
        f_opts.finish_parsing(argc, argv);
        if(!snaplogger::process_logger_options(f_opts, "/etc/snapcommunicator/logger"))
        {
            // exit on any error
            throw advgetopt::getopt_exit("logger options generated an error.", 1);
        }

        f_gui = f_opts.is_defined("gui");

        f_cui = f_opts.is_defined("cui")
            || !f_opts.is_defined("message");

        if(f_gui
        && f_cui)
        {
            std::cerr << "error: --gui and --cui are mutually exclusive." << std::endl;
            exit(1);
            snapdev::NOT_REACHED();
        }

        if(f_cui)
        {
            if(f_opts.is_defined("message"))
            {
                std::cerr << "error: --message is not compatible with --cui." << std::endl;
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

        if(f_opts.is_defined("tcp")
        && f_opts.is_defined("udp"))
        {
            std::cerr << "error: --tcp and --udp are mutually exclusive" << std::endl;
            exit(1);
            snapdev::NOT_REACHED();
        }

        f_connection = std::make_shared<network_connection>();

        if(f_opts.is_defined("address"))
        {
            f_connection->set_address(f_opts.get_string("address"));
        }

        f_connection->set_mode(f_opts.is_defined("ssl")
                ? ed::mode_t::MODE_SECURE
                : ed::mode_t::MODE_PLAIN);

        f_connection->set_selected_connection_type(f_opts.is_defined("tcp")
                    ? network_connection::connection_t::TCP
                    : network_connection::connection_t::UDP);
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

        if(f_opts.is_defined("tcp")
        || f_opts.is_defined("udp"))
        {
            return f_connection->send_message(f_opts.get_string("message")) ? 0 : 1;
        }

        std::cerr << "error: no command specified, one of --gui, --cui, --tcp, --udp is required." << std::endl;

        return 1;
    }

    int start_gui()
    {
        std::cerr << "error: the --gui is not yet implemented." << std::endl;
        return 1;
    }

    int enter_cui()
    {
        // add a CUI connection to the snap_communicator
        //
        {
            console_connection::pointer_t cui(std::make_shared<console_connection>(f_connection));
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
        std::cerr << "error: something went wrong in the snap_communicator run() loop." << std::endl;
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
        snapmessage sm(argc, argv);

        return sm.run();
    }
    catch( advgetopt::getopt_exit const & except )
    {
        return except.code();
    }
    catch(std::exception const & e)
    {
        // clean error on exception
        //
        std::cerr << "snapsignal: exception: " << e.what() << std::endl;
        return 1;
    }
}

// vim: ts=4 sw=4 et

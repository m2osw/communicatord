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

/** \file
 * \brief Implementation of the Communicator.
 *
 * This is the crux of this application, the service which manages all the
 * communication between all the other services (well nearly all of them).
 * The Communicator is what is called an RPC service. You use it
 * to send messages to various services to run commands and get replies.
 *
 * In most cases, the send_message() will be instantaneous. You must have
 * an event loop (see the ed::communicator object) to wait for replies.
 */

// self
//
#include    "server.h"

#include    "interrupt.h"
#include    "listener.h"
#include    "load_timer.h"
#include    "ping.h"
#include    "remote_connection.h"
#include    "remote_communicators.h"
#include    "service_connection.h"
#include    "unix_connection.h"
#include    "unix_listener.h"


// communicatord
//
#include    <communicatord/exception.h>
#include    <communicatord/flags.h>
#include    <communicatord/loadavg.h>
#include    <communicatord/communicatord.h>
#include    <communicatord/version.h>


// advgetopt
//
#include    <advgetopt/exception.h>
#include    <advgetopt/options.h>


// serverplugins
//
#include    <serverplugins/plugin.h>


// libaddr
//
#include    <libaddr/addr_parser.h>
#include    <libaddr/iface.h>
#include    <libaddr/addr_unix.h>


// snapdev lib
//
#include    <snapdev/file_contents.h>
#include    <snapdev/gethostname.h>
#include    <snapdev/glob_to_list.h>
#include    <snapdev/join_strings.h>
#include    <snapdev/pathinfo.h>
#include    <snapdev/tokenize_string.h>
#include    <snapdev/trim_string.h>


// snaplogger
//
#include    <snaplogger/exception.h>
#include    <snaplogger/logger.h>
#include    <snaplogger/message.h>
#include    <snaplogger/options.h>


// boost
//
#include    <boost/preprocessor/stringize.hpp>


// C++
//
#include    <cmath>
#include    <thread>


// C
//
#include    <grp.h>
#include    <pwd.h>


// included last
//
#include    <snapdev/poison.h>






namespace communicator_daemon
{



namespace
{


char const *        g_status_filename = "/var/lib/communicatord/cluster-status.txt";


/** \brief The sequence number of a message being broadcast.
 *
 * Each instance of communicatord may broadcast a message to other
 * communicators. When that happens, we want to ignore that
 * message in case it comes again to the same communicatord. This
 * can be accomplished by saving which messages we received.
 *
 * We also control a number of hops and a timeout.
 *
 * This counter is added to the name of the computer running this
 * communicatord (i.e. f_server_name) so for example it would
 * look as following if the computer name is "snap":
 *
 * \code
 *          snap-123
 * \endcode
 */
int64_t             g_broadcast_sequence = 0;




const advgetopt::option g_options[] =
{
    advgetopt::define_option(
          advgetopt::Name("certificate")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("")
        , advgetopt::Help("certificate for --secure-listen connections.")
    ),
    advgetopt::define_option(
          advgetopt::Name("data-path")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("/var/lib/communicatord")
        , advgetopt::Help("a path where the communicatord saves data it uses between runs such as the list of IP addresses of other communicators.")
    ),
    advgetopt::define_option(
          advgetopt::Name("debug-all-messages")
        , advgetopt::Flags(advgetopt::standalone_all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
#ifdef _DEBUG
        , advgetopt::Help("log all the messages received by the communicatord and verify them (as per the COMMAND message).")
#else
        , advgetopt::Help("verify the incoming messages (as per the COMMAND message).")
#endif
    ),
    advgetopt::define_option(
          advgetopt::Name("group-name")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("communicatord")
        , advgetopt::Help("drop privileges to this group.")
    ),
    advgetopt::define_option(
          advgetopt::Name("local-listen")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("127.0.0.1:4040")
        , advgetopt::Help("<IP:port> to open a local TCP connection (no encryption).")
    ),
    advgetopt::define_option(
          advgetopt::Name("max-connections")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("100")
        , advgetopt::Help("maximum number of connections allowed by this communicatord.")
    ),
    advgetopt::define_option(
          advgetopt::Name("max-pending-connections")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("25")
        , advgetopt::Help("maximum number of client connections waiting to be accepted.")
    ),
    advgetopt::define_option(
          advgetopt::Name("my-address")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("define the communicatord address (i.e. 10.0.2.33); it has to be defined in one of your interfaces.")
        , advgetopt::Validator("address")
    ),
    advgetopt::define_option(
          advgetopt::Name("neighbors")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("define a comma separated list of communicatord neighbors.")
    ),
    advgetopt::define_option(
          advgetopt::Name("private-key")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("")
        , advgetopt::Help("private key for --secure-listen connections.")
    ),
    advgetopt::define_option(
          advgetopt::Name("remote-listen")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("<IP:port> to open a remote TCP connection (no encryption). If 127.0.0.1, ignore (no remote access).")
    ),
    advgetopt::define_option(
          advgetopt::Name("secure-listen")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("<user:password>@<IP:port> to open a remote TCP connection (with encryption, requires the --certificate & --private-key).")
    ),
    advgetopt::define_option(
          advgetopt::Name("server-name")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("name of this server, defaults to `hostname` if undefined here.")
    ),
    advgetopt::define_option(
          advgetopt::Name("services")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("/usr/share/communicatord/services")
        , advgetopt::Help("path to the list of service files.")
    ),
    advgetopt::define_option(
          advgetopt::Name("signal")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("127.0.0.1:4041")
        , advgetopt::Help("an address accepting UDP messages (signals); these messages do not get acknowledged.")
    ),
    advgetopt::define_option(
          advgetopt::Name("signal-secret")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("a secret key used to verify that UDP packets are acceptable.")
    ),
    advgetopt::define_option(
          advgetopt::Name("unix-group")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("communicator-group")
        , advgetopt::Help("the group to assign to this Unix socket.")
    ),
    advgetopt::define_option(
          advgetopt::Name("unix-listen")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("/run/communicatord/communicatord.sock")
        , advgetopt::Help("a Unix socket name to listen for local connections.")
    ),
    advgetopt::define_option(
          advgetopt::Name("user-name")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("communicatord")
        , advgetopt::Help("drop privileges to this user.")
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

constexpr char const * const g_configuration_files[] =
{
    "/etc/communicatord/communicatord.conf",
    nullptr
};

// TODO: once we have stdc++20, remove all defaults & pragma
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "communicatord",
    .f_group_name = "communicatord",
    .f_options = g_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = "COMMUNICATORD",
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = g_configuration_files,
    .f_configuration_filename = nullptr,
    .f_configuration_directories = nullptr,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = COMMUNICATORD_VERSION_STRING,
    .f_license = "GPL v2 or newer",
    .f_copyright = "Copyright (c) 2012-"
                   BOOST_PP_STRINGIZE(UTC_BUILD_YEAR)
                   "  Made to Order Software Corporation",
    .f_build_date = UTC_BUILD_DATE,
    .f_build_time = UTC_BUILD_TIME,
    .f_groups = g_group_descriptions
};
#pragma GCC diagnostic pop





}
// noname namespace




/** \class server
 * \brief Set of connections in the communicatord tool.
 *
 * All the connections and sockets in general will all appear
 * in this class.
 */




/** \brief Construct the server object.
 *
 * This function saves the server pointer in the server
 * object. It is used later to gather various information and call helper
 * functions.
 *
 * \param[in] argc  The number of arguments in \p argv.
 * \param[in] argv  The command line arguments.
 */
server::server(int argc, char * argv[])
    : f_opts(g_options_environment)
    , f_dispatcher(std::make_shared<ed::dispatcher>(this))
{
    snaplogger::add_logger_options(f_opts);
    f_opts.finish_parsing(argc, argv);
    if(!snaplogger::process_logger_options(f_opts, "/etc/communicatord/logger"))
    {
        // exit on any error
        //
        throw advgetopt::getopt_exit("logger options generated an error.", 1);
    }

    f_debug_all_messages = f_opts.is_defined("debug_all_messages");
#ifdef _DEBUG
    if(f_debug_all_messages)
    {
        f_dispatcher->set_trace();
    }
#endif
    set_dispatcher(f_dispatcher);

    f_dispatcher->add_matches({
        DISPATCHER_MATCH("ACCEPT", &server::msg_accept),
        // default in dispatcher: ALIVE
        DISPATCHER_MATCH("CLUSTERSTATUS", &server::msg_clusterstatus),
        DISPATCHER_MATCH("COMMANDS", &server::msg_commands),
        DISPATCHER_MATCH("CONNECT", &server::msg_connect),
        DISPATCHER_MATCH("DISCONNECT", &server::msg_disconnect),
        DISPATCHER_MATCH("FORGET", &server::msg_forget),
        DISPATCHER_MATCH("GOSSIP", &server::msg_gossip),
        // default in dispatcher: HELP
        // default in dispatcher: LEAK
        DISPATCHER_MATCH("LISTENLOADAVG", &server::msg_listen_loadavg),
        DISPATCHER_MATCH("LISTSERVICES", &server::msg_list_services),
        DISPATCHER_MATCH("LOADAVG", &server::msg_save_loadavg),
        // default in dispatcher: LOG_ROTATE
        DISPATCHER_MATCH("PUBLIC_IP", &server::msg_public_ip),
        // default in dispatcher: QUITTING -- the default is not valid for us, we have it overridden, but no need to do anything here
        // default in dispatcher: READY
        DISPATCHER_MATCH("REFUSE", &server::msg_refuse),
        DISPATCHER_MATCH("REGISTER", &server::msg_register),
        DISPATCHER_MATCH("REGISTERFORLOADAVG", &server::msg_registerforloadavg),
        // default in dispatcher: RESTART
        DISPATCHER_MATCH("SERVICESTATUS", &server::msg_servicestatus),
        DISPATCHER_MATCH("SHUTDOWN", &server::msg_shutdown),
        DISPATCHER_MATCH("UNREGISTER", &server::msg_unregister),
        DISPATCHER_MATCH("UNREGISTERFORLOADAVG", &server::msg_unregisterforloadavg),
        // default in dispatcher: STOP -- we overload the stop() which gets called by the message implementation
        // default in dispatcher: UNKNOWN -- we overload the function to include more details in the log

        // others are handled with the eventdispatcher defaults
    });

    f_dispatcher->add_communicator_commands();
}


server::~server()
{
}


/** \brief Initialize the server.
 *
 * This function is used to initialize the connetions object. This means
 * setting up a few parameters such as the nice level of the application
 * and priority scheme for listening to events.
 *
 * Then it creates two sockets: one listening on TCP/IP and the other
 * listening on UDP/IP. The TCP/IP is for other servers to connect to
 * and listen communicate various status between various servers. The
 * UDP/IP is used to very quickly send messages between servers. The
 * UDP/IP messages are viewed as signals to wake up a server so it
 * starts working on new data (in most cases, at least.)
 *
 * \return 0 on success, 1 on error.
 */
int server::init()
{
    // keep a copy of the server name handy
    if(f_opts.is_defined("server-name"))
    {
        f_server_name = f_opts.get_string("server-name");
    }
    if(f_server_name.empty())
    {
        f_server_name = snapdev::gethostname();
    }

    f_number_of_processors = std::max(1U, std::thread::hardware_concurrency());

    // check a user defined maximum number of connections
    // by default this is set to COMMUNICATORD_MAX_CONNECTIONS,
    // which at this time is 100
    //
    f_max_connections = f_opts.get_long("max-connections");

    communicatord::set_loadavg_path(f_opts.get_string("data-path"));

    // read the list of available services
    //
    {
        std::string path_to_services(f_opts.get_string("services"));
        path_to_services += "/*.service";
        typedef std::set<std::string> service_files_t;
        snapdev::glob_to_list<service_files_t> dir;
        if(dir.read_path<snapdev::glob_to_list_flag_t::GLOB_FLAG_NO_ESCAPE>(path_to_services))
        {
            // we have some local services (note that communicatord is
            // not added as a local service)
            //
            // at the moment we do not load those files, these could include
            // data such as the service description
            //
            for(auto const & p : dir)
            {
                std::string const service_name(snapdev::pathinfo::basename(p, ".service"));
                f_local_services_list.insert(service_name);

                SNAP_LOG_DEBUG
                    << "Known local service: \""
                    << service_name
                    << "\"."
                    << SNAP_LOG_SEND;
            }
        }
        else
        {
            SNAP_LOG_ERROR
                << "search of services failed: "
                << dir.get_last_error_message()
                << SNAP_LOG_SEND;
        }

        // the list of local services cannot (currently) change while
        // communicatord is running so generate the corresponding
        // string once
        //
        f_local_services = snapdev::join_strings(f_local_services_list, ",");
    }

    f_communicator = ed::communicator::instance();

    // capture Ctrl-C (SIGINT)
    //
    ed::connection::pointer_t ctrl_c(std::make_shared<interrupt>(shared_from_this()));
    f_communicator->add_connection(ctrl_c);
    f_interrupt = ctrl_c;

    int const max_pending_connections(f_opts.get_long("max-pending-connections"));
    if(max_pending_connections < 5
    || max_pending_connections > 1000)
    {
        SNAP_LOG_FATAL
            << "the --max-pending-connections option must be a valid number between 5 and 1000. "
            << max_pending_connections
            << " is not valid."
            << SNAP_LOG_SEND;
        return 1;
    }

    // create up to four listeners
    //
    // * TCP local (required) -- a local TCP/IP connection on IP 127.0.0.1
    //   for local services to connect to (now the Unix connection is
    //   preferred for security reasons)
    //
    // * Unix (optional) -- a local Unix stream connection for local services
    //
    // * TCP private (optional) -- a LAN based TCP/IP connection on your
    //   LAN network other communicatord servers connect to this connetion
    //
    // * TCP secure (optional) -- a public TCP/IP connection on the Internet
    //   to allow other communicatord servers to connect from anywhere

    // TCP LOCAL
    {
        addr::addr local_listen(addr::string_to_addr(
                  f_opts.get_string("local-listen")
                , "127.0.0.1"
                , communicatord::LOCAL_PORT
                , "tcp"));
        if(local_listen.get_network_type() != addr::network_type_t::NETWORK_TYPE_LOOPBACK)
        {
            SNAP_LOG_FATAL
                << "The --local-listen option must be a loopback IP address. "
                << f_opts.get_string("local-listen")
                << " is not acceptable."
                << SNAP_LOG_SEND;
            return 1;
        }

        // make this listener the local listener
        //
        f_local_listener = std::make_shared<listener>(
                                  shared_from_this()
                                , local_listen
                                , std::string()
                                , std::string()
                                , max_pending_connections
                                , true
                                , f_server_name);
        f_local_listener->set_name("communicator local listener");
        f_communicator->add_connection(f_local_listener);

        SNAP_LOG_CONFIGURATION
            << "listening to local connection \""
            << local_listen.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT)
            << "\"."
            << SNAP_LOG_SEND;
    }

    // UNIX
    //
    if(f_opts.is_defined("unix-listen"))
    {
        addr::addr_unix unix_listen(addr::addr_unix(f_opts.get_string("unix-listen")));
        unix_listen.set_scheme("cd");

        // let users of the communicator-user group access this socket
        //
        unix_listen.set_mode(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
        unix_listen.set_group(f_opts.get_string("unix-group"));

        f_unix_listener = std::make_shared<unix_listener>(
                  shared_from_this()
                , unix_listen
                , max_pending_connections
                , f_server_name);
        f_unix_listener->set_name("communicator unix listener");
        f_communicator->add_connection(f_unix_listener);

        SNAP_LOG_CONFIGURATION
            << "listening to Unix socket \""
            << unix_listen.to_string()
            << "\"."
            << SNAP_LOG_SEND;
    }

    // PLAIN REMOTE
    //
    int default_remote_port(communicatord::REMOTE_PORT);
    if(f_opts.is_defined("remote-listen"))
    {
        std::string const listen_str(f_opts.get_string("remote-listen"));
        addr::addr listen_addr(addr::string_to_addr(
                      listen_str
                    , "0.0.0.0"
                    , communicatord::REMOTE_PORT
                    , "tcp"));

        if(listen_addr.is_default())
        {
            // this is a fatal error!
            //
            SNAP_LOG_FATAL
                << "the communicatord \"listen="
                << listen_str
                << "\" parameter is the default IP address."
                   " For security reasons, we do not allow such an IP in the plain"
                   " remote connection. You may use that address in the secure"
                   " connection instead."
                << SNAP_LOG_SEND;

            //communicatord::flag::pointer_t flag(COMMUNICATORD_FLAG_UP(
            //              "communicatord"
            //            , "cluster"
            //            , "no-cluster"
            //            , ss.str()));
            //flag->set_priority(82);
            //flag->add_tag("initialization");
            //flag->add_tag("network");
            //flag->save();

            return 1;
        }

        // make this listener the remote listener, however, if the IP
        // address is 127.0.0.1 we skip on this one, we do not need
        // two listeners on the local IP address (i.e. the TCP LOCAL is
        // already listening on that IP)
        //
        if(listen_addr.get_network_type() == addr::network_type_t::NETWORK_TYPE_LOOPBACK)
        {
            SNAP_LOG_CONFIGURATION
                << "ignoring the \"listen=...\" parameter since it is set to the loopback."
                << SNAP_LOG_SEND;
        }
        else
        {
            if(listen_addr.get_network_type() != addr::network_type_t::NETWORK_TYPE_PRIVATE)
            {
                // this is a fatal error!
                //
                SNAP_LOG_FATAL
                    << "the communicatord \"listen="
                    << listen_str
                    << "\" parameter is not a private IP address."
                       " For security reasons, the PLAIN REMOTE connection is"
                       " not allowed to use a public IP address. If you need"
                       " that IP address, consider setting up the"
                       " --secure-listen option instead."
                    << SNAP_LOG_SEND;

                return 1;
            }

            default_remote_port = listen_addr.get_port();

            f_public_ip = listen_addr.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT);
            f_remote_listener = std::make_shared<listener>(
                      shared_from_this()
                    , listen_addr
                    , std::string()
                    , std::string()
                    , max_pending_connections
                    , false
                    , f_server_name);
            f_remote_listener->set_name("communicator remote listener");
            f_communicator->add_connection(f_remote_listener);

            SNAP_LOG_CONFIGURATION
                << "listening to plain remote connection \""
                << f_public_ip
                << "\"."
                << SNAP_LOG_SEND;
        }
    }

    // secure remote
    //
    std::string const certificate(f_opts.get_string("certificate"));
    std::string const private_key(f_opts.get_string("private-key"));
    if(!certificate.empty()
    && !private_key.empty()
    && f_opts.is_defined("secure-listen"))
    {
        std::string const secure_info(f_opts.get_string("secure-listen"));
        std::string::size_type const pos(secure_info.find('@'));
        if(pos == std::string::npos)
        {
            SNAP_LOG_FATAL
                << "the --secure-listen parameter must include a user name, a password, an IP address, and optionally a port."
                << SNAP_LOG_SEND;
            return 1;
        }

        // TODO: make use of our password service to keep the password
        //       encrypted instead of having it visible in your .conf files
        //
        std::string user_password(secure_info.substr(0, pos));
        std::string::size_type const colon(user_password.find(':'));
        if(colon == std::string::npos)
        {
            SNAP_LOG_FATAL
                << "the user and password of the --secure-listen parameter must separated by a colon."
                << SNAP_LOG_SEND;
            return 1;
        }
        std::string const username(user_password.substr(0, colon));
        std::string const password(user_password.substr(colon + 1));
        if(username.empty()
        || password.empty())
        {
            SNAP_LOG_FATAL
                << "the user and password of the --secure-listen parameter cannot be empty."
                << SNAP_LOG_SEND;
            return 1;
        }

        std::string const ip_port(secure_info.substr(pos + 1));
        addr::addr const secure_listen(addr::string_to_addr(
                  ip_port
                , "0.0.0.0"
                , communicatord::SECURE_PORT
                , "tcp"));

        // this listener is the secure listener so it is expected to use
        // a public or at least a private IP address
        //
        if(secure_listen.get_network_type() == addr::network_type_t::NETWORK_TYPE_PUBLIC
        || secure_listen.get_network_type() == addr::network_type_t::NETWORK_TYPE_PRIVATE
        || secure_listen.get_network_type() == addr::network_type_t::NETWORK_TYPE_ANY)
        {
            f_secure_ip = secure_listen.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT);
            listener::pointer_t sl(std::make_shared<listener>(
                      shared_from_this()
                    , secure_listen
                    , certificate
                    , private_key
                    , max_pending_connections
                    , false
                    , f_server_name));
            sl->set_name("communicator secure listener");
            sl->set_username(username);
            sl->set_password(password);
            f_communicator->add_connection(sl);
            f_secure_listener = sl;

            SNAP_LOG_CONFIGURATION
                << "listening to public secure connection \""
                << f_secure_ip
                << "\"."
                << SNAP_LOG_SEND;
        }
        else
        {
            SNAP_LOG_WARNING
                << "remote \"secure_listen\" parameter is \""
                << ip_port
                << "\" (not public or private) so it is ignored and no secure remote connections will be possible."
                << SNAP_LOG_SEND;
        }
    }
    else
    {
        SNAP_LOG_INFO
            << "no certificate, private key, or secure-listen was defined, no secure connection will be possible."
            << SNAP_LOG_SEND;
    }

    {
        addr::addr signal_address(addr::string_to_addr(
                          f_opts.get_string("signal")
                        , "127.0.0.1"
                        , communicatord::UDP_PORT
                        , "udp"));

        ping::pointer_t p(std::make_shared<ping>(shared_from_this(), signal_address));
        if(f_opts.is_defined("signal-secret"))
        {
            p->set_secret_code(f_opts.get_string("signal-secret"));
        }
        p->set_name("communicator messenger (UDP)");
        f_ping = p;
        if(!f_communicator->add_connection(f_ping))
        {
            SNAP_LOG_NOTICE
                << "adding the ping signal UDP listener to ed::communicator failed."
                << SNAP_LOG_SEND;
        }
        else
        {
            SNAP_LOG_CONFIGURATION
                << "listening to UDP connection \""
                << signal_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS)
                << "\"."
                << SNAP_LOG_SEND;
        }
    }

    {
        f_loadavg_timer = std::make_shared<load_timer>(shared_from_this());
        f_loadavg_timer->set_name("communicator load balancer timer");
        f_communicator->add_connection(f_loadavg_timer);
    }

    // transform the --my-address to an addr::addr object
    //
    // note that the default port is not important if the listen_addr is not
    // set because then no remote communicatord can connect to this system
    // via this address (instead, we may be able to use the secure address?)
    //
    f_connection_address = addr::addr(addr::string_to_addr(
                  f_opts.get_string("my-address")
                , std::string()
                , default_remote_port
                , "tcp"));
    if(addr::find_addr_interface(f_connection_address, false) == nullptr)
    {
        std::string msg("my-address \"");
        msg += f_connection_address.to_ipv6_string(addr::STRING_IP_BRACKET_ADDRESS);
        msg += "\" not found on this computer. Did a copy of the configuration file and forgot to change that entry?";
        SNAP_LOG_FATAL
            << msg
            << SNAP_LOG_SEND;
        throw communicatord::address_missing(msg);
    }

    f_remote_communicators = std::make_shared<remote_communicators>(
                                          shared_from_this()
                                        , f_connection_address);

    if(f_connection_address.get_network_type() != addr::network_type_t::NETWORK_TYPE_LOOPBACK
    && !f_connection_address.is_default())
    {
        add_neighbors(f_connection_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
    }

    if(f_opts.is_defined("neighbors"))
    {
        f_explicit_neighbors = canonicalize_neighbors(f_opts.get_string("neighbors"));
        add_neighbors(f_explicit_neighbors);
    }

    f_user_name = f_opts.get_string("user-name");
    f_group_name = f_opts.get_string("group-name");

    // if we are in a one computer environment this call would never happen
    // unless someone sends us a CLUSTERSTATUS, but that does not have the
    // exact same effect
    //
    cluster_status(nullptr);

    return 0;
}


void server::drop_privileges()
{
    // drop to non-priv user/group if we are root
    //
    if(getuid() != 0)
    {
        // forget it, probably a programmer?
        return;
    }

    // Group first, then user. Otherwise you lose privs to change your group!
    //
    {
        group const * grp(getgrnam(f_group_name.c_str()));
        if(nullptr == grp)
        {
            std::stringstream ss;
            ss << "Cannot locate group \""
               << f_group_name
               << "\"! Create it first, then run the server.";
            SNAP_LOG_FATAL << ss << SNAP_LOG_SEND;
            throw communicatord::user_missing(ss.str());
        }
        int const sw_grp_id(grp->gr_gid);
        //
        if(setegid(sw_grp_id) != 0)
        {
            int const e(errno);
            std::stringstream ss;
            ss << "Cannot drop privileges to group \""
               << f_group_name
               << "\"! errno: "
               << e
               << ", "
               << strerror(e);
            SNAP_LOG_FATAL << ss << SNAP_LOG_SEND;
            throw communicatord::switching_to_user_failed(ss.str());
        }
    }
    //
    {
        passwd const * pswd(getpwnam(f_user_name.c_str()));
        if(nullptr == pswd)
        {
            std::stringstream ss;
            ss << "Cannot locate user \""
               << f_user_name
               << "\"! Create it first, then run the server.";
            SNAP_LOG_FATAL << ss << SNAP_LOG_SEND;
            throw communicatord::user_missing(ss.str());
        }
        int const sw_usr_id(pswd->pw_uid);
        //
        if(seteuid(sw_usr_id) != 0)
        {
            int const e(errno);
            std::stringstream ss;
            ss << "Cannot drop privileges to user \""
               << f_user_name
               << "\"! Create it first, then run the server. errno: "
               << e
               << ", "
               << strerror(e);
            SNAP_LOG_FATAL << ss << SNAP_LOG_SEND;
            throw communicatord::switching_to_user_failed(ss.str());
        }
    }
}


/** \brief The execution loop.
 *
 * This function runs the execution loop until the communicatord
 * system receives a QUIT or STOP message.
 */
int server::run()
{
    int r(init());
    if(r != 0)
    {
        return r;
    }

    // run "forever" (until we receive a QUIT message)
    //
    f_communicator->run();

    // we are done, cleanly get rid of the communicator
    //
    f_communicator.reset();

    // we received a RELOADCONFIG, exit with 1 so systemd restarts us
    //
    return f_force_restart ? 1 : 0;
}


/** \brief Make sure that the connection understands a command.
 *
 * This function checks whether the specified connection (\p connection)
 * understands the command about to be sent to it (\p reply).
 *
 * \note
 * The test is done only when communicatord is run in debug
 * mode to not waste time.
 *
 * \param[in,out] connection  The concerned connection that has to understand the command.
 * \param[in] message  The message about to be sent to \p connection.
 */
bool server::verify_command(
          base_connection::pointer_t connection
        , ed::message const & msg)
{
    // debug turned on?
    //
    if(!f_debug_all_messages)
    {
        // nope, do not waste any more time
        return true;
    }

    if(!connection->has_commands())
    {
        // if we did not yet receive the COMMANDS message then we cannot
        // pretend that the understand_command() will return a sensible
        // result, so ignore that test...
        //
        return true;
    }

    if(connection->understand_command(msg.get_command()))
    {
        // all good, the command is implemented
        //
        return true;
    }

    // if you get this message, it could be that you do implement
    // the command, but do not advertise it in your COMMANDS
    // reply to the HELP message sent by communicatord
    //
    ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(connection));
    if(c != nullptr)
    {
        SNAP_LOG_ERROR
            << "connection \""
            << c->get_name()
            << "\" does not implement command \""
            << msg.get_command()
            << "\"."
            << SNAP_LOG_SEND;
    }
    else
    {
        SNAP_LOG_ERROR
            << "connection does not understand command \""
            << msg.get_command()
            << "\"."
            << SNAP_LOG_SEND;
    }

    return false;
}


/** \brief Process a message we just received.
 *
 * This function is called whenever a TCP or UDP message is received.
 * The function accepts all TCP messages, however, UDP messages are
 * limited to a very few such as STOP and SHUTDOWN. You will want to
 * check the documentation of each message to know whether it can
 * be sent over UDP or not.
 *
 * Note that the main reason why the UDP port is not allowed for most
 * messages is to send a reply you have to have TCP. This means responses
 * to those messages also need to be sent over TCP (because we could
 * not have sent an ACCEPT as a response to a CONNECT over a UDP
 * connection.)
 *
 * \param[in] msg  The message were were just sent.
 *
 * \return true if the forwarding went as planned.
 */
bool server::forward_message(ed::message & msg)
{
    //
    // the message includes a service name, so we want to forward that
    // message to that service
    //
    // for that purpose we consider the following three lists:
    //
    // 1. we have the service in our local services, we must forward it
    //    to that connection; if the connection is not up and running yet,
    //    cache the information
    //
    // 2. the service is not one of ours, but we found a remote
    //    communicatord server that says it is his, forward the
    //    message to that communicatord instead
    //
    // 3. the service is in the "heard of" list of services, send that
    //    message to that communicatord, it will then forward it
    //    to the correct server (or another proxy...)
    //
    // 4. the service cannot be found anywhere, we save it in our remote
    //    cache (i.e. because it will only be possible to send that message
    //    to a remote communicatord and not to a service on this system)
    //

    // if the destination server was specified, we have to forward
    // the message to that specific server
    //
    std::string const server_name(msg.get_server() == "." ? f_server_name : msg.get_server());
    std::string const service(msg.get_service());

#if 0
SNAP_LOG_VERBOSE
<< "---------------- forward message ["
<< msg.get_command()
<< "] for ["
<< server_name
<< "] / ["
<< service
<< "]"
<< SNAP_LOG_SEND;
#endif

    // broadcasting?
    //
    if(service == "*"
    || service == "?"
    || service == ".")
    {
        if(!server_name.empty()
        && server_name != "*"
        && (service == "*" || service == "?"))
        {
            // do not send the message in this case!
            //
            // we cannot at the same time send it to this local server
            // and broadcast it to other servers... it is contradictory;
            // either set the server to "*" or empty, or do not broadcast
            //
            SNAP_LOG_ERROR
                << "you cannot at the same time specify a server name ("
                << server_name
                << ") and \"*\" or \"?\" as the service."
                << SNAP_LOG_SEND;
            return false;
        }
        broadcast_message(msg);
        return true;
    }

    base_connection::vector_t accepting_remote_connections;
    bool const all_servers(server_name.empty() || server_name == "*");

    // service is local, check whether the service is registered,
    // if registered, forward the message immediately
    //
    ed::connection::vector_t const & connections(f_communicator->get_connections());
    for(auto const & nc : connections)
    {
        base_connection::pointer_t base_conn(std::dynamic_pointer_cast<base_connection>(nc));
        if(base_conn == nullptr)
        {
            continue;
        }

        // verify that there is a server name in all connections
        // (if not we have a bug somewhere else)
        //
        if(base_conn->get_server_name().empty())
        {
            if(!is_debug())
            {
                // ignore in non-debug versions because a throw
                // completely breaks communicatord... and it
                // is not that important at this point without
                // a programmer debugging this software
                //
                continue;
            }
            if(base_conn->get_connection_type() != connection_type_t::CONNECTION_TYPE_DOWN)
            {
                // not connected yet, forget about it
                continue;
            }

            service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(nc));
            if(conn != nullptr)
            {
                throw communicatord::missing_name(
                          "DEBUG: server name missing in connection \""
                        + conn->get_name()
                        + "\"...");
            }
            unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(nc));
            if(unix_conn != nullptr)
            {
                throw communicatord::missing_name(
                          "DEBUG: server name missing in connection \""
                        + unix_conn->get_name()
                        + "\"...");
            }

            switch(base_conn->get_connection_type())
            {
            case connection_type_t::CONNECTION_TYPE_DOWN:
                // not connected yet, forget about it
                continue;

            case connection_type_t::CONNECTION_TYPE_LOCAL:
                throw communicatord::missing_name(
                          "DEBUG: server name missing in connection \"local service\"...");

            case connection_type_t::CONNECTION_TYPE_REMOTE:
                throw communicatord::missing_name(
                          "DEBUG: server name missing in connection \"remote communicatord\"...");

            }
        }

        if(all_servers
        || server_name == base_conn->get_server_name())
        {
            bool is_service(false);
            {
                service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(nc));
                if(conn != nullptr)
                {
                    is_service = true;
                }
                else
                {
                    unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(nc));
                    if(unix_conn != nullptr)
                    {
                        is_service = true;
                    }
                }
            }
            if(is_service)
            {
                ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(nc));
                if(c->get_name() == service)
                {
                    // we have such a service, just forward to it now
                    //
                    // TBD: should we remove the service name from the
                    //      message before forwarding?
                    //
                    try
                    {
                        if(verify_command(base_conn, msg))
                        {
                            base_conn->send_message_to_connection(msg);
                        }
                        else
                        {
                            // helper message for programmers with attention
                            // span having issues
                            //
                            base_connection::pointer_t sender(msg.user_data<base_connection>());
                            if(base_conn == sender)
                            {
                                SNAP_LOG_WARNING
                                    << "service \""
                                    << service
                                    << "\" just tried to send itself a message. Forgot to change the destination service name?"
                                    << SNAP_LOG_SEND;
                            }
                        }
                    }
                    catch(std::runtime_error const & e)
                    {
                        // ignore the error because this can come from an
                        // external source (i.e. ed-signal) where an end
                        // user may try to break the whole system!
                        //
                        SNAP_LOG_DEBUG
                            << "communicatord failed to send a message to connection \""
                            << c->get_name()
                            << "\" (error: "
                            << e.what()
                            << ")"
                            << SNAP_LOG_SEND;
                    }
                    // we found a specific service to which we could
                    // forward the message so we can stop here
                    //
                    return false;
                }

                // if not a local connection with the proper name,
                // still send it to that connection but only if it
                // is a remote connection
                //
                connection_type_t const type(base_conn->get_connection_type());
                if(type == connection_type_t::CONNECTION_TYPE_REMOTE)
                {
                    accepting_remote_connections.push_back(base_conn);
                }
            }
            else
            {
                remote_connection::pointer_t new_remote_connection(std::dynamic_pointer_cast<remote_connection>(nc));
                // TODO: limit sending to remote only if they have that service?
                //       (if we have the 'all_servers' set, otherwise it is not
                //       required, for sure... also, if we have multiple remote
                //       connections that support the same service we should
                //       randomize which one is to receive that message--or
                //       even better, check the current server load--but
                //       seriously, if none of our direct connections know
                //       of that service, we need to check for those that heard
                //       of that service, and if that is also empty, send to
                //       all... for now we send to all anyway)
                //
                if(new_remote_connection != nullptr /*&& new_remote_connection->has_service(service)*/)
                {
                    accepting_remote_connections.push_back(new_remote_connection);
                }
            }
        }
    }

    if((all_servers || server_name == f_server_name)
    && f_local_services_list.find(service) != f_local_services_list.end())
    {
        // its a service that is expected on this computer, but it is not
        // running right now... so cache the message
        //
        if(f_local_message_cache.cache_message(msg) == cache_message_t::CACHE_MESSAGE_REPLY)
        {
            // let the sender know that the message was not forwarded to
            // a client
            //
            ed::message reply;
            reply.set_command("SERVICE_UNAVAILABLE");
            base_connection::pointer_t sender(msg.user_data<base_connection>());
            if(verify_command(sender, reply))
            {
                reply.add_parameter("destination_service", service);
                reply.add_parameter("unsent_command", msg.get_command());
                sender->send_message_to_connection(reply);
            }
            else
            {
                SNAP_LOG_NOTICE
                    << "a reply on unavailable service was requested, but \""
                    << service
                    << "\" does not support message SERVICE_UNAVAILABLE."
                    << SNAP_LOG_SEND;
            }
        }
        transmission_report(msg);
        return true;
    }

    // if attempting to send to self, we cannot go on from here
    //
    if(server_name == f_server_name)
    {
        SNAP_LOG_DEBUG
            << "received event \""
            << msg.get_command()
            << "\" for local service \""
            << service
            << "\", which is not currently registered with this"
               " communicatord. Dropping message."
            << SNAP_LOG_SEND;

        transmission_report(msg);
        return false;
    }

    if(!accepting_remote_connections.empty())
    {
        broadcast_message(msg, accepting_remote_connections);
    }

    return true;
}


void server::transmission_report(ed::message & msg)
{
    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    if(!msg.has_parameter("transmission_report"))
    {
        return;
    }

    std::string const report(msg.get_parameter("transmission_report"));
    if(report != "failure")
    {
        return;
    }

    ed::message reply;
    reply.set_command("TRANSMISSIONREPORT");
    reply.add_parameter("status", "failed");
    //verify_command(conn, reply);
    conn->send_message_to_connection(reply);
}


bool server::check_broadcast_message(ed::message const & msg)
{
    // messages being broadcast to us have a unique ID, if that ID is
    // one we already received we must ignore the message altogether;
    // also, a broadcast message has a timeout, we must ignore the
    // message if it already timed out
    //
    if(!msg.has_parameter("broadcast_msgid"))
    {
        return false;
    }

    // check whether the message already timed out
    //
    // this is a safety feature of our broadcasting capability
    // which should rarely be activated unless you have multiple
    // data center locations
    //
    time_t const timeout(msg.get_integer_parameter("broadcast_timeout"));
    time_t const now(time(nullptr));
    if(timeout < now)
    {
        return true;
    }

    // check whether we already received that message, if so ignore
    // the second instance (it should not happen with the list of
    // neighbors included in the message, but just in case...)
    //
    std::string const broadcast_msgid(msg.get_parameter("broadcast_msgid"));
    auto const received_it(f_received_broadcast_messages.find(broadcast_msgid));
    if(received_it != f_received_broadcast_messages.cend())     // message arrived again?
    {
        // note that although we include neighbors it is normal that
        // this happens in a cluster where some computers are not
        // aware of certain nodes; for example, if A sends a
        // message to B and C, both B and C know of a node D
        // which is unknown to A, then both B and C end up
        // forwarding that same message to D, so D will discard
        // the second instance it receives.
        //
        return true;
    }

    return false;
}


bool server::communicator_message(ed::message & msg)
{
    std::string const server_name(msg.get_server());
    if(!server_name.empty() 
    && server_name != "."       // this is an abbreviation meaning "f_server_name"
    && server_name != "*")
    {
        // message is not for the communicatord server
        //
        return false;
    }

    std::string const service(msg.get_service());
    if(!service.empty()
    && service != "communicatord")
    {
        // message is directed to another service
        //
        return false;
    }

    return true;
}


bool server::shutting_down(ed::message & msg)
{
    if(!f_shutdown)
    {
        return false;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn->is_udp())
    {
        return true;
    }

    // if the user sent us an UNREGISTER we should not generate a
    // QUITTING because the UNREGISTER is in reply to our STOP
    // TBD: we may want to implement the UNREGISTER in this
    //      situation?
    //
    if(msg.get_command() != "UNREGISTER")
    {
        // we are shutting down so just send a quick QUTTING reply
        // letting the other process know about it
        //
        ed::message reply;
        reply.set_command("QUITTING");
        if(verify_command(conn, reply))
        {
            conn->send_message_to_connection(reply);
        }
    }

    // get rid of that connection now, we don't need any more
    // messages coming from it
    //
    f_communicator->remove_connection(std::dynamic_pointer_cast<ed::connection>(conn));

    return true;
}


/** \brief Overload the dispatch_message().
 *
 * The default implementation of dispatch_message() works for all our
 * services except the communicatord which wants to know whether a message
 * was being broadcasted, whether it is actively shutting down, or whether
 * the message is for the communicatord service itself or another service.
 */
bool server::dispatch_message(ed::message & msg)
{
    // check whether this is a timed out or already processed broadcast
    // message
    //
    if(check_broadcast_message(msg))
    {
        // pretend the message was processed
        //
        return true;
    }

    // we are shutting down, ignore further messages
    //
    if(shutting_down(msg))
    {
        // pretend the message was processed
        //
        return true;
    }

    // if this message is directed to the communicator daemon itself
    // then dispatcher it through our own dispatcher
    //
    if(communicator_message(msg))
    {
        return dispatcher_support::dispatch_message(msg);
    }

    // otherwise call `forward_message()`
    //
    return forward_message(msg);
}


bool server::is_tcp_connection(ed::message & msg)
{
    if(msg.user_data<base_connection>()->is_udp())
    {
        SNAP_LOG_ERROR
            << msg.get_command()
            << " is only accepted over a TCP connection."
            << SNAP_LOG_SEND;
        return false;
    }

    return true;
}


void server::msg_accept(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    // the following are mandatory in an ACCEPT message
    //
    if(!msg.has_parameter("server_name")
    || !msg.has_parameter("my_address"))
    {
        SNAP_LOG_ERROR
            << "ACCEPT was received without the \"server_name\" and \"my_address\" parameters, which are mandatory."
            << SNAP_LOG_SEND;
        return;
    }

    // get the remote server name
    //
    conn->set_connection_type(connection_type_t::CONNECTION_TYPE_REMOTE);
    std::string const & remote_server_name(msg.get_parameter("server_name"));
    conn->set_server_name(remote_server_name);

    // reply to a CONNECT, this was to connect to another
    // communicatord on another computer, retrieve the
    // data from that remote computer
    //
    conn->connection_started();
    std::string const his_address_str(msg.get_parameter("my_address"));
    addr::addr his_address(addr::string_to_addr(
              his_address_str
            , "255.255.255.255"
            , communicatord::REMOTE_PORT   // REMOTE_PORT or SECURE_PORT?
            , "tcp"));
    conn->set_connection_address(his_address);

    if(msg.has_parameter("services"))
    {
        conn->set_services(msg.get_parameter("services"));
    }
    if(msg.has_parameter("heard_of"))
    {
        conn->set_services_heard_of(msg.get_parameter("heard_of"));
    }
    if(msg.has_parameter("neighbors"))
    {
        add_neighbors(msg.get_parameter("neighbors"));
    }

    // we just got some new services information,
    // refresh our cache
    //
    refresh_heard_of();

    // also request the COMMANDS of this connection with a HELP message
    //
    ed::message help;
    help.set_command("HELP");
    //verify_command(base, help); -- precisely
    conn->send_message_to_connection(help);

    // if a local service was interested in this specific
    // computer, then we have to start receiving LOADAVG
    // messages from it
    //
    register_for_loadavg(his_address_str);

    // now let local services know that we have a new
    // remote connections (which may be of interest
    // for that service--see snapmanagerdaemon)
    //
    // TODO: to be symmetrical, we should also have
    //       a message telling us when a remote
    //       connection goes down...
    //
    ed::message new_remote_connection;
    new_remote_connection.set_command("NEWREMOTECONNECTION");
    new_remote_connection.set_service(".");
    new_remote_connection.add_parameter("server_name", remote_server_name);
    broadcast_message(new_remote_connection);
}


void server::msg_clusterstatus(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    cluster_status(std::dynamic_pointer_cast<ed::connection>(conn));
}


void server::msg_commands(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }
    ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(conn));

    if(!msg.has_parameter("list"))
    {
        SNAP_LOG_ERROR
            << "COMMANDS was sent without a \"list\" parameter."
            << SNAP_LOG_SEND;
        return;
    }
    conn->set_commands(msg.get_parameter("list"));

    // in normal circumstances, we're done
    //
    if(!is_debug())
    {
        return;
    }

    // here we verify that a few commands are properly defined; for some,
    // we already sent them to that connection and thus it should understand
    // them no matter what; and a few more that are very possibly going to
    // be sent later (i.e. all are part of the communicatord protocol)
    //
    bool ok(true);
    if(!conn->understand_command("HELP"))
    {
        SNAP_LOG_FATAL
            << "connection \""
            << c->get_name()
            << "\" does not understand HELP."
            << SNAP_LOG_SEND;
        ok = false;
    }
    if(!conn->understand_command("QUITTING"))
    {
        SNAP_LOG_FATAL
            << "connection \""
            << c->get_name()
            << "\" does not understand QUITTING."
            << SNAP_LOG_SEND;
        ok = false;
    }
    // on a remote we get ACCEPT instead of READY
    if(std::dynamic_pointer_cast<remote_connection>(conn) != nullptr
    || conn->is_remote())
    {
        if(!conn->understand_command("ACCEPT"))
        {
            SNAP_LOG_FATAL
                << "connection \""
                << c->get_name()
                << "\" does not understand ACCEPT."
                << SNAP_LOG_SEND;
            ok = false;
        }
    }
    else
    {
        if(!conn->understand_command("READY"))
        {
            SNAP_LOG_FATAL
                << "connection \""
                << c->get_name()
                << "\" does not understand READY."
                << SNAP_LOG_SEND;
            ok = false;
        }
    }
    if(!conn->understand_command("STOP"))
    {
        SNAP_LOG_FATAL
            << "connection \""
            << c->get_name()
            << "\" does not understand STOP."
            << SNAP_LOG_SEND;
        ok = false;
    }
    if(!conn->understand_command("UNKNOWN"))
    {
        SNAP_LOG_FATAL
            << "connection \""
            << c->get_name()
            << "\" does not understand UNKNOWN."
            << SNAP_LOG_SEND;
        ok = false;
    }
    if(!ok)
    {
        // end the process so developers can fix their problems
        // (this is only if --debug-all-messages was specified)
        //
        throw communicatord::missing_message(
                  "DEBUG: Connection \""
                + c->get_name()
                + "\" does not implement some of the required commands. See logs for more details.");
    }
}


void server::msg_connect(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    // the CONNECT message has three mandatory parameters
    //
    if(!msg.has_parameter("version")
    || !msg.has_parameter("my_address")
    || !msg.has_parameter("server_name"))
    {
        SNAP_LOG_ERROR
            << "CONNECT was sent without a \"version\", \"my_address\", or \"server_name\" parameter, all are mandatory."
            << SNAP_LOG_SEND;
        return;
    }

    // TODO: the username:password need to be encrypted so we actually want
    //       to change this scheme and use our password service which includes
    //       the necessary encryption and resides on a separate computer not
    //       publicly accessible
    //
    std::string const username(conn->get_username());
    if(!username.empty())
    {
        if(!msg.has_parameter("username")
        || !msg.has_parameter("password"))
        {
            SNAP_LOG_ERROR
                << "CONNECT on this connection is required to include a \"username\" and a \"password\"."
                << SNAP_LOG_SEND;
            return;
        }
        std::string const password(conn->get_password());
        if(username != msg.get_parameter("username")
        || password != msg.get_parameter("password"))
        {
            SNAP_LOG_ERROR
                << "invalid CONNECT credentials for "
                << conn->get_connection_address()
                << "; please verify your username and password information."
                << SNAP_LOG_SEND;

            // TODO: count and after 3 attempts blacklist the client's IP
            //       address for a while...
            //
            service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(conn));
            if(service_conn != nullptr)
            {
                service_conn->block_ip();
            }
            return;
        }
    }

    // first we verify that we have a compatible version to
    // communicate between two communicators
    //
    if(!msg.check_version_parameter())
    {
        SNAP_LOG_ERROR
            << "CONNECT was sent with an incompatible version. Expected "
            << ed::MESSAGE_VERSION
            << ", received "
            << msg.get_message_version()
            << '.'
            << SNAP_LOG_SEND;
        return;
    }

    ed::message reply;
    ed::message new_remote_connection;

    std::string const remote_server_name(msg.get_parameter("server_name"));
    ed::connection::vector_t const all_connections(f_communicator->get_connections());
    ed::connection::pointer_t ed_conn(std::dynamic_pointer_cast<ed::connection>(conn));
    auto const & name_match(std::find_if(
            all_connections.begin(),
            all_connections.end(),
            [remote_server_name, ed_conn](auto const & it)
            {
                // ignore ourselves
                //
                if(it == ed_conn)
                {
                    return false;
                }
                base_connection::pointer_t b(std::dynamic_pointer_cast<base_connection>(it));
                if(!b)
                {
                    return false;
                }
                return remote_server_name == b->get_server_name();
            }));
    bool refuse(name_match != all_connections.end());

    if(refuse)
    {
        SNAP_LOG_ERROR
            << "CONNECT from \""
            << remote_server_name
            << "\" but we already have another computer using that same name."
            << SNAP_LOG_SEND;

        reply.set_command("REFUSE");
        reply.add_parameter("conflict", "name");

        // we may also be shutting down
        //
        // Note: we cannot get here if f_shutdown is true...
        //
        if(f_shutdown)
        {
            reply.add_parameter("shutdown", "true");
        }
    }
    else
    {
        conn->set_server_name(remote_server_name);

        // add neighbors with which the guys asking to
        // connect can attempt to connect with...
        //
        if(!f_explicit_neighbors.empty())
        {
            reply.add_parameter("neighbors", f_explicit_neighbors);
        }

        // Note: we cannot get here if f_shutdown is true...
        //
        refuse = f_shutdown;
        if(refuse)
        {
            // okay, this guy wants to connect to us but we
            // are shutting down, so refuse and put the shutdown
            // flag to true
            //
            reply.set_command("REFUSE");
            reply.add_parameter("shutdown", "true");
        }
        else
        {
            // cool, a remote communicatord wants to connect
            // with us, make sure we did not reach the maximum
            // number of connections though...
            //
            refuse = all_connections.size() >= f_max_connections;
            if(refuse)
            {
                // too many connections already, refuse this new
                // one from a remove system
                //
                reply.set_command("REFUSE");
            }
            else
            {
                // set the connection type if we are not refusing it
                //
                conn->set_connection_type(connection_type_t::CONNECTION_TYPE_REMOTE);

                // same as ACCEPT (see above) -- maybe we could have
                // a sub-function...
                //
                conn->connection_started();

                if(msg.has_parameter("services"))
                {
                    conn->set_services(msg.get_parameter("services"));
                }
                if(msg.has_parameter("heard_of"))
                {
                    conn->set_services_heard_of(msg.get_parameter("heard_of"));
                }
                if(msg.has_parameter("neighbors"))
                {
                    add_neighbors(msg.get_parameter("neighbors"));
                }

                // we just got some new services information,
                // refresh our cache
                //
                refresh_heard_of();

                // the message expects the ACCEPT reply
                //
                reply.set_command("ACCEPT");
                reply.add_parameter("server_name", f_server_name);
                reply.add_parameter("my_address", f_connection_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));

                // services
                //
                if(!f_local_services.empty())
                {
                    reply.add_parameter("services", f_local_services);
                }

                // heard of
                //
                if(!f_services_heard_of.empty())
                {
                    reply.add_parameter("heard_of", f_services_heard_of);
                }

                std::string const his_address_str(msg.get_parameter("my_address"));
                addr::addr his_address(addr::string_to_addr(
                          his_address_str
                        , "255.255.255.255"
                        , communicatord::REMOTE_PORT   // REMOTE_PORT or SECURE_PORT?
                        , "tcp"));

                conn->set_connection_address(his_address);

                // if a local service was interested in this specific
                // computer, then we have to start receiving LOADAVG
                // messages from it
                //
                register_for_loadavg(his_address_str);

                // he is a neighbor too, make sure to add it
                // in our list of neighbors (useful on a restart
                // to connect quickly)
                //
                add_neighbors(his_address_str);

                // since we are accepting a CONNECT we have to make
                // sure we cancel the GOSSIP events to that remote
                // connection; it won't hurt, but it is a waste if
                // we do not need it
                //
                // Note: the name of the function is "GOSSIP"
                //       received because if the "RECEIVED"
                //       message was sent back from that remote
                //       communicatord then it means that
                //       remote daemon received our GOSSIP message
                //       and receiving the "CONNECT" message is
                //       very similar to receiving the "RECEIVED"
                //       message after a "GOSSIP"
                //
                f_remote_communicators->gossip_received(his_address);

                // now let local services know that we have a new
                // remote connections (which may be of interest
                // for that service--see managerdaemon)
                //
                // TODO: to be symmetrical, we should also have
                //       a message telling us when a remote
                //       connection goes down...
                //
                new_remote_connection.set_command("NEWREMOTECONNECTION");
                new_remote_connection.set_service(".");
                new_remote_connection.add_parameter("server_name", remote_server_name);
            }
        }
    }

    //verify_command(base, reply); -- we do not yet have a list of commands understood by the other communicator daemon

    // also request the COMMANDS of this connection with a HELP
    // if the connection was not refused
    //
    ed::message help;
    help.set_command("HELP");
    //verify_command(base, help); -- precisely
    conn->send_message_to_connection(reply);
    if(!refuse)
    {
        conn->send_message_to_connection(help);
        broadcast_message(new_remote_connection);
    }

    // if not refused, then we may have a QUORUM now, check
    // that; the function we call takes care of knowing
    // whether we reach cluster status or not
    //
    if(!refuse)
    {
        cluster_status(nullptr);
    }

    // status changed for this connection
    //
    send_status(std::dynamic_pointer_cast<ed::connection>(conn));
}


void server::msg_disconnect(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    conn->connection_ended();

    // this has to be another communicatord
    // (i.e. an object that sent ACCEPT or CONNECT)
    //
    connection_type_t const type(conn->get_connection_type());
    if(type == connection_type_t::CONNECTION_TYPE_REMOTE)
    {
        // we must ignore and we do ignore connections with a
        // type of "" since they represent an uninitialized
        // connection item (unconnected)
        //
        conn->set_connection_type(connection_type_t::CONNECTION_TYPE_DOWN);

        remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(conn));
        if(remote_conn == nullptr)
        {
            // disconnecting means it is gone so we can remove
            // it from the communicator since the other end
            // will reconnect (we are never responsible
            // for that in this case)
            //
            // Note: this one happens when the computer that
            //       sent us a CONNECT later sends us the
            //       DISCONNECT
            //
            f_communicator->remove_connection(std::dynamic_pointer_cast<ed::connection>(conn));

// devise a test to verify this statement and if correct ensure we avoid the
// extra call
//
SNAP_LOG_TODO
<< "I think that since we remove the connection here, the send_status() at the bottom of the function is rendered useless since the connection will be gone, the write() to the socket will never happen."
<< SNAP_LOG_SEND;
        }
        else
        {
            // in this case we are in charge of attempting
            // to reconnect until it works... however, it
            // is likely that the other side just shutdown
            // so we want to "induce a long enough pause"
            // to avoid attempting to reconnect like crazy
            //
            remote_conn->disconnect();
            f_remote_communicators->shutting_down(remote_conn->get_address());
        }

        // we may have lost some services information,
        // refresh our cache
        //
        refresh_heard_of();

        if(!conn->get_server_name().empty())
        {
            ed::message disconnected;
            disconnected.set_command("DISCONNECTED");
            disconnected.set_service(".");
            disconnected.add_parameter("server_name", conn->get_server_name());
            broadcast_message(disconnected);
        }

        cluster_status(nullptr);
    }
    else
    {
        SNAP_LOG_ERROR
            << "DISCONNECT was sent from a connection which is not of the right type ("
            << (type == connection_type_t::CONNECTION_TYPE_DOWN ? "down" : "client")
            << ")."
            << SNAP_LOG_SEND;
    }

    // status changed for this connection
    //
    send_status(std::dynamic_pointer_cast<ed::connection>(conn));
}


void server::msg_forget(ed::message & msg)
{
    if(!msg.has_parameter("ip"))
    {
        SNAP_LOG_ERROR
            << "the ip=... parameter is missing in the FORGET message"
            << SNAP_LOG_SEND;
        return;
    }

    // whenever computers connect between each others, their
    // IP address gets added to our list of neighbors; this
    // means that the IP address is now stuck in the
    // computer's brain "forever"
    //
    std::string const forget_ip(msg.get_parameter("ip"));

    // self is not a connection that get broadcast messages
    // for communicatord, so we also call the remove_neighbor()
    // function now
    //
    remove_neighbor(forget_ip);

    // once you notice many connection errors to other computers
    // that have been removed from your cluster, you want the
    // remaining computers to forget about that IP address and
    // it is done by broadcasting a FORGET message to everyone
    //
    if(msg.has_parameter("broadcast_hops"))
    {
        return;
    }

    // this was sent directly to this instance only,
    // make sure to broadcast the message instead
    //
    ed::message forget;
    forget.set_command("FORGET");
    forget.set_server("*");
    forget.set_service("communicatord");
    forget.add_parameter("ip", forget_ip);
    broadcast_message(forget);
}


void server::msg_gossip(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    // we got a GOSSIP message, this one will have addresses
    // with various neighbors; we have two modes:
    //
    // 1) my_address=... is defined -- in this case the
    //    remote host sent us his address because he was
    //    not sure whether we knew about him; add that
    //    address as a neighbor and go on as normal
    //
    // 2) heard_of=... is defined -- in this case, the
    //    remote host received a GOSSIP from any one
    //    communicatord and it is propagating the
    //    message; check all the IPs in that list and
    //    if all are present in our list of neighbors,
    //    do nothing; if all are not present, proceed
    //    as normal in regard to attempt connections
    //    and also forward our own GOSSIP to others
    //    since we just heard of some new neighbors!
    //
    //    Note that at this point we use the Flooding
    //    scheme and we implemented the Eventual
    //    Consistency (because at some point in time
    //    we eventually have an exact result.)
    //
    // When using (2) we are using what is called
    // Gossiping in Computer Science. At thist time
    // we use what is called the Flooding Algorithm.
    //
    // https://en.wikipedia.org/wiki/Flooding_(computer_networking)
    //
    // See also doc/focs2003-gossip.pdf
    //
    // We add two important features: (a) the list of
    // nodes we already sent the message to, in
    // order to avoid sending it to the same node
    // over and over again; and (b) a serial number
    // to be able to identify the message.
    //
    // Two other features that could be added are:
    // (c) counting hops, after X hops were reached,
    // stop forwarding the message because we should
    // already have reached all nodes; (d) a specific
    // date when the message times out.
    //
    // The serial number is used to know whether we
    // already received a certain message. These can
    // expire after a while (we may actually want to
    // implement (d) from the get go so we know
    // exactly when such expires).
    //
    // Our GOSSIP has one advantage, it is used to
    // connect all the communicators together
    // once. After that, the GOSSIP messages stop,
    // no matter what (i.e. if a new communicator
    // daemon is started, then the GOSSIP restart
    // for that instance, but that's it.)
    //
    // However, we also offer a way to broadcast
    // messages and these happen all the time
    // (i.e. think of the snaplock broadcast messages).
    // In those cases, we do not need to use the same
    // algorithm because at that point we are expected
    // to have a complete list of all the
    // communicators available.
    //
    // (TODO: only we may not be directly connected to all of them,
    // so we need to keep track of the communicators
    // we are not connected to and ask others to do some
    // forwarding!)
    //
    if(msg.has_parameter("my_address"))
    {
        // this is a "simple" GOSSIP of a communicatord
        // telling us it exists and expects a connection
        // from us
        //
        // in this case we just reply with RECEIVED to
        // confirm that we get the GOSSIP message
        //
        std::string const reply_to(msg.get_parameter("my_address"));
        add_neighbors(reply_to);

        ed::message reply;
        reply.set_command("RECEIVED");
        //verify_command(base, reply); -- in this case the remote
        //                                communicatord is not connected,
        //                                so no HELP+COMMANDS and thus no
        //                                verification possible
        conn->send_message_to_connection(reply);
        return;
    }

    if(msg.has_parameter("heard_of"))
    {
        SNAP_LOG_ERROR
            << snaplogger::section(snaplogger::g_not_implemented_component)
            << snaplogger::section(snaplogger::g_debug_component)
            << "GOSSIP is not yet fully implemented. heard_of=... not available."
            << SNAP_LOG_SEND;
        return;
    }

    SNAP_LOG_ERROR
        << "GOSSIP must have my_address=... or heard_of=... defined."
        << SNAP_LOG_SEND;
}


void server::msg_list_services(ed::message & msg)
{
    snapdev::NOT_USED(msg);

    ed::connection::vector_t const & all_connections(f_communicator->get_connections());
    std::string list;
    std::for_each(
            all_connections.begin(),
            all_connections.end(),
            [&list](auto const & c)
            {
                if(!list.empty())
                {
                    list += ", ";
                }
                list += c->get_name();
            });
    SNAP_LOG_INFO
        << "current list of connections: "
        << list
        << SNAP_LOG_SEND;

    // TODO: send a reply so communicators can know of discrepancies
}


void server::msg_log_unknown(ed::message & msg)
{
    // we sent a command that the other end did not understand
    // and got an UNKNOWN reply
    //
    std::string name("<unknown-connection>");
    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn != nullptr)
    {
        ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(conn));
        if(c != nullptr)
        {
            name = c->get_name();
        }
    }

    if(msg.has_parameter("command"))
    {
        SNAP_LOG_ERROR
            << "we sent command \""
            << msg.get_parameter("command")
            << "\" to \""
            << name
            << "\" which told us it does not know that command"
               " so we probably did not get the expected result."
            << SNAP_LOG_SEND;
    }
    else
    {
        SNAP_LOG_ERROR
            << "we sent a command (name of which was not reported in"
               " the \"command\" paramter) to "
            << msg.get_parameter("command")
            << "\" to \""
            << name
            << "\" which told us it does not know that command"
               " so we probably did not get the expected result."
            << SNAP_LOG_SEND;
    }
}


void server::msg_public_ip(ed::message & msg)
{
    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    ed::message reply;
    reply.set_command("SERVER_PUBLIC_IP");
    if(!f_public_ip.empty())
    {
        reply.add_parameter("public_ip", f_public_ip);
    }
    if(!f_secure_ip.empty())
    {
        reply.add_parameter("secure_ip", f_secure_ip);
    }
    if(verify_command(conn, reply))
    {
        conn->send_message_to_connection(reply);
    }
}


void server::msg_quitting(ed::message & msg)
{
    snapdev::NOT_USED(msg);

    // the other services will call their stop() function, but the
    // communicatord does not do that on this message; just inform
    // the admin with a quick log message
    //
    SNAP_LOG_INFO
        << "Received a QUITTING as a reply to a message."
        << SNAP_LOG_SEND;
}


void server::msg_refuse(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(conn));
    if(remote_conn == nullptr)
    {
        // we have to have a remote connection
        //
        SNAP_LOG_ERROR
            << "REFUSE sent on a connection which is not a remote connection (another communicatord)."
            << SNAP_LOG_SEND;
        return;
    }

    // we were not connected so we do not have to
    // disconnect; mark that corresponding server
    // as too busy or as shutting down and try
    // connecting again later...
    //
    addr::addr peer_addr(remote_conn->get_address());
    if(msg.has_parameter("shutdown"))
    {
        f_remote_communicators->shutting_down(peer_addr);
    }
    else
    {
        f_remote_communicators->too_busy(peer_addr);
    }

    // we are responsible to try again later, so we do not
    // lose the connection (remove it from the ed::communicator),
    // but we need to disconnect
    //
    remote_conn->disconnect();
}


void server::msg_register(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    if(!msg.has_parameter("service")
    || !msg.has_parameter("version"))
    {
        SNAP_LOG_ERROR
            << "REGISTER was called without a \"service\" and/or a \"version\" parameter, both are mandatory."
            << SNAP_LOG_SEND;
        return;
    }

    if(!msg.check_version_parameter())
    {
        SNAP_LOG_ERROR
            << "REGISTER was called with an incompatible version; expected "
            << ed::MESSAGE_VERSION
            << ", received "
            << msg.get_message_version()
            << '.'
            << SNAP_LOG_SEND;
        return;
    }

    // the "service" parameter is the name of the service,
    // now we can process messages for this service
    //
    std::string const service_name(msg.get_parameter("service"));
    if(service_name.empty())
    {
        SNAP_LOG_ERROR
            << "REGISTER had a \"service\" parameter, but it is empty, which is not valid."
            << SNAP_LOG_SEND;
        return;
    }

    {
        unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(conn));
        if(unix_conn == nullptr)
        {
            service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(conn));
            if(service_conn == nullptr)
            {
                SNAP_LOG_ERROR
                    << "only local services are expected to REGISTER with the communicatord service."
                    << SNAP_LOG_SEND;
                return;
            }
            service_conn->properly_named();
        }
        else
        {
            unix_conn->properly_named();
        }
    }

    ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(conn));
    if(c == nullptr)
    {
        throw communicatord::logic_error("the service_connection and unix_connection are both derived from ed::connection, c == nullptr should never happen.");
    }

    SNAP_LOG_VERBOSE
        << "service named \""
        << service_name
        << "\" just registered."
        << SNAP_LOG_SEND;

    c->set_name(service_name);

    conn->set_connection_type(connection_type_t::CONNECTION_TYPE_LOCAL);

    // connection is up now
    //
    conn->connection_started();

    // request the COMMANDS of this connection
    //
    ed::message help;
    help.set_command("HELP");
    //verify_command(base, help); -- we cannot do that here since we did not yet get the COMMANDS reply
    conn->send_message_to_connection(help);

    // tell the connection we are ready
    // (many connections use that as a trigger to start work)
    //
    ed::message reply;
    reply.set_command("READY");
    reply.add_parameter("my_address", f_connection_address);
    //verify_command(base, reply); -- we cannot do that here since we did not yet get the COMMANDS reply
    conn->send_message_to_connection(reply);

    // status changed for this connection
    //
    send_status(c);

    // if we have local messages that were cached, then
    // forward them now
    //
    f_local_message_cache.process_messages(
        [service_name, conn](ed::message & cached_msg)
        {
            if(cached_msg.get_service() != service_name)
            {
                return false;
            }

            conn->send_message_to_connection(cached_msg);
            return true;
        });
}


void server::msg_registerforloadavg(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    conn->set_wants_loadavg(true);
    f_loadavg_timer->set_enable(true);
}


void server::msg_servicestatus(ed::message & msg)
{
    if(!msg.has_parameter("service"))
    {
        SNAP_LOG_ERROR
            << "The SERVICESTATUS service parameter is mandatory."
            << SNAP_LOG_SEND;
        return;
    }
    std::string const & service_name(msg.get_parameter("service"));
    if(service_name.empty())
    {
        SNAP_LOG_ERROR
            << "The SERVICESTATUS service parameter cannot be an empty string."
            << SNAP_LOG_SEND;
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    ed::connection::vector_t const & named_connections(f_communicator->get_connections());
    auto const named_service(std::find_if(
                  named_connections.begin()
                , named_connections.end()
                , [service_name](auto const & named_connection)
                {
                    return named_connection->get_name() == service_name;
                }));
    ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(conn));
    if(named_service == named_connections.end())
    {
        // service is totally unknown
        //
        // create a fake connection so we can call the
        // send_status() function
        //
        ed::connection::pointer_t fake_connection(std::make_shared<ed::timer>(0));
        fake_connection->set_name(service_name);
        send_status(fake_connection, &c);
    }
    else
    {
        send_status(*named_service, &c);
    }
}


void server::msg_shutdown(ed::message & msg)
{
    snapdev::NOT_USED(msg);

    stop(true);
}


void server::msg_unregister(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    if(!msg.has_parameter("service"))
    {
        SNAP_LOG_ERROR
            << "UNREGISTER was called without a \"service\" parameter, which is mandatory."
            << SNAP_LOG_SEND;
        return;
    }

    // also remove all the connection types
    // an empty string represents an unconnected item
    //
    conn->set_connection_type(connection_type_t::CONNECTION_TYPE_DOWN);

    // connection is down now
    //
    conn->connection_ended();

    // status changed for this connection
    //
    ed::connection::pointer_t c(std::dynamic_pointer_cast<ed::connection>(conn));
    if(c != nullptr)
    {
        send_status(c);

        // now remove the service name
        // (send_status() needs the name to still be in place!)
        //
        c->set_name(std::string());

        // get rid of that connection now (it is faster than
        // waiting for the HUP because it will not be in the
        // list of connections on the next loop).
        //
        f_communicator->remove_connection(c);
    }
}


void server::msg_unregisterforloadavg(ed::message & msg)
{
    if(!is_tcp_connection(msg))
    {
        return;
    }

    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

    conn->set_wants_loadavg(false);

    // check whether all connections are now unregistered
    //
    ed::connection::vector_t const & all_connections(f_communicator->get_connections());
    if(all_connections.end() == std::find_if(
            all_connections.begin(),
            all_connections.end(),
            [](auto const & c)
            {
                base_connection::pointer_t b(std::dynamic_pointer_cast<base_connection>(c));
                return b->wants_loadavg();
            }))
    {
        // no more connections requiring LOADAVG messages
        // so stop the timer
        //
        f_loadavg_timer->set_enable(false);
    }
}




void server::broadcast_message(
      ed::message & msg
    , base_connection::vector_t const & accepting_remote_connections)
{
    std::string broadcast_msgid;
    std::string informed_neighbors;
    int hops(0);
    time_t timeout(0);

    // note: the "broacast_msgid" is required when we end up sending that
    //       message forward to some other computers; so we have to go
    //       through that if() block; however, the timeout was already
    //       already checked, so we probably would not need it to do
    //       it again?
    //
    if(msg.has_parameter("broadcast_msgid"))
    {
        // check whether the message already timed out
        //
        // this is a safety feature of our broadcasting capability
        // which should rarely be activated unless you have multiple
        // data center locations
        //
        timeout = msg.get_integer_parameter("broadcast_timeout");
        time_t const now(time(nullptr));
        if(timeout < now)
        {
            return;
        }

        // check whether we already received that message, if so ignore
        // the second instance (it should not happen with the list of
        // neighbors included in the message, but just in case...)
        //
        broadcast_msgid = msg.get_parameter("broadcast_msgid");
        auto const received_it(f_received_broadcast_messages.find(broadcast_msgid));
        if(received_it != f_received_broadcast_messages.cend())     // message arrived again?
        {
            // note that although we include neighbors it is normal that
            // this happens in a cluster where some computers are not
            // aware of certain nodes; for example, if A sends a
            // message to B and C, both B and C know of a node D
            // which is unknown to A, then both B and C will end
            // up forward that same message to D, so D will discard
            // the second instance it receives.
            //
            return;
        }

        // delete "received messages" that have now timed out (because
        // such are not going to be forwarded since we check the timeout
        // of a message early and prevent the broadcasting in that case)
        //
        // XXX: I am thinking that this loop should probably be run before
        //      the "broadcast_timeout" test above...
        //
        for(auto it(f_received_broadcast_messages.cbegin()); it != f_received_broadcast_messages.cend(); )
        {
            if(it->second < now)
            {
                // this one timed out, we do not need to keep it in this
                // list, so erase
                //
                it = f_received_broadcast_messages.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // add the new message after we check for timed out entries
        // so that way we avoid going through this new entry within
        // the previous loop
        //
        f_received_broadcast_messages[broadcast_msgid] = timeout;

        // Note: we skip the canonicalization on this list of neighbors
        //       because we assume only us (communicatord) handles
        //       that message and we know that it is already
        //       canonicalized here
        //
        informed_neighbors = msg.get_parameter("broadcast_informed_neighbors");

        // get the number of hops this message already performed
        //
        hops = msg.get_integer_parameter("broadcast_hops");
    }

    sorted_list_of_strings_t informed_neighbors_list;
    snapdev::tokenize_string(
              informed_neighbors_list
            , informed_neighbors
            , { "," }
            , true);

    // we always broadcast to all local services
    //
    ed::connection::vector_t broadcast_connection;

    if(accepting_remote_connections.empty())
    {
        std::string destination("?");
        std::string const service(msg.get_service());
        if(service != "."
        && service != "?"
        && service != "*")
        {
            destination = msg.get_server();
            if(destination.empty())
            {
                destination = "?";
            }
        }
        else
        {
            destination = service;
        }
        bool const all(hops < 5 && destination == "*");
        bool const remote(hops < 5 && (all || destination == "?"));

        ed::connection::vector_t const & connections(f_communicator->get_connections());
        for(auto const & nc : connections)
        {
            // try for a service or communicatord that connected to us
            //
            service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(nc));
            remote_connection::pointer_t remote_conn;
            if(!conn)
            {
                remote_conn = std::dynamic_pointer_cast<remote_connection>(nc);
            }
            bool broadcast(false);
            if(conn != nullptr)
            {
                switch(conn->get_address().get_network_type())
                {
                case addr::network_type_t::NETWORK_TYPE_LOOPBACK:
                    // these are localhost services, avoid sending the
                    // message is the destination does not know the
                    // command
                    //
                    if(conn->understand_command(msg.get_command())) // destination: "*" or "?" or "."
                    {
                        //verify_command(conn, message); -- we reach this line only if the command is understood, it is therefore good
                        conn->send_message(msg);
                    }
                    break;

                case addr::network_type_t::NETWORK_TYPE_PRIVATE:
                    // these are computers within the same local network (LAN)
                    // we forward messages if at least 'remote' is true
                    //
                    broadcast = remote; // destination: "*" or "?"
                    break;

                case addr::network_type_t::NETWORK_TYPE_PUBLIC:
                    // these are computers in another data center
                    // we forward messages only when 'all' is true
                    //
                    broadcast = all; // destination: "*"
                    break;

                default:
                    // unknown/unexpected type of IP address, totally ignore
                    break;

                }
            }
            else if(remote_conn != nullptr)
            {
                // another communicatord that connected to us
                //
                switch(remote_conn->get_address().get_network_type())
                {
                case addr::network_type_t::NETWORK_TYPE_LOOPBACK:
                    {
                        static bool warned(false);
                        if(!warned)
                        {
                            warned = true;
                            SNAP_LOG_WARNING
                                << "remote communicator was connected on a LOOPBACK IP address..."
                                << SNAP_LOG_SEND;
                        }
                    }
                    break;

                case addr::network_type_t::NETWORK_TYPE_PRIVATE:
                    // these are computers within the same local network (LAN)
                    // we forward messages if at least 'remote' is true
                    //
                    broadcast = remote; // destination: "*" or "?"
                    break;

                case addr::network_type_t::NETWORK_TYPE_PUBLIC:
                    // these are computers in another data center
                    // we forward messages only when 'all' is true
                    //
                    broadcast = all; // destination: "*"
                    break;

                default:
                    // unknown/unexpected type of IP address, totally ignore
                    break;

                }
            }
            if(broadcast)
            {
                // get the IP address of the remote communicatord
                //
                std::string const address((conn != nullptr
                            ? conn->get_address()
                            : remote_conn->get_address()).to_ipv4or6_string(addr::STRING_IP_ADDRESS));
                auto it(informed_neighbors_list.find(address));
                if(it == informed_neighbors_list.end())
                {
                    // not in the list of informed neighbors, add it and
                    // keep nc in a list that we can use to actually send
                    // the broadcast message
                    //
                    informed_neighbors_list.insert(address);
                    broadcast_connection.push_back(nc);
                }
            }
        }
    }
    else
    {
        // we already have a list, copy that list only as it is already
        // well defined
        //
        std::for_each(
            accepting_remote_connections.begin(),
            accepting_remote_connections.end(),
            [&broadcast_connection, &informed_neighbors_list](auto const & nc)
            {
                // the dynamic_cast<>() should always work in this direction
                //
                service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(nc));
                if(conn)
                {
                    std::string const address(conn->get_address().to_ipv4or6_string(addr::STRING_IP_ADDRESS));
                    auto it(informed_neighbors_list.find(address));
                    if(it == informed_neighbors_list.end())
                    {
                        // not in the list of informed neighbors, add it and
                        // keep conn in a list that we can use to actually
                        // send the broadcast message
                        //
                        informed_neighbors_list.insert(address);
                        broadcast_connection.push_back(conn);
                    }
                }
                else
                {
                    remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(nc));
                    if(remote_conn != nullptr)
                    {
                        std::string const address(remote_conn->get_address().to_ipv4or6_string(addr::STRING_IP_ADDRESS));
                        auto it(informed_neighbors_list.find(address));
                        if(it == informed_neighbors_list.end())
                        {
                            // not in the list of informed neighbors, add it and
                            // keep conn in a list that we can use to actually
                            // send the broadcast message
                            //
                            informed_neighbors_list.insert(address);
                            broadcast_connection.push_back(remote_conn);
                        }
                    }
                }
            });
    }

    if(!broadcast_connection.empty())
    {
        // we are broadcasting now (Gossiping a regular message);
        // for the gossiping to work, we include additional
        // information in the message
        //
        std::string const originator(f_connection_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS));
        auto it(informed_neighbors_list.find(originator));
        if(it != informed_neighbors_list.end())
        {
            // include self since we already know of the message too!
            // (no need for others to send it back to us)
            //
            informed_neighbors_list.insert(originator);
        }

        // message is 'const', so we need to create a copy
        //
        ed::message broadcast_msg(msg);

        // generate a unique broadcast message identifier if we did not
        // yet have one, it is very important to NOT generate a new message
        // in a many to many broadcasting system because you must block
        // duplicates here
        //
        ++g_broadcast_sequence;
        if(broadcast_msgid.empty())
        {
            broadcast_msgid += f_server_name;
            broadcast_msgid += '-';
            broadcast_msgid += g_broadcast_sequence;
        }
        broadcast_msg.add_parameter("broadcast_msgid", broadcast_msgid);

        // increase the number of hops; if we reach the limit, we still
        // want to forward the message, the destination will not forward
        // (broadcast) more, but it will possibly send that to its own
        // services
        //
        broadcast_msg.add_parameter("broadcast_hops", hops + 1);

        // mainly noise at this point, but I include the originator so
        // we can track that back if needed for debug purposes
        //
        broadcast_msg.add_parameter("broadcast_originator", originator);

        // define a timeout if this is the originator
        //
        if(timeout == 0)
        {
            // give message 10 seconds to arrive to any and all destinations
            timeout = time(nullptr) + 10;
        }
        broadcast_msg.add_parameter("broadcast_timeout", timeout);

        // note that we currently define the list of neighbors BEFORE
        // sending the message (anyway the send_message() just adds the
        // message to a memory cache at this point, so whether it will
        // be sent is not known until later.)
        //
        broadcast_msg.add_parameter("broadcast_informed_neighbors", snapdev::join_strings(informed_neighbors_list, ","));

        for(auto const & bc : broadcast_connection)
        {
            service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(bc));
            if(conn != nullptr)
            {
                conn->send_message(broadcast_msg);
            }
            else
            {
                remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(bc));
                if(remote_conn != nullptr) // this should always be true, but to be double sure...
                {
                    remote_conn->send_message(broadcast_msg);
                }
            }
        }
    }
}


/** \brief Send the current status of a client to connections.
 *
 * Some connections (at this time only the sitter) may be interested
 * by the STATUS event. Any connection that understands the STATUS
 * event will be sent that event whenever the status of a connection
 * changes (specifically, on a REGISTER and on an UNREGISTER or equivalent.)
 *
 * \param[in] client  The client that just had its status changed.
 * \param[in] reply_connection  If not nullptr, the connection where the
 *                              STATUS message gets sent.
 */
void server::send_status(
              ed::connection::pointer_t connection
            , ed::connection::pointer_t * reply_connection)
{
    ed::message reply;
    reply.set_command("STATUS");
    reply.add_parameter("cache", "no");

    // the name of the service is the name of the connection
    //
    reply.add_parameter("service", connection->get_name());

    base_connection::pointer_t base_connection(std::dynamic_pointer_cast<base_connection>(connection));
    if(base_connection != nullptr)
    {
        // include the server name
        //
        std::string const server_name(base_connection->get_server_name());
        if(!server_name.empty())
        {
            reply.add_parameter("server_name", server_name);
        }

        // check whether the connection is now up or down
        //
        connection_type_t const type(base_connection->get_connection_type());
        reply.add_parameter("status", type == connection_type_t::CONNECTION_TYPE_DOWN ? "down" : "up");

        // get the time when it was considered up
        //
        time_t const up_since(base_connection->get_connection_started());
        if(up_since != -1)
        {
            reply.add_parameter("up_since", up_since);
        }

        // get the time when it was considered down (if not up yet, this will be skipped)
        //
        time_t const down_since(base_connection->get_connection_ended());
        if(down_since != -1)
        {
            reply.add_parameter("down_since", down_since);
        }
    }

    if(reply_connection != nullptr)
    {
        service_connection::pointer_t sc(std::dynamic_pointer_cast<service_connection>(*reply_connection));
        if(sc)
        {
            // if the verify_command() fails then it means the caller has to
            // create a handler for the STATUS message
            //
            if(verify_command(sc, reply))
            {
                sc->send_message(reply);
            }
        }
        else
        {
            unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(*reply_connection));
            if(unix_conn != nullptr)
            {
                if(unix_conn->understand_command("STATUS"))
                {
                    // send that STATUS message
                    //
                    //verify_command(sc, reply); -- we reach this line only if the command is understood
                    unix_conn->send_message(reply);
                }
            }
        }
    }
    else
    {
        // we have the message, now we need to find the list of connections
        // interested by the STATUS event
        //
        // TODO: use the broadcast_message() function instead? (with service set to ".")
        //
        ed::connection::vector_t const & all_connections(f_communicator->get_connections());
        for(auto const & conn : all_connections)
        {
            service_connection::pointer_t sc(std::dynamic_pointer_cast<service_connection>(conn));
            if(sc != nullptr)
            {
                if(sc->understand_command("STATUS"))
                {
                    // send that STATUS message
                    //
                    //verify_command(sc, reply); -- we reach this line only if the command is understood
                    sc->send_message(reply);
                }
                continue;
            }

            unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(conn));
            if(unix_conn != nullptr)
            {
                if(unix_conn->understand_command("STATUS"))
                {
                    // send that STATUS message
                    //
                    //verify_command(sc, reply); -- we reach this line only if the command is understood
                    unix_conn->send_message(reply);
                }
                continue;
            }
        }
    }
}


/** \brief Check our current cluster status.
 *
 * We received or lost a connection with a remote computer and
 * need to determine (again) whether we are part of a cluster
 * or not.
 *
 * This function is also called when we receive the CLUSTERSTATUS
 * which is a query to know now what the status of the cluster is.
 * This is generally sent by daemons who need to know and may have
 * missed our previous broadcasts.
 *
 * \param[in] reply_connection  A connection to reply to directly.
 */
void server::cluster_status(ed::connection::pointer_t reply_connection)
{
    // the count_live_connections() counts all the other communicators,
    // not ourself, this is why we have a +1 here (it is very important
    // if you have a single computer like many developers would have when
    // writing code and testing quickly.)
    //
    size_t const count(f_remote_communicators->count_live_connections() + 1);

    // calculate the quorum, minimum number of computers that have to be
    // interconnected to be able to say we have a live cluster
    //
    size_t const total_count(f_all_neighbors.size());
    size_t const quorum(total_count / 2 + 1);
    bool modified = false;

    std::string const new_status(count >= quorum ? "CLUSTERUP" : "CLUSTERDOWN");
    if(new_status != f_cluster_status
    || f_total_count_sent != total_count
    || reply_connection != nullptr)
    {
        if(reply_connection == nullptr)
        {
            f_cluster_status = new_status;
            modified = true;
        }

        // send the results to either the requesting connection or broadcast
        // the status to everyone
        //
        ed::message cluster_status_msg;
        cluster_status_msg.set_command(new_status);
        cluster_status_msg.set_service(".");
        cluster_status_msg.add_parameter("neighbors_count", total_count);
        if(reply_connection != nullptr)
        {
            // reply to a direct CLUSTERSTATUS
            //
            service_connection::pointer_t r(std::dynamic_pointer_cast<service_connection>(reply_connection));
            if(r->understand_command(cluster_status_msg.get_command()))
            {
                r->send_message(cluster_status_msg);
            }
        }
        else
        {
            broadcast_message(cluster_status_msg);
        }
    }

    std::string const new_complete(count == total_count ? "CLUSTERCOMPLETE" : "CLUSTERINCOMPLETE");
    if(new_complete != f_cluster_complete
    || f_total_count_sent != total_count
    || reply_connection != nullptr)
    {
        if(reply_connection == nullptr)
        {
            f_cluster_complete = new_complete;
            modified = true;
        }

        // send the results to either the requesting connection or broadcast
        // the complete to everyone
        //
        ed::message cluster_complete_msg;
        cluster_complete_msg.set_command(new_complete);
        cluster_complete_msg.set_service(".");
        cluster_complete_msg.add_parameter("neighbors_count", total_count);
        if(reply_connection != nullptr)
        {
            // reply to a direct CLUSTERSTATUS
            //
            service_connection::pointer_t r(std::dynamic_pointer_cast<service_connection>(reply_connection));
            if(r->understand_command(cluster_complete_msg.get_command()))
            {
                r->send_message(cluster_complete_msg);
            }
        }
        else
        {
            broadcast_message(cluster_complete_msg);
        }
    }

    if(reply_connection == nullptr)
    {
        f_total_count_sent = total_count;
    }

    if(modified)
    {
        std::ofstream status_file;
        status_file.open(g_status_filename);
        if(status_file.is_open())
        {
            status_file << f_cluster_status << std::endl
                        << f_cluster_complete << std::endl;
        }
    }

    SNAP_LOG_INFO
        << "cluster status is \""
        << new_status
        << "\" and \""
        << new_complete
        << "\" (count: "
        << count
        << ", total count: "
        << total_count
        << ", quorum: "
        << quorum
        << ")"
        << SNAP_LOG_SEND;
}


/** \brief Request LOADAVG messages from a communicatord.
 *
 * This function gets called whenever a local service sends us a
 * request to listen to the LOADAVG messages of a specific
 * communicatord.
 *
 * \param[in] msg  The LISTENLOADAVG message.
 */
void server::msg_listen_loadavg(ed::message & msg)
{
    std::string const ips(msg.get_parameter("ips"));

    std::vector<std::string> ip_list;
    snapdev::tokenize_string(ip_list, ips, { "," });

    // we have to save those as IP addresses since the remote
    // communicators come and go and we have to make sure
    // that all get our REGISERFORLOADAVG message when they
    // come back after a broken link
    //
    for(auto const & ip : ip_list)
    {
        auto it(f_registered_neighbors_for_loadavg.find(ip));
        if(it == f_registered_neighbors_for_loadavg.end())
        {
            // add this one, it was not there yet
            //
            f_registered_neighbors_for_loadavg.insert(ip);

            register_for_loadavg(ip);
        }
    }
}


void server::register_for_loadavg(std::string const & ip)
{
    addr::addr address(addr::string_to_addr(
              ip
            , "127.0.0.1"
            , communicatord::LOCAL_PORT  // the port is ignore, use a safe default
            , "tcp"));
    ed::connection::vector_t const & all_connections(f_communicator->get_connections());
    auto const & it(std::find_if(
            all_connections.begin(),
            all_connections.end(),
            [address](auto const & connection)
            {
                remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(connection));
                if(remote_conn != nullptr)
                {
                    return remote_conn->get_connection_address() == address;
                }
                else
                {
                    service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(connection));
                    if(service_conn != nullptr)
                    {
                        return service_conn->get_connection_address() == address;
                    }
                }

                return false;
            }));

    if(it != all_connections.end())
    {
        // there is such a connection, send it a request for
        // LOADAVG message
        //
        ed::message register_message;
        register_message.set_command("REGISTERFORLOADAVG");

        remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(*it));
        if(remote_conn != nullptr)
        {
            remote_conn->send_message(register_message);
        }
        else
        {
            service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(*it));
            if(service_conn != nullptr)
            {
                service_conn->send_message(register_message);
            }
        }
    }
}


void server::msg_save_loadavg(ed::message & msg)
{
    std::string const avg_str(msg.get_parameter("avg"));
    std::string const my_address(msg.get_parameter("my_address"));
    std::string const timestamp_str(msg.get_parameter("timestamp"));

    communicatord::loadavg_item item;

    // Note: we do not use the port so whatever number here is fine
    addr::addr a(addr::string_to_addr(
                  my_address
                , "127.0.0.1"
                , communicatord::LOCAL_PORT  // the port is ignore, use a safe default
                , "tcp"));
    a.set_port(communicatord::LOCAL_PORT); // actually force the port so in effect it is ignored
    a.get_ipv6(item.f_address);

    item.f_avg = std::stof(avg_str);
    if(item.f_avg < 0.0)
    {
        return;
    }

    item.f_timestamp = std::stoll(timestamp_str);
    if(item.f_timestamp < SERVERPLUGINS_UNIX_TIMESTAMP(2016, 1, 1, 0, 0, 0))
    {
        return;
    }

    communicatord::loadavg_file file;
    file.load();
    file.add(item);
    file.save();
}


void server::process_load_balancing()
{
    std::ifstream in;
    in.open("/proc/loadavg", std::ios::in | std::ios::binary);
    if(in.is_open())
    {
        std::string avg_str;
        for(;;)
        {
            char c;
            in.read(&c, 1);
            if(in.fail())
            {
                SNAP_LOG_ERROR
                    << "error reading the /proc/loadavg data."
                    << SNAP_LOG_SEND;
                return;
            }
            if(std::isspace(c))
            {
                // we only read the first number (1 min. load avg.)
                break;
            }
            avg_str += c;
        }

        // we really only need the first number, we would not know what
        // to do with the following ones at this time...
        // (although that could help know whether the load average is
        // going up or down, but it's not that easy, really.)
        //
        // we divide by the number of processors because each computer
        // could have a different number of processors and a load
        // average of 1 on a computer with 16 processors really
        // represents 1/16th of the machine capacity.
        //
        float const avg(std::stof(avg_str) / f_number_of_processors);

        // TODO: see whether the current epsilon is good enough
        if(std::fabs(f_last_loadavg - avg) < 0.1f)
        {
            // do not send if it did not change lately
            return;
        }
        f_last_loadavg = avg;

        ed::message load_avg;
        load_avg.set_command("LOADAVG");
        std::stringstream ss;
        ss << avg;
        load_avg.add_parameter("avg", ss.str());
        load_avg.add_parameter("my_address", f_connection_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
        load_avg.add_parameter("timestamp", time(nullptr));

        ed::connection::vector_t const & all_connections(f_communicator->get_connections());
        std::for_each(
                all_connections.begin(),
                all_connections.end(),
                [&load_avg](auto const & connection)
                {
                    base_connection::pointer_t base(std::dynamic_pointer_cast<base_connection>(connection));
                    if(base
                    && base->wants_loadavg())
                    {
                        remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(connection));
                        if(remote_conn != nullptr)
                        {
                            remote_conn->send_message(load_avg);
                        }
                        else
                        {
                            service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(connection));
                            if(service_conn != nullptr)
                            {
                                service_conn->send_message(load_avg);
                            }
                        }
                    }
                });
    }
    else
    {
        SNAP_LOG_ERROR
            << "error opening file \"/proc/loadavg\"."
            << SNAP_LOG_SEND;
    }
}


/** \brief Return the list of services offered on this computer.
 */
std::string server::get_local_services() const
{
    return f_local_services;
}


/** \brief Return the list of services we heard of.
 */
std::string server::get_services_heard_of() const
{
    return f_services_heard_of;
}


/** \brief Add neighbors to this communicator server.
 *
 * Whenever a communicatord connects to another communicatord
 * server, it is given a list of neighbors. These are added using
 * this function. In the end, all servers are expected to have a
 * complete list of all the neighbors.
 *
 * The input is expected to be a list of comma separated <IP:port>
 * addresses.
 *
 * These IPs are used to connect all the communicatord servers
 * together. If an IP is smaller than this server IP address, then
 * the connection is permanent and uses a CONNECT.
 *
 * If the IP address is larger, this server uses the IP to connect
 * and send a GOSSIP message. The server that receives the GOSSIP
 * is then expected to create a permanent connection and send the
 * CONNECT message.
 *
 * The first time add_neighbors() is called it reads the list of
 * neighbors from the communicatord cache
 * (`/var/lib/communicatord/neighbors.txt`) and connects / gossips
 * top all of those communicatord servers.
 *
 * The init() function first adds this server. This information is
 * important because it is used to calculate the size() which is
 * used to determine whether the cluster is up, down, or complete.
 * parameter although we do not want to add 127.0.0.1 as an IP
 *
 * \todo
 * Make this list survive restarts of the communicator server.
 *
 * \param[in] new_neighbors  The list of new neighbors
 */
void server::add_neighbors(std::string const & new_neighbors)
{
    auto const trimmed(snapdev::trim_string(
          new_neighbors
        , true
        , true
        , false
        , std::string(" ,\n")));
    if(trimmed.empty())
    {
        return;
    }

    SNAP_LOG_DEBUG
        << "add neighbors: "
        << trimmed
        << SNAP_LOG_SEND;

    // first time initialize and read the cache file
    //
    read_neighbors();

    addr::addr_parser list;
    list.set_protocol(IPPROTO_TCP);
    list.set_allow(addr::allow_t::ALLOW_REQUIRED_ADDRESS,         true);
    list.set_allow(addr::allow_t::ALLOW_MULTI_ADDRESSES_COMMAS,   true);
    list.set_allow(addr::allow_t::ALLOW_MULTI_ADDRESSES_SPACES,   true);
    list.set_allow(addr::allow_t::ALLOW_MULTI_ADDRESSES_NEWLINES, true);
    list.set_allow(addr::allow_t::ALLOW_REQUIRED_PORT,            true);
    list.set_allow(addr::allow_t::ALLOW_COMMENT_HASH,             true);
    addr::addr_range::vector_t const addresses(list.parse(trimmed));

    bool changed(false);
    for(auto const & a : addresses)
    {
        if(a.has_to()
        || a.is_range()
        || !a.has_from())
        {
            SNAP_LOG_RECOVERABLE_ERROR
                << "neighbor <IP:port> cannot be a range: "
                << a.to_string()
                << SNAP_LOG_SEND;
            continue;
        }

        if(f_all_neighbors.insert(a.get_from()).second)
        {
            changed = true;

            // in case we are already running we want to also add
            // the corresponding connection
            //
            f_remote_communicators->add_remote_communicator(a.get_from());
        }
    }

    // if the map changed, then save the change in the cache
    //
    // TODO: we may be able to optimize this by not saving on each and
    //       every call; although since it should remain relatively
    //       small, we should be fine (yes, 8,000 computers is still
    //       a small file in this cache.)
    //
    if(changed)
    {
        save_neighbors();
    }
}


/** \brief Remove a neighbor from our list of neighbors.
 *
 * This function removes a neighbor from the cache of this machine. If
 * the neighbor is also defined in the configuration file, such as
 * /etc/communicatord/communicatord.conf, then the IP will not be
 * forgotten any time soon.
 *
 * \param[in] neighbor  The neighbor to be removed.
 */
void server::remove_neighbor(std::string const & neighbor)
{
    addr::addr n(addr::string_to_addr(
              neighbor
            , "255.255.255.255"
            , communicatord::REMOTE_PORT // if neighbor does not include a port, we may miss the SECURE_PORT...
            , "tcp"));

    auto it(f_all_neighbors.find(n));

    SNAP_LOG_DEBUG
        << "Forgetting neighbor: "
        << neighbor
        << (it != f_all_neighbors.end() ? " (exists)" : "")
        << SNAP_LOG_SEND;

    // remove the IP from the neighbors.txt file if still present there
    //
    if(it != f_all_neighbors.end())
    {
        f_all_neighbors.erase(it);
        save_neighbors();
    }

    // make sure we stop all gossiping toward that address
    //
    f_remote_communicators->gossip_received(n);

    // also remove the remote connection otherwise it will send that
    // info in broadcast messages and the neighbor resaved in those
    // other platforms neighbors.txt files
    //
    f_remote_communicators->forget_remote_connection(n);
}


/** \brief Read the list of neighbors from disk.
 *
 * The first time we deal with our list of neighbors we need to call this
 * file to make sure we get that list ready as expected, which is with
 * all the IP:port previously saved in the neighbors.txt file.
 */
void server::read_neighbors()
{
    if(!f_neighbors_cache_filename.empty())
    {
        return;
    }

    // get the path to the dynamic communicatord data files
    //
    f_neighbors_cache_filename = f_opts.get_string("data_path");
    f_neighbors_cache_filename += "/neighbors.txt";

    snapdev::file_contents cache(f_neighbors_cache_filename);
    if(!cache.exists())
    {
        return;
    }

    if(cache.read_all())
    {
        std::string const all(cache.contents());

        addr::addr_parser list;
        list.set_protocol(IPPROTO_TCP);
        list.set_allow(addr::allow_t::ALLOW_REQUIRED_ADDRESS,         true);
        list.set_allow(addr::allow_t::ALLOW_MULTI_ADDRESSES_COMMAS,   true);
        list.set_allow(addr::allow_t::ALLOW_MULTI_ADDRESSES_SPACES,   true);
        list.set_allow(addr::allow_t::ALLOW_MULTI_ADDRESSES_NEWLINES, true);
        list.set_allow(addr::allow_t::ALLOW_REQUIRED_PORT,            true);
        list.set_allow(addr::allow_t::ALLOW_COMMENT_HASH,             true);
        addr::addr_range::vector_t const addresses(list.parse(all));

        for(auto const & a : addresses)
        {
            if(a.has_to()
            || a.is_range()
            || !a.has_from())
            {
                SNAP_LOG_RECOVERABLE_ERROR
                    << "neighbor <IP:port> cannot be a range: "
                    << a.to_string()
                    << SNAP_LOG_SEND;
                continue;
            }

            f_all_neighbors.insert(a.get_from());
            f_remote_communicators->add_remote_communicator(a.get_from());
        }
    }
    else
    {
        SNAP_LOG_NOTICE
            << "neighbor file \""
            << f_neighbors_cache_filename
            << "\" could not be read: "
            << cache.last_error()
            << '.'
            << SNAP_LOG_SEND;
    }
}


/** \brief Save the current list of neighbors to disk.
 *
 * Whenever the list of neighbors changes, this function gets called
 * so the changes can get save on disk and reused on a restart.
 */
void server::save_neighbors()
{
    if(f_neighbors_cache_filename.empty())
    {
        throw communicatord::logic_error("Somehow save_neighbors() was called when f_neighbors_cache_filename was not set yet.");
    }

    std::ofstream out;
    out.open(f_neighbors_cache_filename);
    if(!out.is_open())
    {
        SNAP_LOG_ERROR
            << "could not open \""
            << f_neighbors_cache_filename
            << "\" for writing."
            << SNAP_LOG_SEND;
        return;
    }

    out << addr::setaddrmode(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT)
        << addr::setaddrsep("\n")
        << f_all_neighbors
        << std::endl;

    if(!out)
    {
        SNAP_LOG_ERROR
            << "could not properly write all neighbors to \""
            << f_neighbors_cache_filename
            << "\"."
            << SNAP_LOG_SEND;
        return;
    }
}


/** \brief The list of services we know about from other communicators.
 *
 * This function gathers the list of services that this communicatord
 * heard of. This means, the list of all the services offered by other
 * communicators, heard of or not, minus our own services (because
 * these other servers will return our own services as heard of!)
 */
void server::refresh_heard_of()
{
    // reset the list
    f_services_heard_of_list.clear();

    // first gather all the services we have access to
    ed::connection::vector_t const & all_connections(f_communicator->get_connections());
    for(auto const & connection : all_connections)
    {
        service_connection::pointer_t c(std::dynamic_pointer_cast<service_connection>(connection));
        if(c != nullptr)
        {
            // get list of services and heard services
            //
            c->get_services(f_services_heard_of_list);
            c->get_services_heard_of(f_services_heard_of_list);
        }
    }

    // now remove services we are in control of
    //
    for(auto const & service : f_local_services_list)
    {
        auto it(f_services_heard_of_list.find(service));
        if(it != f_services_heard_of_list.end())
        {
            f_services_heard_of_list.erase(it);
        }
    }

    // generate a string we can send in a CONNECT or an ACCEPT
    //
    f_services_heard_of = snapdev::join_strings(f_services_heard_of_list, ",");
}


bool server::send_message(ed::message & msg, bool cache)
{
    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        throw communicatord::logic_error("server::send_message() called with a missing user data connection pointer.");
    }

    return conn->send_message_to_connection(msg, cache);
}


/** \brief This communicatord received the SHUTDOWN or a STOP command.
 *
 * This function processes the SHUTDOWN or STOP commands. It is a bit of
 * work since we have to send a message to all connections and the message
 * vary depending on the type of connection.
 *
 * \param[in] quitting  Do a full shutdown (true) or just a stop (false).
 */
void server::stop(bool quitting)
{
    // from now on, we are shutting down; use this flag to make sure we
    // do not accept any more REGISTER, CONNECT and other similar
    // messages
    //
    f_shutdown = true;

    SNAP_LOG_DEBUG
        << "shutting down communicatord ("
        << (quitting ? "QUIT" : "STOP")
        << ")"
        << SNAP_LOG_SEND;

    // all gossiping can stop at once, since we cannot recognize those
    // connections in the list returned by f_communicator, we better
    // do that cleanly ahead of time
    //
    if(f_remote_communicators != nullptr)
    {
        f_remote_communicators->stop_gossiping();
    }

    // DO NOT USE THE REFERENCE -- we need a copy of the vector
    // because the loop below uses remove_connection() on the
    // original vector!
    //
    ed::connection::vector_t const all_connections(f_communicator->get_connections());
    for(auto const & connection : all_connections)
    {
        // a remote communicator for which we initiated a new connection?
        //
        remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(connection));
        if(remote_conn != nullptr)
        {

// TODO: if the remote communicator IP address is the same as the
//       STOP, DISCONNECT, or SHUTDOWN message we just received,
//       then we have to just disconnect (HUP) instead of sending
//       a "reply"

            // remote communicators are just timers and can be removed
            // as is, no message are sent there (no interface to do so anyway)
            //
            ed::message reply;

            // a remote communicatord needs to also
            // shutdown so duplicate that message there
            //
            if(quitting)
            {
                // SHUTDOWN means we shutdown the entire cluster!!!
                //
                reply.set_command("SHUTDOWN");
            }
            else
            {
                // STOP means we do not shutdown the entire cluster
                // so here we use DISCONNECT instead
                //
                reply.set_command("DISCONNECT");
            }

            // we know this a remote communicatord, no need to verify, and
            // we may not yet have received the ACCEPT message
            //
            //verify_command(remote_communicator, reply);
            remote_conn->send_message(reply);

            // we are quitting so we want the permanent connection
            // to exit ASAP, by marking as done, it will stop as
            // soon as the message is written to the socket
            //
            remote_conn->mark_done(true);
        }
        else
        {
            // a standard service connection or a remote communicatord server?
            //
            service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(connection));
            unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(connection));
            if(conn != nullptr
            || unix_conn != nullptr)
            {
                base_connection::pointer_t base_conn(std::dynamic_pointer_cast<base_connection>(connection));
                connection_type_t const type(base_conn->get_connection_type());
                if(type == connection_type_t::CONNECTION_TYPE_DOWN)
                {
                    // not initialized, just get rid of that one
                    //
                    f_communicator->remove_connection(connection);
                }
                else
                {
                    ed::message reply;
                    if(type == connection_type_t::CONNECTION_TYPE_REMOTE)
                    {

// TODO: if the remote communicator IP address is the same as the
//       STOP, DISCONNECT, or SHUTDOWN message we just received,
//       then we have to just disconnect (HUP) instead of sending
//       a reply

                        // a remote communicatord server needs to also
                        // shutdown so duplicate that message there
                        if(quitting)
                        {
                            // SHUTDOWN means we shutdown the entire cluster!!!
                            reply.set_command("SHUTDOWN");
                        }
                        else
                        {
                            // DISCONNECT means only we are going down
                            reply.set_command("DISCONNECT");
                        }

                        if(verify_command(base_conn, reply))
                        {
                            base_conn->send_message_to_connection(reply);
                        }

                        // we cannot yet remove the connection from the
                        // communicator or the message would never be sent...
                        //
                        // the remote connections are expected to disconnect
                        // us when they receive a DISCONNECT, but really we
                        // disconnect ourselves as soon as we sent the
                        // message, no need to wait any longer
                        //
                        connection->mark_done();
                    }
                    else
                    {
                        // a standard client (i.e. pagelist, images, etc.)
                        // may want to know when it gets disconnected
                        // from the communicatord...
                        //
                        if(base_conn->understand_command("DISCONNECTING"))
                        {
                            // close connection as soon as the message was
                            // sent (i.e. we are "sending the last message")
                            //
                            connection->mark_done();

                            reply.set_command("DISCONNECTING");
                            base_conn->send_message_to_connection(reply);
                        }
                        else if((conn != nullptr && conn->has_output())
                             || (unix_conn != nullptr && unix_conn->has_output()))
                        {
                            // we just sent some data to that connection
                            // so we do not want to kill it immediately
                            //
                            // instead we mark it done so once the write
                            // buffer gets empty, the connection gets
                            // removed (See process_empty_buffer())
                            //
                            connection->mark_done();
                        }
                        else
                        {
                            // that local connection does not understand
                            // DISCONNECTING and has nothing more in its
                            // buffer, so just remove it immediately
                            //
                            // we will not accept new local connections since
                            // we also remove the f_local_listener connection
                            //
                            f_communicator->remove_connection(connection);
                        }
                    }
                }
            }
            // else -- ignore the main TCP and UDP servers which we
            //         handle below
        }
    }

    // remove the two main servers; we will not respond to any more
    // requests anyway
    //
    f_communicator->remove_connection(f_interrupt.lock());  // TCP/IP
    f_communicator->remove_connection(f_local_listener);    // TCP/IP
    f_communicator->remove_connection(f_remote_listener);   // TCP/IP
    f_communicator->remove_connection(f_secure_listener);   // TCP/IP
    f_communicator->remove_connection(f_unix_listener);     // Unix Stream
    f_communicator->remove_connection(f_ping);              // UDP/IP
    f_communicator->remove_connection(f_loadavg_timer);     // load balancer timer

//#ifdef _DEBUG
    {
        ed::connection::vector_t const all_connections_remaining(f_communicator->get_connections());
        if(all_connections_remaining.empty())
        {
            SNAP_LOG_DEBUG
                << "No connections left after the stop() call."
                << SNAP_LOG_SEND;
        }
        else
        {
            for(auto const & connection : all_connections_remaining)
            {
                SNAP_LOG_DEBUG
                    << "Connection still left after the stop() call: \""
                    << connection->get_name()
                    << "\"."
                    << SNAP_LOG_SEND;
            }
        }
    }
//#endif
}


bool server::is_debug() const
{
    return snaplogger::logger::get_instance()->get_lowest_severity() <= snaplogger::severity_t::SEVERITY_DEBUG;
}


void server::process_connected(ed::connection::pointer_t conn)
{
    ed::message connect;
    connect.set_command("CONNECT");
    connect.add_version_parameter();
    connect.add_parameter("my_address", f_connection_address.to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
    connect.add_parameter("server_name", f_server_name);
    if(!f_explicit_neighbors.empty())
    {
        connect.add_parameter("neighbors", f_explicit_neighbors);
    }
    if(!f_local_services.empty())
    {
        connect.add_parameter("services", f_local_services);
    }
    if(!f_services_heard_of.empty())
    {
        connect.add_parameter("heard_of", f_services_heard_of);
    }
    base_connection::pointer_t base(std::dynamic_pointer_cast<base_connection>(conn));
    if(base != nullptr)
    {
        base->send_message_to_connection(connect);
    }

    // status changed for this connection
    //
    send_status(conn);
}



} // namespace communicator_daemon
// vim: ts=4 sw=4 et

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

// self
//
#include    "communicator/communicator_connection.h"

#include    "communicator/exception.h"
#include    "communicator/names.h"


// snaplogger
//
#include    <snaplogger/message.h>


// eventdispatcher
//
#include    <eventdispatcher/local_stream_client_permanent_message_connection.h>
#include    <eventdispatcher/tcp_client_permanent_message_connection.h>
#include    <eventdispatcher/udp_server_message_connection.h>


// libaddr
//
#include    <libaddr/addr_parser.h>
#include    <libaddr/addr_unix.h>


// edhttp
//
#include    <edhttp/uri.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator
{

namespace
{


/** \brief Options to handle the communicator connection.
 *
 * One can connect to the communicator daemon through several possible
 * connetions:
 *
 * \li TCP / plain text / local (on your computer loopback)
 * \li TCP / plain text / remote (on your local network)
 * \li TCP / secure / remote (from anywhere)
 * \li Stream / Unix socket (on your computer via a local stream)
 * \li UDP / plain text (on your local network)
 *
 * The options offered to the clients allow for selection which one
 * of those connections to make from this client. The default is
 * to use the Unix socket since this is generally the safest.
 */
advgetopt::option const g_options[] =
{
    // COMMUNICATOR OPTIONS
    //
    advgetopt::define_option(
          advgetopt::Name("communicator-listen")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE
            , advgetopt::GETOPT_FLAG_CONFIGURATION_FILE
            , advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_SHOW_SYSTEM>())
        , advgetopt::EnvironmentVariableName("COMMUNICATOR_LISTEN")
        , advgetopt::DefaultValue("cd:///run/communicator/communicatord.sock")
        , advgetopt::Help("define the communicator daemon connection type as a scheme (cd://, cdu://, cds://, cdb://) along an \"address:port\" or \"/socket/path\".")
    ),
    advgetopt::define_option(
          advgetopt::Name("permanent-connection-retries")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE
            , advgetopt::GETOPT_FLAG_CONFIGURATION_FILE
            , advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_SHOW_SYSTEM>())
        , advgetopt::EnvironmentVariableName("PERMANENT_CONNECTION_RETRIES")
        , advgetopt::DefaultValue("1,1,1,3,5,10,20,30,60")
        , advgetopt::Help("define a list of pause durations for the permanent connection; each one gets used until a connection happens; the list restart at the beginning after a lost connection.")
    ),

    // END
    //
    advgetopt::end_options()
};



class communicator_interface
{
public:
    virtual             ~communicator_interface() {}

    virtual bool        is_connected() const = 0;
};


class local_stream
    : public ed::local_stream_client_permanent_message_connection
    , public communicator_interface
{
public:
    local_stream(
              addr::addr_unix const & address
            , std::string const & service_name
            , ed::pause_durations const & retries)
        : local_stream_client_permanent_message_connection(
                  address
                , retries
                , true
                , false
                , true
                , service_name)
    {
        set_name("communicator_local_stream");
    }

    virtual void process_connected() override
    {
        local_stream_client_permanent_message_connection::process_connected();
        register_service();
    }

    virtual bool is_connected() const override
    {
        return local_stream_client_permanent_message_connection::is_connected();
    }
};


class tcp_stream
    : public ed::tcp_client_permanent_message_connection
    , public communicator_interface
{
public:
    tcp_stream(
              addr::addr_range::vector_t const & ranges
            , ed::mode_t mode
            , std::string const & service_name
            , ed::pause_durations const & retries)
        : tcp_client_permanent_message_connection(
              ranges
            , mode
            , retries
            , true
            , service_name)
    {
        set_name("communicator_tcp_stream");
    }

    virtual void process_connected() override
    {
        tcp_client_permanent_message_connection::process_connected();
        register_service();
    }

    virtual bool is_connected() const override
    {
        return tcp_client_permanent_message_connection::is_connected();
    }
};


class udp_dgram
    : public ed::udp_server_message_connection
    , public communicator_interface
{
public:
    typedef std::shared_ptr<udp_dgram>  pointer_t;

    udp_dgram(
              addr::addr const & server
            , addr::addr const & client
            , std::string const & service_name)
        : udp_server_message_connection(server, client, service_name)
    {
        set_name("communicator_udp_dgram");
    }

    void simulate_connected()
    {
        register_service();
    }

    virtual bool is_connected() const override
    {
        // UDP is not really ever "connected"
        return true;
    }
};



} // no name namespace


/** \brief Initialize the communicator extension.
 *
 * The constructor initializes the communicator extension by saving a
 * reference to the command line option object. This is used in a few
 * other functions as required to determine the value of options used
 * by this object.
 *
 * See the g_options array for a list of the options added by this
 * constructor and made available on your command line.
 *
 * The communicator extension allows for your service to connect to
 * the communicator daemon without effort on your part (well... nearly.
 * Hopefully eventdispatcher version 2.x will do a lot better on that end.)
 * This is done by first adding the communicator available options
 * (i.e. this very function) then by processing the options by calling the
 * process_communicator_options(). You are responsible for that second call
 * (see an example in the fluid-settings server.cpp & messenger.cpp files).
 *
 * \note
 * This function calls set_name() with the default name "communicator_client".
 * Although it is expected that your final connection calls that function to
 * properly name the connection.
 *
 * \warning
 * At this point, the \p opts parameter is likely a reference to a
 * non-initialized options object (a.k.a. empty, no argc/argv passed
 * through, no base environment). This means all sorts of features are
 * not yet available.
 *
 * \param[in] opts  An reference to an advgetopt::getopt object.
 * \param[in] service_name  Name of the service (daemon) using this communicator
 * client.
 */
communicator_connection::communicator_connection(
          advgetopt::getopt & opts
        , std::string const & service_name)
    : timer(-1)
    , f_opts(opts)
    , f_communicator(ed::communicator::instance())
    , f_service_name(service_name)
    , f_dispatcher(std::make_shared<ed::dispatcher>(this))
{
    if(f_service_name.empty())
    {
        throw invalid_name("the service_name parameter of the communicator_connection constructor cannot be an empty string.");
    }

    // WARNING: keep in mind that the dispatcher_support class only has
    //          a weak pointer to the dispatcher so that multiple types
    //          of connections can have access to the same dispatcher
    //          but not hold on to it when the main connection disappears
    //
    set_dispatcher(f_dispatcher);

    // further dispatcher initialization
    //
#ifdef _DEBUG
    f_dispatcher->set_trace();
    f_dispatcher->set_show_matches();
#endif

    f_dispatcher->add_matches({
        ed::define_match(
              ed::Expression(::communicator::g_name_communicator_cmd_status)
            , ed::Callback(std::bind(&communicator_connection::msg_status, this, std::placeholders::_1))
            , ed::Priority(ed::dispatcher_match::DISPATCHER_MATCH_SYSTEM_PRIORITY)
        ),
    });
    f_dispatcher->add_communicator_commands();

    set_name("communicator_client");
    f_opts.parse_options_info(g_options, true);
    ed::add_message_definition_options(f_opts);
}


/** \brief Destroy the communicator_connection.
 *
 * The communicator_connection needed a virtual destructor so here it is.
 */
communicator_connection::~communicator_connection()
{
}


/** \brief Get the command options.
 *
 * The communicator_connection saves a reference to the advgetopt options. This
 * function gives you access to that reference.
 *
 * \return A reference to the command line options managed by the
 *         communicator_connection.
 */
advgetopt::getopt & communicator_connection::get_options()
{
    return f_opts;
}


/** \brief Retrieve the service name from the communicator_connection.
 *
 * This function returns a reference to the service name as defined when
 * the communicator object was created. This parameter cannot be an empty
 * string.
 *
 * \return the service name as specified when you constructed this
 * communicator_connection.
 */
std::string const & communicator_connection::service_name() const
{
    return f_service_name;
}


/** \brief Call this function after you finalized your own option processing.
 *
 * This function acts on your client's various command line options.
 * Assuming the command line options are all valid, this function opens
 * a connection to the specified communicator daemon.
 *
 * Once you are ready to quit your process, make sure to call the
 * disconnect_communicator_messenger() function to send a last message
 * to the communicator daemon and remove this connection from list of
 * connections managed by ed::communicator. Not doing so would block
 * your service since it would continue to listen for messages on this
 * connection forever.
 */
void communicator_connection::process_communicator_options()
{
    if(f_communicator_connection != nullptr)
    {
        throw logic_error("process_communicator_options() called twice.");
    }

    ed::process_message_definition_options(f_opts);

    std::string const retries(f_opts.get_string("permanent-connection-retries"));

    // extract the scheme and segments
    //
    edhttp::uri u;
    u.set_uri(f_opts.get_string("communicator_listen"), true, true);

    std::string const scheme(u.scheme());

    // unix is a special case since the URI is a path to a file
    // so we have to handle it in a special way
    //
    if(u.is_unix())
    {
        if(scheme != "cd")
        {
            connection_unavailable const e("a Unix socket connection only works with the \"cd:\" scheme.");
            SNAP_LOG_FATAL
                << e
                << SNAP_LOG_SEND;
            throw e;
        }
        addr::addr_unix address('/' + u.path(false));
        address.set_scheme(scheme);
        f_communicator_connection = std::make_shared<local_stream>(address, f_service_name, retries);
    }
    else
    {
        addr::addr_range::vector_t const & ranges(u.address_ranges());
        if(ranges.size() <= 0)
        {
            // I don't think this is possible
            //
            connection_unavailable const e("the communicator_connection requires at least one address to work.");
            SNAP_LOG_FATAL
                << e
                << SNAP_LOG_SEND;
            throw e;
        }

        if(scheme == g_name_communicator_scheme_cd)
        {
            for(auto const & r : ranges)
            {
                if((r.has_from() && !r.get_from().is_lan())
                || (r.has_to()   && !r.get_to().is_lan()))
                {
                    std::stringstream ss;
                    ss << "the \"cd:\" scheme requires a LAN address. For public addresses, please use \"cds:\" instead. "
                       << (r.has_from() ? r.get_from() : r.get_to())
                       << " will not work.";
                    security_issue const e(ss.str());
                    SNAP_LOG_FATAL
                        << e
                        << SNAP_LOG_SEND;
                    throw e;
                }
            }
            f_communicator_connection = std::make_shared<tcp_stream>(
                      ranges
                    , ed::mode_t::MODE_PLAIN
                    , f_service_name
                    , retries);
        }
        else if(scheme == g_name_communicator_scheme_cds)
        {
            for(auto const & r : ranges)
            {
                if((r.has_from() && r.get_from().get_network_type() == addr::network_type_t::NETWORK_TYPE_LOOPBACK)
                || (r.has_to()   && r.get_to().get_network_type() == addr::network_type_t::NETWORK_TYPE_LOOPBACK))
                {
                    SNAP_LOG_IMPORTANT
                        << "the \"cds:\" scheme is not likely to work on the loopback network ("
                        << (r.has_from() ? r.get_from() : r.get_to())
                        << ")."
                        << SNAP_LOG_SEND;
                }
            }
            f_communicator_connection = std::make_shared<tcp_stream>(
                      ranges
                    , ed::mode_t::MODE_ALWAYS_SECURE
                    , f_service_name
                    , retries);
        }
        else if(scheme == g_name_communicator_scheme_cdu)
        {
            // I don't think that the URI object can return a "to" only range
            //
            if(ranges.size() != 1
            || ranges[0].size() != 1
            || !ranges[0].has_from())
            {
                connection_unavailable const e("the \"cdu:\" requires exactly one address to work.");
                SNAP_LOG_FATAL
                    << e
                    << SNAP_LOG_SEND;
                throw e;
            }

            // we listen on the "Server" IP:port (port will be auto-assigned)
            //
            addr::addr server(ranges[0].get_from());
            server.set_port(0);

            if(!server.is_lan())
            {
                security_issue const e(
                      "the \"cdu:\" scheme requires a LAN address. For public addresses, please use \"cds:\" instead. "
                    + server.to_ipv4or6_string()
                    + " will not work.");
                SNAP_LOG_FATAL
                    << e
                    << SNAP_LOG_SEND;
                throw e;
            }

            // we send messages on the "Client" IP:port
            //
            addr::addr const client(ranges[0].get_from());

            udp_dgram::pointer_t conn = std::make_shared<udp_dgram>(
                      server
                    , client
                    , f_service_name);
            conn->simulate_connected();
            f_communicator_connection = conn;
        }
        else if(scheme == g_name_communicator_scheme_cdb)
        {
            // TBD: I don't think I'll implement that one since broadcasting
            //      happens in the communicator daemon itself.
            //
            connection_unavailable const e("the \"cdb:\" scheme is not yet supported.");
            SNAP_LOG_FATAL
                << e
                << SNAP_LOG_SEND;
            throw e;
        }
        else
        {
            connection_unavailable const e(
                      "unknown scheme \""
                    + scheme
                    + ":\" to connect to communicatord.");
            SNAP_LOG_FATAL
                << e
                << SNAP_LOG_SEND;
            throw e;
        }
    }

    if(f_communicator_connection == nullptr)
    {
        connection_unavailable const e("could not create a connection to the communicatord.");
        SNAP_LOG_FATAL
            << e
            << SNAP_LOG_SEND;
        throw e;
    }

    ed::dispatcher_support::pointer_t d(std::dynamic_pointer_cast<ed::dispatcher_support>(f_communicator_connection));
    if(d == nullptr)
    {
        connection_unavailable const e("the connection does not support the ed::dispatcher.");
        SNAP_LOG_FATAL
            << e
            << SNAP_LOG_SEND;
        throw e;
    }
    d->set_dispatcher(get_dispatcher());

    if(!f_communicator->add_connection(f_communicator_connection))
    {
        f_communicator_connection.reset();

        connection_unavailable const e("could not register the communicatord connection.");
        SNAP_LOG_FATAL
            << e
            << SNAP_LOG_SEND;
        throw e;
    }
}


/** \brief Check whether the connection to the communicator is up.
 *
 * This function returns true if there is a connection to the communicator
 * daemon and it is up.
 *
 * \return true if the connection is available.
 */
bool communicator_connection::is_connected() const
{
    return f_communicator_connection != nullptr
        ? std::dynamic_pointer_cast<communicator_interface>(f_communicator_connection)->is_connected()
        : false;
}


bool communicator_connection::send_message(ed::message & msg, bool cache)
{
    ed::connection_with_send_message::pointer_t messenger(std::dynamic_pointer_cast<ed::connection_with_send_message>(f_communicator_connection));
    if(messenger != nullptr)
    {
        if(msg.get_sent_from_service().empty())
        {
            msg.set_sent_from_service(f_service_name);
        }
        return messenger->send_message(msg, cache);
    }

    return false;
}


/** \brief The communicator STATUS message.
 *
 * Whenever the communicator obtains or loses a connection with a
 * service, it sends a STATUS message with the name and new status
 * of that message.
 *
 * The communicator object manages the message and calls this
 * callback function. The default function does nothing at the
 * moment, but it should be called, just in case it does later.
 *
 * \note
 * The UP status uses the following name:
 * \code
 *     communicator::g_name_communicator_value_up
 * \endcode
 * And the DOWN status uses the following name:
 * \code
 *     communicator::g_name_communicator_value_down
 * \endcode
 *
 * \param[in] service  The name of the service of which the status changed.
 * \param[in] status  The new status (usually UP or DOWN).
 */
void communicator_connection::service_status(std::string const & service, std::string const & status)
{
    snapdev::NOT_USED(service, status);
}


void communicator_connection::msg_status(ed::message & msg)
{
    if(!msg.has_parameter(::communicator::g_name_communicator_param_service)
    || !msg.has_parameter(::communicator::g_name_communicator_param_status))
    {
        return;
    }
    service_status(
          msg.get_parameter(::communicator::g_name_communicator_param_service)
        , msg.get_parameter(::communicator::g_name_communicator_param_status));
}


/** \brief When exiting your process, make sure to unregister.
 *
 * To cleanly unregister a service and thus send a message to the communicator
 * service letting it know that you are exiting, call this function.
 *
 * The process takes the \p quitting parameter to know whether the communicator
 * itself is quitting (true) or not (false).
 *
 * \param[in] quitting  true if the communicator daemon itself is quitting.
 */
void communicator_connection::unregister_communicator(bool quitting)
{
    if(f_communicator_connection != nullptr)
    {
        // check whether we are connected, which depends on the type of
        // connection we are currently using
        //
        ed::connection_with_send_message::pointer_t messenger(std::dynamic_pointer_cast<ed::connection_with_send_message>(f_communicator_connection));
        if(quitting || messenger == nullptr)
        {
            f_communicator->remove_connection(f_communicator_connection);
            f_communicator_connection.reset();
        }
        else
        {
            // in this case we can UNREGISTER from the communicator daemon
            //
            messenger->unregister_service();
        }
    }
}


/** \brief Request the communicator daemon to return a transmission report.
 *
 * By calling this function, you mark the message so that if an error
 * occurs and the message cannot be forward to the actual destination
 * service, then a `TRANSMISSION_REPORT` is sent back to your service.
 *
 * The function adds the "transmission_report" parameter to the \p msg
 * with the value "failure".
 *
 * \param[in] msg  The message to which to add the `transmission_report`
 * parameter.
 */
void request_failure(ed::message & msg)
{
    msg.add_parameter(
          ::communicator::g_name_communicator_param_transmission_report
        , ::communicator::g_name_communicator_value_failure);
}



} // namespace communicator
// vim: ts=4 sw=4 et

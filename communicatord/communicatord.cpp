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

// self
//
#include    "communicatord.h"

#include    "exception.h"


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
#include    <libaddr/unix.h>


// edhttp
//
#include    <edhttp/uri.h>



namespace communicatord
{

namespace
{


/** \brief Options to handle the communicatord connection.
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
          advgetopt::Name("communicatord-listen")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE
            , advgetopt::GETOPT_FLAG_CONFIGURATION_FILE
            , advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::EnvironmentVariableName("COMMUNICATORD_LISTEN")
        , advgetopt::DefaultValue("cd:///run/communicatord/communicatord.sock")
        , advgetopt::Help("define the communicator daemon connection type as a scheme (cd://, cdu://, cds://, cdb://) along an \"address:port\" or \"/socket/path\".")
    ),

    // END
    //
    advgetopt::end_options()
};



class local_stream
    : public ed::local_stream_client_permanent_message_connection
{
public:
    local_stream(
              addr::unix const & address
            , std::string const & service_name)
        : local_stream_client_permanent_message_connection(
                  address
                , ed::DEFAULT_PAUSE_BEFORE_RECONNECTING
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
};


class tcp_stream
    : public ed::tcp_client_permanent_message_connection
{
public:
    tcp_stream(
              addr::addr_range::vector_t const & ranges
            , ed::mode_t mode
            , std::string const service_name)
        : tcp_client_permanent_message_connection(
              ranges
            , mode
            , ed::DEFAULT_PAUSE_BEFORE_RECONNECTING
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
};


class udp_dgram
    : public ed::udp_server_message_connection
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
 * \param[in] opts  An reference to an advgetopt::getopt object.
 */
communicator::communicator(
          advgetopt::getopt & opts
        , std::string const & service_name)
    : timer(-1)
    , f_opts(opts)
    , f_communicator(ed::communicator::instance())
    , f_service_name(service_name)
{
    set_name("communicator_timer");

    if(f_service_name.empty())
    {
        throw invalid_name("the service_name parameter of the communicator constructor cannot be an empty string.");
    }

    f_opts.parse_options_info(g_options, true);
}


/** \brief Destroy the communicator.
 *
 * The communicator needed a virtual destructor so here it is.
 */
communicator::~communicator()
{
}


/** \brief Call this function after you finalized your own option processing.
 *
 * This function acts on the communicator various command line options.
 * Assuming the command line options were valid, this function opens a
 * connection to the specified communicator daemon.
 *
 * Once you are ready to quit your process, make sure to call the
 * disconnect_communicator_messenger() function to send a last message
 * to the communicator daemon and remove this connection from list of
 * connections managed by ed::communicator. Not doing so would block
 * your service since it would continue to listen for messages on this
 * connection forever.
 */
void communicator::process_communicatord_options()
{
    if(f_communicator_connection != nullptr)
    {
        throw logic_error("process_communicatord_options() called twice.");
    }

    // extract the scheme and segments
    //
    edhttp::uri u;
    u.set_uri(f_opts.get_string("communicatord-listen"), true, true);

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
        addr::unix address('/' + u.path(false));
        address.set_scheme(scheme);
        f_communicator_connection = std::make_shared<local_stream>(address, f_service_name);
    }
    else
    {
        addr::addr_range::vector_t const & ranges(u.address_ranges());
        if(ranges.size() <= 0)
        {
            // I don't think this is possible
            //
            connection_unavailable const e("the communicatord requires at least one address to work.");
            SNAP_LOG_FATAL
                << e
                << SNAP_LOG_SEND;
            throw e;
        }

        if(scheme == "cd")
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
                    , f_service_name);
        }
        else if(scheme == "cds")
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
                    , f_service_name);
        }
        else if(scheme == "cdu")
        {
            // I don't think that the URI object can return a "to" only range
            //
            if(ranges.size() != 1
            || ranges[0].size() != 1
            || !ranges[0].has_from())
            {
                connection_unavailable const e("the \"cdu:\" requires at least one address to work.");
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
        else if(scheme == "cdb")
        {
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


/** \brief Retrieve the service name from the communicator.
 *
 * This function returns a reference to the service name as defined when
 * the communocator object was created. This paramter cannot be an empty
 * string.
 *
 * \return the service name as specified when you constructed this
 * communicator.
 */
std::string const & communicator::service_name() const
{
    return f_service_name;
}


bool communicator::send_message(ed::message & msg, bool cache)
{
    ed::connection_with_send_message::pointer_t messenger(std::dynamic_pointer_cast<ed::connection_with_send_message>(f_communicator_connection));
    if(messenger != nullptr)
    {
        return messenger->send_message(msg, cache);
    }

    return false;
}


/** \brief When exiting your process, make sure to unregister.
 *
 * To cleanly unregister a service and thus send a message to the communicator
 * service letting it know that you are exiting, call this function.
 *
 * The process takes the \p quitting parameter to know whether the communicator
 * itself is quitting (true) or not (false).
 *
 * \param[in] quitting  true if the communicator itself is quitting.
 */
void communicator::unregister_communicator(bool quitting)
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



} // namespace communicatord
// vim: ts=4 sw=4 et

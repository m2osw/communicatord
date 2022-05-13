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
#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/local_stream_client_permanent_message_connection.h>
#include    <eventdispatcher/tcp_client_permanent_message_connection.h>


// libaddr
//
#include    <libaddr/addr_parser.h>
#include    <libaddr/unix.h>


// edhttp
//
#include    <edhttp/uri.h>



namespace sc
{

namespace
{


/** \brief Options to handle the communicator connection.
 *
 * One can connect to the communicator through several possible
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
            , advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::EnvironmentVariableName("COMMUNICATORD_LISTEN")
        , advgetopt::DefaultValue("unix:///run/communicatord/communicatord.socket")
        , advgetopt::Help("define the connection type as a protocol (tcp, ssl, unix, udp) along an <address:port>.")
    ),
    advgetopt::define_option(
          advgetopt::Name("communicator-secret")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_GROUP_OPTIONS
            , advgetopt::GETOPT_FLAG_COMMAND_LINE
            , advgetopt::GETOPT_FLAG_ENVIRONMENT_VARIABLE
            , advgetopt::GETOPT_FLAG_CONFIGURATION_FILE
            , advgetopt::GETOPT_FLAG_REQUIRED>())
        , advgetopt::EnvironmentVariableName("COMMUNICATORD_SECRET")
        , advgetopt::Help("the <login:password> to connect from a remote computer (i.e. a computer with an IP address other than this one or a local network IP).")
    ),

    // END
    //
    advgetopt::end_options()
};

} // no name namespace


communicator::communicator(advgetopt::getopt & opts)
    : f_opts(opts)
{
}


void communicator::add_communicator_options()
{
    // add options
    //
    f_opts.parse_options_info(g_options, true);
}


/** \brief Call this function after you finalized option processing.
 *
 * This function acts on the communicator various command line options.
 * Assuming the command line options were valid, this function will
 * open a connection to the specified communicator.
 *
 * Once you are ready to quit your process, make sure to call the
 * disconnect_communicator_messenger() function to remove this
 * connection from ed::communicator. Not doing so would block the
 * communicator since it would continue to listen for messages on this
 * channel.
 */
void communicator::process_communicator_options()
{
    // extract the protocol and segments
    //
    edhttp::uri u(f_opts.get_string("communicator-listen"));

    // unix is a special case since the URI is a path to a file
    // so we have to handle it in a special way
    //
    if(u.protocol() == "unix")
    {
        addr::unix address(u.path(false));
        f_communicator_connection = std::make_shared<ed::local_stream_client_permanent_message_connection>(address);
    }
    else
    {
        addr::addr const address(addr::string_to_addr(
              u.full_domain() + ':' + std::to_string(u.get_port())
            , "127.0.0.1"
            , 4040
            , "tcp"));
        f_communicator_connection = std::make_shared<ed::tcp_client_permanent_message_connection>(address);
    }

    if(f_communicator_connection == nullptr)
    {
        SNAP_LOG_FATAL
            << "could not create a connection to the communicatord."
            << SNAP_LOG_SEND;
        throw connection_unavailable("could not create a connection to the communicatord.");
    }

    if(!ed::communicator::instance()->add_connection(f_communicator_connection))
    {
        f_communicator_connection.reset();

        SNAP_LOG_FATAL
            << "could not register the communicatord connection."
            << SNAP_LOG_SEND;
        throw connection_unavailable("could not register the communicatord connection.");
    }
}



} // namespace sc
// vim: ts=4 sw=4 et

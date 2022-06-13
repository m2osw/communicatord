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



namespace communicatord
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
        , advgetopt::EnvironmentVariableName("COMMUNICATOR_LISTEN")
        , advgetopt::DefaultValue("cd:///run/communicatord/communicatord.socket")
        , advgetopt::Help("define the connection type as a protocol (cd, cdu, cds, cdb) along an <address:port> or </socket/path>.")
    ),

    // END
    //
    advgetopt::end_options()
};

} // no name namespace


/** \brief Initialize the communicator extension.
 *
 * The constructor initializes the communicator extension by saving a
 * reference to the command line option object. This is used in a few
 * other functions as required to determine the value of options used
 * by this object.
 *
 * See the g_options array for a list of the supported options.
 *
 * \param[in] opts  An reference to an advgetopt::getopt object.
 */
communicator::communicator(advgetopt::getopt & opts)
    : f_opts(opts)
{
}


/** \brief Add the command line option of communicator extension.
 *
 * The communicator extension allows for your service to connect to
 * the communicator daemon without effort on your part. This is done
 * by first adding the communicator available options (i.e. this very
 * function) then by processing the options by calling the
 * process_communicator_options().
 */
void communicator::add_communicator_options()
{
    // add options
    //
    f_opts.parse_options_info(g_options, true);
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
void communicator::process_communicator_options()
{
    // extract the protocol and segments
    //
    edhttp::uri u;
    u.set_uri(f_opts.get_string("communicator-listen"), true, true);

    // unix is a special case since the URI is a path to a file
    // so we have to handle it in a special way
    //
    if(u.is_unix())
    {
        addr::unix const address(u.path(false));
        f_communicator_connection = std::make_shared<ed::local_stream_client_permanent_message_connection>(address);
    }
    else
    {
        addr::addr const address(addr::string_to_addr(
              u.full_domain() + ':' + u.get_str_port()
            , "127.0.0.1"
            , LOCAL_PORT
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



} // namespace communicatord
// vim: ts=4 sw=4 et

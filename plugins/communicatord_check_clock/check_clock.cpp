// Copyright (c) 2011-2026  Made to Order Software Corp.  All Rights Reserved.
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
#include    "check_clock.h"

//#include    "stable_clock.h"

#include    "communicator/daemon/base_connection.h"
#include    "communicator/daemon/communicatord.h"
#include    "communicator/daemon/service_connection.h"
#include    "communicator/daemon/unix_connection.h"


// communicator
//
#include    <communicator/exception.h>
#include    <communicator/names.h>


// advgetopt
//
//#include    <advgetopt/conf_file.h>
//#include    <advgetopt/validator_integer.h>


// snaplogger
//
#include    <snaplogger/message.h>


// serverplugins
//
#include    <serverplugins/collection.h>
//#include    <serverplugins/factory.h>
//#include    <serverplugins/plugin.h>


// snapdev
//
//#include    <snapdev/file_contents.h>
//#include    <snapdev/not_used.h>
//#include    <snapdev/not_reached.h>
//#include    <snapdev/trim_string.h>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{
namespace check_clock
{



SERVERPLUGINS_START(check_clock)
    , ::serverplugins::description(
            "Verify that the clock has been setup with some kind of NTP system.")
    , ::serverplugins::dependency("communicatord")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("time")
    , ::serverplugins::categorization_tag("synchronization")
SERVERPLUGINS_END(check_clock)




/** \brief Initialize check_clock.
 *
 * This function terminates the initialization of the check_clock plugin
 * by registering for different events.
 */
void check_clock::bootstrap()
{
    SERVERPLUGINS_LISTEN(check_clock, communicatord, initialize, boost::placeholders::_1);
    SERVERPLUGINS_LISTEN0(check_clock, communicatord, terminate);
}


/** \brief Finish initialization by adding more functionality.
 *
 * This function initializes the check_clock functionality of the communicator
 * daemon.
 *
 * This allows other services and communicators to receive the check_clock
 * information from the computer running this communicator daemon.
 *
 * \param[in] opts  A set of command line options.
 */
void check_clock::on_initialize(advgetopt::getopt & opts)
{
    SNAP_LOG_DEBUG
        << "check_clock::on_initialization(): processing..."
        << SNAP_LOG_SEND;

    f_communicator = ed::communicator::instance();

    if(f_tag != ed::dispatcher_match::DISPATCHER_MATCH_NO_TAG)
    {
        throw communicator::logic_error("plugin already initialized.");
    }
    f_tag = ed::dispatcher_match::get_next_tag();

    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    ed::dispatcher::pointer_t dispatcher(s->get_dispatcher());
    dispatcher->add_matches({
        ::ed::define_match(
              ::ed::Expression(communicator::g_name_communicator_cmd_clock_status)
            , ::ed::Callback(std::bind(&check_clock::msg_clock_status, this, std::placeholders::_1))
            , ::ed::Tag(f_tag)
        ),
    });

    f_check_clock_status = std::make_shared<stable_clock>(this);
    if(opts.is_defined("timedate_wait_command"))
    {
        f_check_clock_status->set_timedate_wait_command(opts.get_string("timedate_wait_command"));
    }
    if(!f_communicator->add_connection(f_check_clock_status))
    {
        f_check_clock_status.reset();

        SNAP_LOG_ERROR
            << "could not add the check_clock_status connection to ed::communicator"
            << SNAP_LOG_SEND;
    }
}


void check_clock::on_terminate()
{
    f_communicator->remove_connection(f_check_clock_status);
    f_check_clock_status.reset();

    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    s->get_dispatcher()->remove_matches(f_tag);
}


void check_clock::msg_clock_status(ed::message & msg)
{
SNAP_LOG_WARNING << "--- msg_clock_status() called..." << SNAP_LOG_SEND;
    base_connection::pointer_t conn(msg.user_data<base_connection>());
    if(conn == nullptr)
    {
        return;
    }

SNAP_LOG_WARNING << "--- msg_clock_status() with valid user connection..." << SNAP_LOG_SEND;
    send_clock_status(std::dynamic_pointer_cast<ed::connection>(conn));
}


void check_clock::set_clock_status(clock_status_t status)
{
    if(f_clock_status == status)
    {
        // status did not change, do nothing
        //
        return;
    }
    f_clock_status = status;
    send_clock_status(ed::connection::pointer_t());
}


void check_clock::send_clock_status(ed::connection::pointer_t reply_connection)
{
    ed::message clock_status_msg;

SNAP_LOG_WARNING << "--- send_clock_status() prepare clock status message..." << static_cast<int>(f_clock_status) << SNAP_LOG_SEND;
    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    clock_status_msg.set_sent_from_server(s->get_server_name());
    clock_status_msg.set_sent_from_service(communicator::g_name_communicator_service_communicatord);
    clock_status_msg.set_command(communicator::g_name_communicator_cmd_clock_unstable);
    clock_status_msg.add_parameter(
              communicator::g_name_communicator_param_cache
            , communicator::g_name_communicator_value_no);
    switch(f_clock_status)
    {
    case clock_status_t::CLOCK_STATUS_STABLE:
        clock_status_msg.set_command(communicator::g_name_communicator_cmd_clock_stable);
        clock_status_msg.add_parameter(
                  communicator::g_name_communicator_param_clock_resolution
                , communicator::g_name_communicator_value_verified);
        break;

    case clock_status_t::CLOCK_STATUS_NO_NTP:
        clock_status_msg.set_command(communicator::g_name_communicator_cmd_clock_stable);
        clock_status_msg.add_parameter(
                  communicator::g_name_communicator_param_clock_resolution
                , communicator::g_name_communicator_value_no_ntp);
        break;

    case clock_status_t::CLOCK_STATUS_INVALID:
        clock_status_msg.add_parameter(
                  communicator::g_name_communicator_param_clock_error
                , communicator::g_name_communicator_value_invalid);
        break;

    default:
        clock_status_msg.add_parameter(
                  communicator::g_name_communicator_param_clock_error
                , communicator::g_name_communicator_value_checking);
        break;

    }

SNAP_LOG_WARNING << "--- send_clock_status() check connection :" << std::boolalpha << (reply_connection != nullptr) << SNAP_LOG_SEND;
    if(reply_connection != nullptr)
    {
        // reply to a direct CLOCK_STATUS
        //
        unix_connection::pointer_t unix_conn(std::dynamic_pointer_cast<unix_connection>(reply_connection));
        if(unix_conn != nullptr)
        {
            if(unix_conn->understand_command(clock_status_msg.get_command()))
            {
                unix_conn->send_message(clock_status_msg);
            }
        }
        else
        {
            service_connection::pointer_t conn(std::dynamic_pointer_cast<service_connection>(reply_connection));
            if(conn != nullptr
            && conn->understand_command(clock_status_msg.get_command()))
            {
                conn->send_message(clock_status_msg);
            }
        }
    }
    else
    {
        clock_status_msg.set_service(communicator::g_name_communicator_service_local_broadcast);
        s->broadcast_message(clock_status_msg);
    }
}



} // namespace check_clock
} // namespace communicator_daemon
// vim: ts=4 sw=4 et

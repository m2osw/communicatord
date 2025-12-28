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
#include    "loadavg.h"

#include    "load_timer.h"

#include    "communicator/daemon/remote_connection.h"
#include    "communicator/daemon/service_connection.h"
#include    "communicator/daemon/unix_connection.h"


// communicator
//
#include    <communicator/communicator.h>
#include    <communicator/exception.h>
#include    <communicator/loadavg.h>
#include    <communicator/names.h>


// eventdispatcher
//
#include    <eventdispatcher/names.h>


// libaddr
//
//#include    <libaddr/addr.h>
#include    <libaddr/addr_parser.h>
//#include    <libaddr/iface.h>
//#include    <libaddr/addr_unix.h>


// advgetopt
//
//#include    <advgetopt/conf_file.h>
//#include    <advgetopt/validator_integer.h>


// snaplogger
//
//#include    <snaplogger/message.h>


// serverplugins
//
//#include    <serverplugins/collection.h>


// snapdev
//
//#include    <snapdev/file_contents.h>
//#include    <snapdev/not_used.h>
//#include    <snapdev/not_reached.h>
//#include    <snapdev/trim_string.h>
#include    <snapdev/tokenize_string.h>


// C++
//
#include    <thread>


// last include
//
#include    <snapdev/poison.h>



namespace communicator_daemon
{
namespace loadavg
{



SERVERPLUGINS_START(loadavg)
    , ::serverplugins::description(
            "Load the load average of this computer and share with others.")
    , ::serverplugins::dependency("communicatord")
    , ::serverplugins::help_uri("https://snapwebsites.org/help")
    , ::serverplugins::categorization_tag("statistics")
SERVERPLUGINS_END(loadavg)




/** \brief Initialize loadavg.
 *
 * This function terminates the initialization of the loadavg plugin
 * by registering for different events.
 */
void loadavg::bootstrap()
{
    SERVERPLUGINS_LISTEN(loadavg, communicatord, initialize, std::placeholders::_1);
    SERVERPLUGINS_LISTEN0(loadavg, communicatord, terminate);
    SERVERPLUGINS_LISTEN(loadavg, communicatord, new_connection, std::placeholders::_1);
}


/** \brief Finish initialization by adding more functionality.
 *
 * This function initializes the loadavg functionality of the communicator
 * daemon.
 *
 * This allows other services and communicators to receive the loadavg
 * information from the computer running this communicator daemon.
 *
 * \todo
 * The opts is not changeable from the plugins, instead we need the
 * serverplugins to load config files as required. There is already
 * an example in the sitter: `void server::add_plugin_options()`.
 * But since that would be useful for all that have plugins, we want
 * that to be moved to the serverplugins instead of copy/pasting that
 * everywhere.
 *
 * \param[in] opts  A set of command line options.
 */
void loadavg::on_initialize(advgetopt::getopt & opts)
{
    SNAP_LOG_CONFIGURATION
        << "loadavg::on_initialization(): processing..."
        << SNAP_LOG_SEND;

    f_communicator = ed::communicator::instance();

    f_number_of_processors = std::max(1U, std::thread::hardware_concurrency());

    // here we piggy back on an option that the communicatord defines for
    // itself so no need to have our own definition
    //
    communicator::set_loadavg_path(opts.get_string("data-path"));

    if(f_tag != ed::dispatcher_match::DISPATCHER_MATCH_NO_TAG)
    {
        throw communicator::logic_error("plugin already initialized.");
    }
    f_tag = ed::dispatcher_match::get_next_tag();

    communicatord::pointer_t s(plugins()->get_server<communicatord>());

    ed::dispatcher::pointer_t dispatcher(s->get_dispatcher());
    dispatcher->add_matches({
        ::ed::define_match(
              ::ed::Expression(communicator::g_name_communicator_cmd_listen_loadavg)
            , ::ed::Callback(std::bind(&loadavg::msg_listen_loadavg, this, std::placeholders::_1))
            , ::ed::Tag(f_tag)
        ),
        ::ed::define_match(
              ::ed::Expression(communicator::g_name_communicator_cmd_loadavg)
            , ::ed::Callback(std::bind(&loadavg::msg_save_loadavg, this, std::placeholders::_1))
            , ::ed::Tag(f_tag)
        ),
        ::ed::define_match(
              ::ed::Expression(communicator::g_name_communicator_cmd_register_for_loadavg)
            , ::ed::Callback(std::bind(&loadavg::msg_register_for_loadavg, this, std::placeholders::_1))
            , ::ed::Tag(f_tag)
        ),
        ::ed::define_match(
              ::ed::Expression(communicator::g_name_communicator_cmd_unregister_from_loadavg)
            , ::ed::Callback(std::bind(&loadavg::msg_unregister_from_loadavg, this, std::placeholders::_1))
            , ::ed::Tag(f_tag)
        ),
    });

    f_loadavg_timer = std::make_shared<load_timer>(this);
    f_loadavg_timer->set_name("loadavg_timer");
    if(!f_communicator->add_connection(f_loadavg_timer))
    {
        f_loadavg_timer.reset();

        SNAP_LOG_ERROR
            << "could not add the check_clock_status connection to ed::communicator"
            << SNAP_LOG_SEND;
    }

    SNAP_LOG_CONFIGURATION
        << "loadavg::on_initialization(): ready."
        << SNAP_LOG_SEND;
}


void loadavg::on_terminate()
{
    f_communicator->remove_connection(f_loadavg_timer);
    f_loadavg_timer.reset();

    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    s->get_dispatcher()->remove_matches(f_tag);
}


/** \brief Request LOADAVG messages from a communicatord.
 *
 * This function gets called whenever a local service sends us a
 * request to listen to the LOADAVG messages of one or more
 * communicatord.
 *
 * \param[in] msg  The LISTEN_LOADAVG message.
 */
void loadavg::msg_listen_loadavg(ed::message & msg)
{
    std::string const ips(msg.get_parameter(communicator::g_name_communicator_param_ips));

    // we save those as IP addresses since the remote
    // communicators come and go and we have to make sure
    // that all get our REGISTER_FOR_LOADAVG message when they
    // come back after a broken link
    //
    // Note: I don't think this is correct since if one registers anew each
    //       time then it would be re-added once the other communicator is ready
    //
    advgetopt::string_set_t ip_list;
    snapdev::tokenize_string(ip_list, ips, { "," });

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


void loadavg::msg_save_loadavg(ed::message & msg)
{
    std::string const avg_str(msg.get_parameter(communicator::g_name_communicator_param_avg));
    std::string const my_address(msg.get_parameter(ed::g_name_ed_param_my_address));
    snapdev::timespec_ex const timestamp_str(msg.get_timespec_parameter(communicator::g_name_communicator_param_timestamp));

    communicator::loadavg_item item;

    // Note: we do not use the port so whatever number here is fine
    addr::addr a(addr::string_to_addr(
                  my_address
                , "127.0.0.1"
                , communicator::LOCAL_PORT  // the port is ignore, use a safe default
                , "tcp"));
    a.set_port(communicator::LOCAL_PORT); // actually force the port so in effect it is ignored
    a.get_ipv6(item.f_address);

    item.f_avg = std::stof(avg_str);
    if(item.f_avg < 0.0)
    {
        return;
    }

    item.f_timestamp = timestamp_str;
    if(snapdev::timespec_ex(item.f_timestamp) < snapdev::timespec_ex(SERVERPLUGINS_UNIX_TIMESTAMP(UTC_BUILD_YEAR, 1, 1, 0, 0, 0), 0))
    {
        return;
    }

    communicator::loadavg_file file;
    file.load();
    file.add(item);
    file.save();
}


void loadavg::msg_register_for_loadavg(ed::message & msg)
{
    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    if(!s->is_tcp_connection(msg))
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


void loadavg::msg_unregister_from_loadavg(ed::message & msg)
{
    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    if(!s->is_tcp_connection(msg))
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


void loadavg::register_for_loadavg(std::string const & ip)
{
    addr::addr address(addr::string_to_addr(
              ip
            , "127.0.0.1"
            , communicator::LOCAL_PORT  // the port is ignored, use a safe default
            , "tcp"));
    ed::connection::vector_t const & all_connections(f_communicator->get_connections());
    auto const & it(std::find_if(
            all_connections.begin(),
            all_connections.end(),
            [address](auto const & conn)
            {
                remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(conn));
                if(remote_conn != nullptr)
                {
                    return remote_conn->get_connection_address() == address;
                }
                else
                {
                    service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(conn));
                    if(service_conn != nullptr)
                    {
                        return service_conn->get_connection_address() == address;
                    }
                }

                return false;
            }));

    if(it != all_connections.end())
    {
        // there is such a connection, send it a request for LOADAVG messages
        //
        base_connection::pointer_t conn(std::dynamic_pointer_cast<base_connection>(*it));
        on_new_connection(conn);
    }
}


void loadavg::on_new_connection(base_connection::pointer_t conn)
{
    // when connecting to another communicator, we want to register to
    // receive it's loadavg data so we send that message immediately
    // (TBD: this maybe too soon? although I think it's after the ACCEPT
    // or REGISTER-ed so we should be good.)
    //
    ed::message register_message;
    register_message.set_command(communicator::g_name_communicator_cmd_register_for_loadavg);
    communicatord::pointer_t s(plugins()->get_server<communicatord>());
    register_message.set_sent_from_server(s->get_server_name());
    register_message.set_sent_from_service(communicator::g_name_communicator_service_communicatord);

    remote_connection::pointer_t remote_conn(std::dynamic_pointer_cast<remote_connection>(conn));
    if(remote_conn != nullptr)
    {
        remote_conn->send_message(register_message);
    }
    else
    {
        service_connection::pointer_t service_conn(std::dynamic_pointer_cast<service_connection>(conn));
        if(service_conn != nullptr)
        {
            service_conn->send_message(register_message);
        }
    }
}



void loadavg::process_load_balancing()
{
    static bool error_emitted = false;
    std::ifstream in;
    in.open("/proc/loadavg", std::ios::in | std::ios::binary);
    if(in.is_open())
    {
        error_emitted = false;

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
        load_avg.set_command(communicator::g_name_communicator_cmd_loadavg);
        communicatord::pointer_t s(plugins()->get_server<communicatord>());
        load_avg.set_sent_from_server(s->get_server_name());
        load_avg.set_sent_from_service(communicator::g_name_communicator_service_communicatord);
        std::stringstream ss;
        ss << avg;
        load_avg.add_parameter(communicator::g_name_communicator_param_avg, ss.str());
        load_avg.add_parameter(
                  ed::g_name_ed_param_my_address
                , s->get_connection_address().to_ipv4or6_string(addr::STRING_IP_BRACKET_ADDRESS | addr::STRING_IP_PORT));
        load_avg.add_parameter(communicator::g_name_communicator_param_timestamp, snapdev::now());

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
    else if(!error_emitted)
    {
        error_emitted = false;
        SNAP_LOG_ERROR
            << "error opening file \"/proc/loadavg\"."
            << SNAP_LOG_SEND;
    }
}



} // namespace loadavg
} // namespace communicator_daemon
// vim: ts=4 sw=4 et

// Copyright (c) 2011-2024  Made to Order Software Corp.  All Rights Reserved
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
#include    <communicatord/names.h>
#include    <communicatord/version.h>


// eventdispatcher
//
#include    <eventdispatcher/tcp_client_message_connection.h>
#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/dispatcher.h>


// libaddr
//
#include    <libaddr/addr_parser.h>


// snapdev
//
#include    <snapdev/not_reached.h>
#include    <snapdev/stringize.h>


// advgetopt
//
#include    <advgetopt/advgetopt.h>
#include    <advgetopt/conf_file.h>
#include    <advgetopt/exception.h>


// last include
//
#include    <snapdev/poison.h>



namespace
{


class cluster;

class cluster_messenger
    : public ed::tcp_client_message_connection
{
public:
    typedef std::shared_ptr<cluster_messenger>     pointer_t;

                                cluster_messenger(
                                      cluster * sl
                                    , addr::addr const & address);
                                cluster_messenger(cluster_messenger const & rhs) = delete;
    virtual                     ~cluster_messenger() override {}

    cluster_messenger &     operator = (cluster_messenger const & rhs) = delete;

protected:
    // this is owned by a snaplock function so no need for a smart pointer
    // (and it would create a loop)
    cluster *               f_cluster = nullptr;
};


cluster_messenger::cluster_messenger(
              cluster * sl
            , addr::addr const & address)
    : tcp_client_message_connection(address)
    , f_cluster(sl)
{
    set_name("cluster messenger");
}



#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
class cluster
    : public ed::connection_with_send_message
    , public ed::dispatcher
    , public std::enable_shared_from_this<cluster>
{
public:
    typedef std::shared_ptr<cluster>      pointer_t;

                                cluster(int argc, char * argv[]);
                                cluster(cluster const & rhs) = delete;
    virtual                     ~cluster() {}

    cluster &                   operator = (cluster const & rhs) = delete;

    int                         run();

    // ed::connection_with_send_message implementation
    virtual bool                send_message(ed::message & msg, bool cache = false) override;
    virtual void                ready(ed::message & msg) override; // no "msg_" because that's in connection_with_send_message
    virtual void                stop(bool quitting) override; // no "msg_" because that's in connection_with_send_message

private:
    void                        done(ed::message & msg);

    // messages handled by the dispatcher
    // (see also ready() and stop() above)
    //
    void                        msg_cluster_status(ed::message & msg);
    void                        msg_cluster_complete(ed::message & msg);

    advgetopt::getopt                   f_opts;
    advgetopt::conf_file::pointer_t     f_communicatord_config = advgetopt::conf_file::pointer_t();
    addr::addr                          f_communicator_addr = addr::addr();
    ed::communicator::pointer_t         f_communicator = ed::communicator::pointer_t();
    cluster_messenger::pointer_t        f_messenger = cluster_messenger::pointer_t();
    std::string                         f_cluster_status = std::string();       // UP or DOWN
    std::string                         f_cluster_complete = std::string();     // COMPLETE or INCOMPLETE
    size_t                              f_neighbors_count = 0;
};
#pragma GCC diagnostic pop






advgetopt::option const g_options[] =
{
    advgetopt::define_option(
          advgetopt::Name("communicatord-config")
        , advgetopt::Flags(advgetopt::all_flags<
              advgetopt::GETOPT_FLAG_REQUIRED
            , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::DefaultValue("/etc/communicatord/communicatord.conf")
        , advgetopt::Help("path to the communicatord configuration file.")
    ),
    advgetopt::end_options()
};




// until we have C++20 remove warnings this way
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "communicatord",
    .f_group_name = "communicatord",
    .f_options = g_options,
    .f_options_files_directory = nullptr,
    .f_environment_variable_name = nullptr,
    .f_environment_variable_intro = nullptr,
    .f_section_variables_name = nullptr,
    .f_configuration_files = nullptr,
    .f_configuration_filename = nullptr,
    .f_configuration_directories = nullptr,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = COMMUNICATORD_VERSION_STRING,
    .f_license = "GNU GPL v3",
    .f_copyright = "Copyright (c) 2011-"
                   SNAPDEV_STRINGIZE(UTC_BUILD_YEAR)
                   " by Made to Order Software Corporation -- All Rights Reserved",
    //.f_build_date = UTC_BUILD_DATE,
    //.f_build_time = UTC_BUILD_TIME
};
#pragma GCC diagnostic pop







cluster::cluster(int argc, char * argv[])
    : dispatcher(this)
    , f_opts(g_options_environment)
    , f_communicator(ed::communicator::instance())
{
    add_matches({
        DISPATCHER_MATCH(communicatord::g_name_communicatord_cmd_cluster_up, &cluster::msg_cluster_status),
        DISPATCHER_MATCH(communicatord::g_name_communicatord_cmd_cluster_down, &cluster::msg_cluster_status),
        DISPATCHER_MATCH(communicatord::g_name_communicatord_cmd_cluster_complete, &cluster::msg_cluster_complete),
        DISPATCHER_MATCH(communicatord::g_name_communicatord_cmd_cluster_incomplete, &cluster::msg_cluster_complete),
    });

    f_opts.finish_parsing(argc, argv);

    advgetopt::conf_file_setup setup(f_opts.get_string("communicatord-config"));
    f_communicatord_config = advgetopt::conf_file::get_conf_file(setup);

    // TODO: convert to using the communidatord library object
    f_communicator_addr = addr::string_to_addr(
                  f_communicatord_config->get_parameter(communicatord::g_name_communicatord_config_local_listen).c_str()
                , "localhost"
                , 4040
                , "tcp");
}


int cluster::run()
{
    f_messenger = std::make_shared<cluster_messenger>(
                              this
                            , f_communicator_addr);
    f_messenger->set_dispatcher(shared_from_this());
    f_communicator->add_connection(f_messenger);

    // our messenger here is a direct connection (not a permanent one) so
    // we have to REGISTER immediately (if it couldn't connect we raise
    // an exception so this works)
    //
    ed::message register_cluster;
    register_cluster.set_command(communicatord::g_name_communicatord_cmd_register);
    register_cluster.add_parameter(
                      communicatord::g_name_communicatord_param_service
                    , communicatord::g_name_communicatord_service_cluster);
    register_cluster.add_version_parameter();
    send_message(register_cluster);

    f_communicator->run();

    return 0;
}


bool cluster::send_message(ed::message & msg, bool cache)
{
    return f_messenger->send_message(msg, cache);
}


void cluster::ready(ed::message & msg)
{
    snapdev::NOT_USED(msg);

    ed::message clusterstatus_message;
    clusterstatus_message.set_command(communicatord::g_name_communicatord_cmd_cluster_status);
    clusterstatus_message.set_service(communicatord::g_name_communicatord_service_communicatord);
    send_message(clusterstatus_message);
}


void cluster::stop(bool quitting)
{
    snapdev::NOT_USED(quitting);

    if(f_messenger != nullptr)
    {
        f_communicator->remove_connection(f_messenger);
        f_messenger.reset();
    }
}


void cluster::msg_cluster_status(ed::message & msg)
{
    f_cluster_status = msg.get_command();
    done(msg);
}


void cluster::msg_cluster_complete(ed::message & msg)
{
    f_cluster_complete = msg.get_command();
    done(msg);
}


void cluster::done(ed::message & msg)
{
    if(f_cluster_status.empty()
    || f_cluster_complete.empty())
    {
        // not quite done yet...
        return;
    }

    f_neighbors_count = msg.get_integer_parameter("neighbors_count");

    // got our info!
    //
    std::cout << "              Status: " << f_cluster_status          << '\n'
              << "            Complete: " << f_cluster_complete        << '\n'
              << "Computers in Cluster: " << f_neighbors_count         << '\n'
              << " Quorum of Computers: " << f_neighbors_count / 2 + 1 << '\n';

    // we're done, remove the messenger which is enough for the
    // communicator::run() to return
    //
    stop(false);
}



}
// no name namespace


int main(int argc, char * argv[])
{
    try
    {
        cluster::pointer_t cluster(std::make_shared<cluster>(argc, argv));
        return cluster->run();
    }
    catch(advgetopt::getopt_exit const & e)
    {
        return e.code();
    }
    catch(std::exception const & e)
    {
        // clean error on exception
        std::cerr << "clusterstatus: an exception occurred: " << e.what() << '\n';
        return 1;
    }
    catch(...)
    {
        std::cerr << "clusterstatus: an unknown exception occurred.\n";
        return 1;
    }
}


// vim: ts=4 sw=4 et

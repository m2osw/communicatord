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

/** \file
 * \brief Verify some of the communicator client implementation.
 *
 * This test file implements verification of the communicator client
 * class.
 */

// communicator
//
#include    <communicator/communicator.h>
#include    <communicator/exception.h>
#include    <communicator/version.h>


// eventdispatcher
//
#include    <eventdispatcher/communicator.h>
#include    <eventdispatcher/dispatcher.h>
#include    <eventdispatcher/names.h>
#include    <eventdispatcher/tcp_client_permanent_message_connection.h>

#include    <eventdispatcher/reporter/executor.h>
#include    <eventdispatcher/reporter/lexer.h>
#include    <eventdispatcher/reporter/parser.h>
#include    <eventdispatcher/reporter/state.h>


// self
//
#include    "catch_main.h"



namespace
{



advgetopt::option const g_options[] =
{
    advgetopt::define_option(
          advgetopt::Name("fancy-option")
        , advgetopt::Flags(advgetopt::all_flags<
                      advgetopt::GETOPT_FLAG_REQUIRED
                    , advgetopt::GETOPT_FLAG_GROUP_OPTIONS>())
        , advgetopt::Help("The fancy option.")
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


//constexpr char const * const g_configuration_files[] =
//{
//    "/etc/communicator/communicatord.conf",
//    nullptr
//};


advgetopt::options_environment const g_options_environment =
{
    .f_project_name = "communicator",
    .f_group_name = "communicator",
    .f_options = g_options,
    .f_environment_variable_name = "COMMUNICATOR_OPTIONS",
    //.f_configuration_files = g_configuration_files,
    .f_environment_flags = advgetopt::GETOPT_ENVIRONMENT_FLAG_SYSTEM_PARAMETERS
                         | advgetopt::GETOPT_ENVIRONMENT_FLAG_PROCESS_SYSTEM_PARAMETERS,
    .f_help_header = "Usage: %p [-<opt>]\n"
                     "where -<opt> is one or more of:",
    .f_help_footer = "%c",
    .f_version = COMMUNICATOR_VERSION_STRING,
    .f_license = "GNU GPL v3",
    .f_copyright = "Copyright Notice",
    .f_groups = g_group_descriptions,
};



addr::addr get_address()
{
    addr::addr a;
    sockaddr_in ip = {
        .sin_family = AF_INET,
        .sin_port = htons(20002),
        .sin_addr = {
            .s_addr = htonl(0x7f000001),
        },
        .sin_zero = {},
    };
    a.set_ipv4(ip);
    return a;
}


// the cluck class requires a messenger to function, it is a client
// extension instead of a standalone client
//
class test_messenger
    : public communicator::communicator
    , public ed::manage_message_definition_paths
{
public:
    typedef std::shared_ptr<test_messenger> pointer_t;

    enum class sequence_t
    {
        SEQUENCE_SUCCESS,
    };

    test_messenger(
              advgetopt::getopt & opts
            , int argc
            , char * argv[]
            , sequence_t sequence)
        : communicator(opts, "test_communicator_client")
        , manage_message_definition_paths(
                // WARNING: the order matters, we want to test with our source
                //          (i.e. original) files first
                //
                SNAP_CATCH2_NAMESPACE::g_source_dir() + "/tests/message-definitions:"
                    + SNAP_CATCH2_NAMESPACE::g_source_dir() + "/daemon/message-definitions:"
                    + SNAP_CATCH2_NAMESPACE::g_dist_dir() + "/share/eventdispatcher/messages")
        , f_sequence(sequence)
        , f_dispatcher(std::make_shared<ed::dispatcher>(this))
    {
        set_name("test_messenger");    // connection name
        set_dispatcher(f_dispatcher);

        opts.finish_parsing(argc, argv);

        f_dispatcher->add_matches({
            DISPATCHER_MATCH("DATA", &test_messenger::msg_data),
            //DISPATCHER_CATCH_ALL(),
        });
        f_dispatcher->add_communicator_commands();

        // further dispatcher initialization
        //
#ifdef _DEBUG
        f_dispatcher->set_trace();
        f_dispatcher->set_show_matches();
#endif
    }

    void finish_init()
    {
        process_communicator_options();

        // that function cannot be called again
        //
        CATCH_REQUIRE_THROWS_MATCHES(
              process_communicator_options()
            , ::communicator::logic_error
            , Catch::Matchers::ExceptionMessage("logic_error: process_communicator_options() called twice."));

        CATCH_REQUIRE(service_name() == "test_communicator_client");

        // when we start, we're not connected
        //
        CATCH_REQUIRE_FALSE(is_connected());

        // at this point the communicator is not connected so sending
        // messages fails with false
        //
        ed::message too_early;
        too_early.set_command("TOO_EARLY");
        CATCH_REQUIRE_FALSE(send_message(too_early));
    }

    ed::dispatcher::pointer_t get_dispatcher() const
    {
        return f_dispatcher;
    }

//    virtual void process_connected() override
//    {
//        // always register at the time we connect
//        //
//        tcp_client_permanent_message_connection::process_connected();
//
//std::cerr << "--- ready!\n";
//    }


    void msg_data(ed::message & msg)
    {
std::cerr << "--- got DATA message: " << msg << "...\n";
        CATCH_REQUIRE(msg.get_sent_from_server() == "monster");
        CATCH_REQUIRE(msg.get_sent_from_service() == "test_communicator_client");
        CATCH_REQUIRE(msg.get_server() == "communicatord");
        CATCH_REQUIRE(msg.get_service() == "communicator_test");
        CATCH_REQUIRE(msg.has_parameter("filename"));
        CATCH_REQUIRE(msg.get_parameter("filename") == "/var/log/communicator/communicatord.log");

        //std::string const data(msg.get_parameter("data"));
        //std::int64_t const size(msg.get_integer_parameter("size"));
        //CATCH_REQUIRE(data.size() == static_cast<std::string::size_type>(size));

        // since we received a message, we're connected (registered)
        //
        CATCH_REQUIRE(is_connected());

        ed::message reply;
        reply.reply_to(msg);
        reply.set_command("DONE");
        CATCH_REQUIRE(send_message(reply));
    }

    //virtual void msg_reply_with_unknown(ed::message & msg) override
    //{
    //    tcp_client_permanent_message_connection::msg_reply_with_unknown(msg);

    //    // note that the cluck class has no idea about the unknown
    //    // message so we do not get our finally() callback called
    //    // automatically here (we should not get UNKNOWN messages
    //    // about cluck anyway)
    //    //
    //    switch(f_sequence)
    //    {
    //    //case sequence_t::SEQUENCE_FAIL_MISSING_LOCKED_PARAMETERS:
    //        //break;

    //    default:
    //        break;

    //    }
    //}

    void stop(bool quitting)
    {
        unregister_communicator(quitting);

        if(quitting)
        {
            // at this point the communicator cleared its messenger
            //
            ed::message too_late;
            too_late.set_command("TOO_LATE");
            CATCH_REQUIRE_FALSE(send_message(too_late));
        }

        //disconnect();
    }

    void disconnect()
    {
        remove_from_communicator();

        ed::connection::pointer_t timer_ptr(f_timer.lock());
        if(timer_ptr != nullptr)
        {
            timer_ptr->remove_from_communicator();
        }
    }

    void set_timer(ed::connection::pointer_t done_timer)
    {
        f_timer = done_timer;
    }

private:
    // the sequence & step define the next action
    //
    //advgetopt::getopt           f_opts;
    sequence_t                  f_sequence = sequence_t::SEQUENCE_SUCCESS;
    ed::dispatcher::pointer_t   f_dispatcher = ed::dispatcher::pointer_t();
    int                         f_step = 0;
    ed::connection::weak_pointer_t
                                f_timer = ed::connection::weak_pointer_t();
};


class test_timer
    : public ed::timer
{
public:
    typedef std::shared_ptr<test_timer> pointer_t;

    test_timer(test_messenger::pointer_t m)
        : timer(-1)
        , f_messenger(m)
    {
        set_name("test_timer");
    }

    void process_timeout()
    {
        remove_from_communicator();
        f_messenger->remove_from_communicator();
        f_timed_out = true;
    }

    bool timed_out_prima() const
    {
        return f_timed_out;
    }

private:
    test_messenger::pointer_t   f_messenger = test_messenger::pointer_t();
    bool                        f_timed_out = false;
};



} // no name namespace



CATCH_TEST_CASE("communicator", "[client]")
{
    CATCH_START_SECTION("communicator: verify default strings")
    {
        std::stringstream port_str;
        port_str << communicator::LOCAL_PORT;
        CATCH_REQUIRE(port_str.str() == std::string(communicator::g_communicator_default_port));

        std::stringstream ip_port;
        ip_port << communicator::g_communicator_default_ip << ":" << communicator::LOCAL_PORT;
        CATCH_REQUIRE(ip_port.str() == std::string(communicator::g_communicator_default_ip_port));
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("communicator: verify request_failure()")
    {
        ed::message msg;
        CATCH_REQUIRE_FALSE(msg.has_parameter("transmission_report"));
        communicator::request_failure(msg);
        CATCH_REQUIRE(msg.has_parameter("transmission_report"));
        CATCH_REQUIRE(msg.get_parameter("transmission_report") == "failure");
    }
    CATCH_END_SECTION()
}


CATCH_TEST_CASE("communicator_client_connection", "[client]")
{
    CATCH_START_SECTION("communicator_client_connection: service name cannot be an empty string")
    {
        advgetopt::getopt opts(g_options_environment);
        CATCH_REQUIRE_THROWS_MATCHES(
              std::make_shared<communicator::communicator>(opts, "")
            , communicator::invalid_name
            , Catch::Matchers::ExceptionMessage("communicator_exception: the service_name parameter of the communicator constructor cannot be an empty string."));
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("communicator_client_connection: test communicator client (regular stop)")
    {
        std::string const source_dir(SNAP_CATCH2_NAMESPACE::g_source_dir());
        std::string const filename(source_dir + "/tests/rprtr/communicator_server_test.rprtr");
        SNAP_CATCH2_NAMESPACE::reporter::lexer::pointer_t l(SNAP_CATCH2_NAMESPACE::reporter::create_lexer(filename));
        CATCH_REQUIRE(l != nullptr);
        SNAP_CATCH2_NAMESPACE::reporter::state::pointer_t s(std::make_shared<SNAP_CATCH2_NAMESPACE::reporter::state>());
        SNAP_CATCH2_NAMESPACE::reporter::parser::pointer_t p(std::make_shared<SNAP_CATCH2_NAMESPACE::reporter::parser>(l, s));
        p->parse_program();

        SNAP_CATCH2_NAMESPACE::reporter::executor::pointer_t e(std::make_shared<SNAP_CATCH2_NAMESPACE::reporter::executor>(s));
        e->start();

        addr::addr a(get_address());
        std::vector<std::string> const args = {
            "test-service", // name of command
            "--communicatord-listen",
            "cd://" + a.to_ipv4or6_string(addr::STRING_IP_ADDRESS_PORT),
            "--path-to-message-definitions",

            // WARNING: the order matters, we want to test with our source
            //          (i.e. original) files first
            //
            SNAP_CATCH2_NAMESPACE::g_source_dir() + "/tests/message-definitions:"
                + SNAP_CATCH2_NAMESPACE::g_source_dir() + "/daemon/message-definitions:"
                + SNAP_CATCH2_NAMESPACE::g_dist_dir() + "/share/eventdispatcher/messages",
        };

        // convert arguments so we can use them with execvpe()
        //
        std::vector<char const *> args_strings;
        args_strings.reserve(args.size() + 1);
        for(auto const & arg : args)
        {
            args_strings.push_back(arg.c_str());
        }
        args_strings.push_back(nullptr); // NULL terminated

        advgetopt::getopt opts(g_options_environment);
        test_messenger::pointer_t messenger(std::make_shared<test_messenger>(
                  opts
                , args.size()
                , const_cast<char **>(args_strings.data())
                , test_messenger::sequence_t::SEQUENCE_SUCCESS));
        messenger->finish_init();
        ed::communicator::instance()->add_connection(messenger);
        test_timer::pointer_t timer(std::make_shared<test_timer>(messenger));
        ed::communicator::instance()->add_connection(timer);
        messenger->set_timer(timer);

        CATCH_REQUIRE(messenger->service_name() == "test_communicator_client");

        e->set_thread_done_callback([messenger, timer]()
            {
                ed::communicator::instance()->remove_connection(messenger);
                ed::communicator::instance()->remove_connection(timer);
            });

        CATCH_REQUIRE(e->run());

        CATCH_REQUIRE(s->get_exit_code() == 0);
    }
    CATCH_END_SECTION()

    CATCH_START_SECTION("communicator_client_connection: test communicator client (quitting)")
    {
        std::string const source_dir(SNAP_CATCH2_NAMESPACE::g_source_dir());
        std::string const filename(source_dir + "/tests/rprtr/communicator_server_test_quitting.rprtr");
        SNAP_CATCH2_NAMESPACE::reporter::lexer::pointer_t l(SNAP_CATCH2_NAMESPACE::reporter::create_lexer(filename));
        CATCH_REQUIRE(l != nullptr);
        SNAP_CATCH2_NAMESPACE::reporter::state::pointer_t s(std::make_shared<SNAP_CATCH2_NAMESPACE::reporter::state>());
        SNAP_CATCH2_NAMESPACE::reporter::parser::pointer_t p(std::make_shared<SNAP_CATCH2_NAMESPACE::reporter::parser>(l, s));
        p->parse_program();

        SNAP_CATCH2_NAMESPACE::reporter::executor::pointer_t e(std::make_shared<SNAP_CATCH2_NAMESPACE::reporter::executor>(s));
        e->start();

        addr::addr a(get_address());
        std::vector<std::string> const args = {
            "test-service", // name of command
            "--communicatord-listen",
            "cd://" + a.to_ipv4or6_string(addr::STRING_IP_ADDRESS_PORT),
            "--path-to-message-definitions",

            // WARNING: the order matters, we want to test with our source
            //          (i.e. original) files first
            //
            SNAP_CATCH2_NAMESPACE::g_source_dir() + "/tests/message-definitions:"
                + SNAP_CATCH2_NAMESPACE::g_source_dir() + "/daemon/message-definitions:"
                + SNAP_CATCH2_NAMESPACE::g_dist_dir() + "/share/eventdispatcher/messages",
        };

        // convert arguments so we can use them with execvpe()
        //
        std::vector<char const *> args_strings;
        args_strings.reserve(args.size() + 1);
        for(auto const & arg : args)
        {
            args_strings.push_back(arg.c_str());
        }
        args_strings.push_back(nullptr); // NULL terminated

        advgetopt::getopt opts(g_options_environment);
        test_messenger::pointer_t messenger(std::make_shared<test_messenger>(
                  opts
                , args.size()
                , const_cast<char **>(args_strings.data())
                , test_messenger::sequence_t::SEQUENCE_SUCCESS));
        messenger->finish_init();
        ed::communicator::instance()->add_connection(messenger);
        test_timer::pointer_t timer(std::make_shared<test_timer>(messenger));
        ed::communicator::instance()->add_connection(timer);
        messenger->set_timer(timer);

        CATCH_REQUIRE(messenger->service_name() == "test_communicator_client");

        e->set_thread_done_callback([messenger, timer]()
            {
                ed::communicator::instance()->remove_connection(messenger);
                ed::communicator::instance()->remove_connection(timer);
            });

        CATCH_REQUIRE(e->run());

        CATCH_REQUIRE(s->get_exit_code() == 0);
    }
    CATCH_END_SECTION()
}


// vim: ts=4 sw=4 et

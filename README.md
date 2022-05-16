
# Introduction

The Communicator daemon is a way to send messages to any one of your
services on your entire cluster of Snap! Websites. This is a form of RPC
system that works on a standalone computer or in a complete cluster.

What you need to do for this to work is:

1. Link against the eventdispatcher library
2. Register your services with your local Communicator
3. Send messages to the Communicator
4. List of the messages you understand with the Communicator
5. Tell each Communicator where the others are

Point (5) is only if you have more than one computer.

Points (1) to (4) are to make it work where all your services only need to
know about the Communicator. All the other services are auomatically
messaged through the communicator.

In other words, once your service connects to the Communicator, it
is visible to all the other services that are connected to a Communicator
in the entire cluster. Sending a message from service A to service B is 100%
managed by the Communicator, wherever these services are running (on
computer X, and Z for example). The Communicator is capable of creating
a graph in case you use multiple networks and need to have intermediary
Communicators used to send a message between two services.


# Library

The project includes a library extension which allows you to connect to
the controller totally effortlessly.

    # Basic idea at the moment...
    class MyService
        : public communicatord::communicator("<name>")
    {
        MyService()
            : add_communicator_options(f_opts)
        {
            ...
            if(!process_communicator_options(f_opts, "/etc/snapwebsites"))
            {
                // handled failure
                ...
            }

            // from here on, the communicator is viewed as connected
            // internally, the communicator started a connection and it
            // will automatically REGISTER itself
        }
    }

It also simplifies sending message as you don't have to know everything
about the eventdispatcher library to send messages with this library
extension.

    COMMUNICATORD_MESSAGE("COMMAND")
        << communicatord::param("name", "value")
        << ...
        << communicatord::cache
        << communicatord::...
        << COMMUNICATORD_SEND;

So, something similar to the snaplogger feature, but for messages in the
eventdispatcher (actually, this may be a feature that can live in the
eventdispatcher in which case we could use `ED_MESSAGE` and
`ED_SEND`, the TCP connection to the communicator would be the
default place where these messages would go).

Another idea, if we do not need a copy, would be to use a `message()`
function which returns a reference which we can use just like the `<<`
but the syntax would be more like:

    communicator.message("COMMAND").param("name", "value").cache();

Thinking about it, though, it's not really any simpler than:

    ed::message msg("COMMAND");
    msg.add_parameter("name", "value");
    connection.send_message(msg, true);


# Daemon

The Communicator is primarily a deamon which can graphically map your
network and the location of each of your services to allow for messages to
seamlessly travel between all the services.


# Tools

The projects comes with a few useful tools to send messages in script
(such as the logrotate scripts) and debug your systems by sniffing
the traffic going through the Communicator.


# Protocols

The Communicator has its own protocol and we can access it using a
protocol name as follow:

* `cd:` -- basic stream connection
* `cds:` -- secured stream connection
* `cdu:` -- basic datagram connection

The address defines whether we have an IP based connection (TCP/IP or UDP/IP)
or a Unix based connection (path to a socket). The `cds` protocol cannot
be used with a Unix based connection (not available).

Whether the connection is local or remote is defined by the IP address. By
default, the `cdu` is limited to local connections (127.0.0.1). In most cases,
the `cds` is used between data centers (i.e. within one center, it is assumed
that a non-encrypted connection works just fine).


# License

The project is covered by the GPL 3.0 license.


# Bugs

Submit bug reports and patches on
[github](https://github.com/m2osw/communicator/issues).


_This file is part of the [snapcpp project](https://snapwebsites.org/)._

vim: ts=4 sw=4 et

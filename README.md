
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

**IMPORTANT NOTE:** In most cases, commands and names are statically defined
                    values are found in the `names.an` file. You should use
                    those definition instead by including the `names.h` and
                    then names such as:

                        communicatord::g_name_communicatord_param_uri

## Flags

The `communicatord` library inheriated the ability to raise and lower
flags. In most cases, this is done with simple macros.

First we want to raise a flag:

    COMMUNICATORD_FLAG_UP(
              "<service>"
            , "<section>"
            , "<flag>"
            , "<description>")
                ->set_priority(<priority>)
                .set_manual_down(false)
                .add_tag("<tag1>")
                .add_tag("<tag2>")
                ...
                .add_tag("<tagN>")
                .save();

Except for the `save()`, the other parameters are optional. If you prefer,
you can save the pointer and reuse it for each additional call:

    auto flag(COMMUNICATORD_FLAG_UP(
              "<service>"
            , "<section>"
            , "<flag>"
            , "<description>"));
    flag->set_priority(<priority>)
    flag->set_manual_down(false)
    flag->add_tag("<tag1>")
    flag->add_tag("<tag2>")
    ...
    flag->add_tag("<tagN>")
    flag->save();

If possible (i.e. `set_manual_down(false)`, which is the default value),
you then can take the flag down once the alarm goes off:

    COMMUNICATORD_FLAG_DOWN(
              "<service>"
            , "<section>"
            , "<flag>")
                ->save();

As above, you could save the pointer and call `save()` on a separate line.
Note that using the other `set_...()` functions when dropping a flag
is not necessary (that data will be ignored by the `save()`).


# Daemon

The Communicator is primarily a deamon which can graphically map your
network and the location of each of your services to allow for messages to
seamlessly travel between all the services.

## Initialization Process

Whenever your service registers with the communicator daemon using the
REGISTER message, you are expected to receive a reply: READY.

At the time you receive this reply, your service is generally ready to start.
You want to finish up your initialization and then go for it.

In some cases, though, you may want to know a little. There are two other
messages that you can request: `CLUSTER_COMPLETE` and `CLOCK_STABLE`. Once
you received all of those messages, everything is 100% ready. In most cases,
though, most processes do not care much whether the cluster is ready or not
and whether the clock is properly synchronized (although it can be a pain,
it is very unlikely that the clock will be off by much on most modern
systems so most of our services ignore that message).

    Client          Communicator Daemon

       ---- REGISTER --------->

       <--- HELP --------------
       <--- READY -------------
       (<-- cached messages --)    // if the client was already sent messages

       ---- COMMANDS --------->

       ---- CLOCK_STATUS ----->

       <--- CLOCK_STABLE ------

       ---- CLUSTER_STATUS --->

       <--- CLUSTER_UP --------

The order in which the `CLUSTER_UP` and `CLOCK_STABLE` are received
may vary in case the state changes before you send the `CLUSTER_STATUS`
and `CLOCK_STATUS` messages.

Note that if the cluster is not up yet, the communicator daemon sends a
`CLUSTER_DOWN` message instead. Similar, if the clock is not yet verified
as stable, it sends a `CLOCK_UNSTABLE`. So your process wants to understand
those messages too.

**WARNING:** The `CLUSTER_COMPLETE` should nearly never be used. Instead
             you want to consider using `CLUSTER_UP`. The cluster is
             considered up once we reach a sufficient quorum. The
             `CLUSTER_COMPLETE`, on the other hand, is only sent once
             all the computer in the cluster are up. This may never
             happen, especially on really large clusters.


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
* `cdb:` -- broadcast datagram connection (not implemented yet)

_Note: The `cdb:` scheme would be used to send a message to multiple
       communicator daemons which does not really make sense here. What you
       actually want to do is mark the message itself for broadcasting and
       the communicator daemon will take care of the rest._

The address defines whether we have an IP based connection (TCP/IP or UDP/IP)
or a Unix based connection (path to a socket). The `cds` protocol cannot
be used with a Unix based connection (not available).

Whether the connection is local or remote is defined by the IP address. By
default, the `cdu` is limited to local connections (127.0.0.1). In most cases,
the `cds` is used between data centers (i.e. within one center, it is assumed
that a non-encrypted connection is secure, using a LAN).


# License

The project is covered by the GPL 3.0 license.


# Bugs

Submit bug reports and patches on
[github](https://github.com/m2osw/communicator/issues).


_This file is part of the [snapcpp project](https://snapwebsites.org/)._

vim: ts=4 sw=4 et

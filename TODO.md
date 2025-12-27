
* Remove debug code (I have many logs that are really just debug).

* Look at whether we can separate the daemon from the main library

  I moved the daemon (like in the Sitter) to the library so the plugins
  can be loaded and linked against some of the daemon's classes. Maybe
  I should have a separate library instead. So the libcommunicator.so
  as now and a libcommunicatord.so in the daemon folder.

* Use bare pointers for child connections

  **NOTE:** this is done, just not tested 100%.

  Look into the daemon to replace the shared pointers within children with
  bare pointers. At the moment, some destructors do not get called properly
  on exit because the server reference counter is not 0. It also means all
  the children and possibly other items do not get deleted (I do a reset()
  for now, but that should be removed and all the children should properly
  be deleted--we can know by seeing whether the communicatord.sock file
  gets deleted or not when we do not reset the pointer:
  `f_unix_listener.reset();`).

  i.e. right now, if we quit because of an exception, we do not do a good
  job at cleaning up the server.

* Properly test password usage and requirements

  Any publicly accessible connection (port) must be "password" protected.
  The REGISTER and CONNECT messages both support a password field. This
  is an optional field only for local connections.

* Change the cluster status messages

  Right now, we send a `CLUSTER_STATUS` message and expect one of two replies:
  one of `CLUSTER_UP` or `CLUSTER_DOWN` and one of `CLUSTER_COMPLETE` or
  `CLUSTER_INCOMPLETE`.

  To conform to other services, I think I should change that to (1) send a
  `CLUSTER_GET_STATUS` and a reply with `CLUSTER_CURRENT_STATUS`. That way,
  the people handling the messages can handle one single message with all the
  info. The library should have the necessary to catch those messages and
  record the current status for client applications.

* Write Unit Tests

  Like cluckd, we want to create tests using the new eventdispatcher/reporter
  scripting system. Right now, I think many features in the communicator
  daemon are either redundant or incorrect. (i.e. many bugs even if it
  generally works.)

* Add help in the flag implementation so it is easier to administer

  This should be in the form of: (1) an `f_help` field allowing people
  to just follow those instructions to get things through; and (2) an
  `f_help_uri` field with a URL to a page on snapwebsites.org with complete
  documentation of the situation.

* Definition files should include the list of supported messages

  i.e. up until the time the service connects to the communicator we have no
       clue whether a message can make it to that service; by having the list
       of messages in the .service file, we could load that ahead of time and
       handle that check even before the service registers

  Note 1: we can actually look into the COMMANDS=... and save that in the
          file (we could do that each time we get a COMMANDS=... message
          so that way it stays up to date and does not require us to know
          the list of messages and install a file).

  Note 2: we already have an implementation to test a list of messages,
          see how this TODO can benefit from that

  The list of messages could be message definitions (see below). Probably a
  C-like block definition:

      GOSSIP {
        my_address {
           value=validator("address(ip)")
           require=true
           description=this communicatord IP address so others can connect to us
        }
      }

  In general, if everything is properly tested, then verifying the message
  structure itself is less of a concern. Yet, over time, it can become
  complicated to keep messages straight if not documented. And with such
  a definition we (1) document the message and (2) allow a computer to
  verify the message.

* Message definition

  i.e. at this point, I "randomly" add parameters with "random" values and
       call that a _valid_ message... it would be great to have message
       definitions which we can turn on in debug mode so that way the
       parameters of a message can be validated:
         (1) is that parameter name accepted by this command? [DONE]
         (2) is the value valid for that parameter? [PARTIAL]
         (3) are required parameter present? [DONE]
         (4) make sure message definitions do not reuse "global" parameters
             (i.e. `cache=no`, `transmission_status=failure`, etc.)
         (5) support a way to define "global" parameters
         (6) whether the message supports broadcasting or not
         (7) whether a message is local or cluster wide

  For (2) we only support generic tests at the moment (i.e. is it an
  integer, IP address, ... but not a more detailed test
  i.e. is that integer between 0 and 100).

* Time Accuracy in your Cluster

  The communicator daemon should be in charge of gathering wallclock time
  from all the computers in the cluster. Then report to the sitter if there
  are discrepancy. However, the communicator itself runs on all the computers
  so it probably isn't practical to have the communicator itself handle this.
  Instead, we probably want one separate service that can run on one of your
  backends and determine the differences. In all likelihood, though, having
  NTP running on one computer and then use that as the source of time for
  all the other computers should make it a lot more efficient and time
  accuracy should be much higher. This may not always be possible, though,
  especially on really large networks which would require separate data
  centers. In that case, having a tool to verify that our computers are all
  in sync. is certainly quite important.

  Note that the sitter is already setup to verify that some NTP system is
  up and running your computer. The communicator daemon also has its own
  implementation checking the local clock to make sure there is an NTP
  system and that system is UP.

* Broadcast only if message supported

  Make sure that we are broadcasting only to services that accept the given
  message. It seems to me that is not the case (anymore). Sending a
  non-supported message results in an UNKNOWN message otherwise.

* Broadcast to a specific service

  Right now, the broadcasting uses `"*"` or `"?"` or `"."` as a service name.
  The snaprfs wants to send a message to all the other snaprfs running on the
  cluster. To do so, we should be able to indicate "snaprfs" as the service
  name, only if we do that we'll have the message sent to one "snaprfs"
  (probably ourselves). The idea here would be to look into a way to (1) do
  a broadcast and (2) specify the service so we do not try to send it to
  all registered services instead limit the sending to remote services.

  Note: Up until now, this was less of a concern because only "snaprfs"
        should support message X, Y, Z and thus specifying the service was
        not absolutely necessary. However, it's difficult to make sure that
        all messages are unique to a daemon.

* Service Version & Failures ("Canary" or "Shadow" system)

  Look into a system where we record the version of a service (whenever the
  service registers it sends its version to communicatord) and if that version
  always fails, mark it as invalid and stop trying to connect to it on
  that server.

  i.e. that means we can create a canary of sort as a separate VPN machine
  on which we install all of our new services and detect whether they fail
  or not before starting to use them in production. This his hardly perfect,
  but could be really useful.

  Potential problems:
  1.  for cluckd, if setup as a forwarder (proxy), then it's not testing
      much of anything; for it to be a leader, we need that machine to
      be a fast machine (and if we install all of our systems, then it's
      also a pretty fat machine)
  2.  it could be sent "ghost" traffic to shadow the existing system in
      which case it does not need to be super fast (i.e. could be on my
      system at home) [this is client's traffic that we do not reply to,
      we just handle everything as a real production system and verify
      the reply in some way but do not do anything with the reply otherwise,
      to avoid slowness, we can drop traffic while a service is already
      considered too busy]

  Types of deployments:
  1.  canary--one machine runs new version, others not upgraded
      no need for any additional hardware/maintenance
  2.  blue-green--duplicate system with new version everywhere, once proven
      that it works, switch to this new system
      this is easy with a system like kubernetes, not as easy on a bare bone
      system
  3.  shadow--duplicate system with new version receiving real traffic but
      no reply to client; this can be setup to test the load by having a
      similar installation for the shadow as the main system
  4.  feature flags--hide new features behind flags; test by setting the
      flag to true, quickly stop the test by setting the flag back to false;
      like the canary, no additional hardware is required, but maintaining
      the flags can be really hard if not removed as soon as the new feature
      is proven to work

// vim: ts=4 sw=4 et

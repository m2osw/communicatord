
* Definition files should include the list of supported messages

  i.e. up until the time the service connects to the communicator we have no
       clue whether a message can make it to that service; by having the list
       of messages in the .service file, we could load that ahead of time and
       handle that check even before the service registers

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
       parameters of a message can be validated (two checks: (1) whether
       that paramter name is accepted for that command and (2) whether the
       parameter value is valid--using advgetopt::validator objects)

  This second one should probably be implemented in the eventdispatcher.

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
  in sync. is certain quite important.

  Note that the sitter is already setup to verify that some NTP system is
  up and running your your computer.

* Broadcast to a specific service

  Right now, the broadcasting uses `"*"` or `"?"` or `"."` as a service name.
  The snaprfs wants to send a message to all the other snaprfs running on the
  cluster. To do so, we should be able to indicate "snaprfs" as the service
  name, only if we do that we'll have the message sent to one "snaprfs"
  (probably ourself). The idea here would be to look into a way to (1) do
  a broadcast and (2) specific the service so we do not try to send it to
  all registered services.


// vim: ts=4 sw=4 et

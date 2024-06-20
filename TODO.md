
* Write Unit Tests

  Like cluckd, we want to create tests using the new eventdispathcer/reporter
  scripting system. Right now, I think many features in the communicator
  daemon are either redundant or incorrect. (i.e. many bugs even if it
  generally works.)

* Add help in the flag implementation so it is easier to administer

  This should be in the form of: (1) an `f_help` field allowing people
  to just follow those instructions to get things through; and (2) an
  `f_uri` field with a URL to a page on snapwebsites.org with complete
  documentation of the situation.

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
       parameters of a message can be validated:
         (1) is that parameter name is accepted by this command?
         (2) is the value valid for that parameter?
         (3) are required parameter present?
         (4) make sure message definitions do not reuse global parameters
             (i.e. cache=no, transmission_status=failure, etc.)

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

* "Address" parsing (it uses a URI now)

  I just switched the `secure_listen=...` to accept a `cds://...` URI.
  I think all our IPs should be supported in a similar way. This allows
  me to include the login name and password in the URI and not have
  additional command line parameters.

  It does look somewhat strange to include the scheme in such an IP address,
  but at the same time to connect to it you need that scheme. So it is not
  really that bad, I think.

  Note that in this case we could also consider having just one `listen=...`
  parameter and use spaces or commas to separate each URI. Then you could
  define any number of URIs with their proper scheme. However, I think I
  prefer to keep the separate variables since it makes it a lot easier to
  override the value between several .conf file.

* Broadcast only if message supported

  Make sure that we are broadcasting only to services that accept the given
  message. It seems to me that is not the case (anymore). Sending a
  non-supported message results in an UNKNOWN message otherwise.

* Broadcast to a specific service

  Right now, the broadcasting uses `"*"` or `"?"` or `"."` as a service name.
  The snaprfs wants to send a message to all the other snaprfs running on the
  cluster. To do so, we should be able to indicate "snaprfs" as the service
  name, only if we do that we'll have the message sent to one "snaprfs"
  (probably ourself). The idea here would be to look into a way to (1) do
  a broadcast and (2) specify the service so we do not try to send it to
  all registered services instead limit the sending to remote services.

  Note: Up until now, this was less of a concern because only "snaprfs"
        should support message X, Y, Z and thus specifying the service was
        not absolutely necessary. However, it's difficult to make sure that
        all messages are unique to a daemon.


// vim: ts=4 sw=4 et

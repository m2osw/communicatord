
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


// vim: ts=4 sw=4 et


SSL Repository
==============

This directory is used to create various types of key pairs.

This directory is created when you install the `communicatord` package.
It also automatically generates a key and certificate so the communicator
actually works as expected and securely.

You may remove the encryption between computers running `communicatord`
by editing the `/etc/communicator/communicator.d/35-communicatord.conf`
file and setting the `ssl_certificate` and `ssl_private_key` parameters
to empty (or comment them out).

    # Entries to prevent the communicator daemon from using SSL
    ssl_certificate=
    ssl_private_key=

Note: this has change slightly in the sense that we now have more available
      open ports. The new version of the communicator daemon listens for
      connections on a Unix socket, UDP socket, and three TCP sockets.
      The TCP sockets are local (127.0.0.1), private (on your private
      network such as 10.x.x.x or 192.168.x.x), or public (a public Internet
      IP address). The public connection is the one that makes use of SSL.
      --- TBD: could the private connections also use SSL and if so how do
      we distinguish them from the public connections in that realm? ---

_This file is part of the [snapcpp project](https://snapwebsites.org/)._


# Communicator Daemon Configuration

To change configuration parameters we strongly suggest that you use this
folder.

Here the files are named:

    ??-communicatord.conf

Where ?? are decimal digits (0 to 9). The order matters as in most cases
the parameters get overwritten by the last file loaded.

To find out the order in which the configuration files will be loaded,
run the `communicatord` command like so:

    $ communicatord --configuration-filenames
    Configuration filenames:
     . /etc/communicator/communicatord.conf
     . /etc/communicator/communicator.d/50-communicatord.conf

The number of files may vary depending on the version and which files are
already defined.


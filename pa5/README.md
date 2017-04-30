## CSCI3753 PA5

Build and test with:

    make
    ./pa5-encfs [optional flags] <passphrase> <mirror directory> <mountpoint>

For example, `./pa5-encfs -f 1234 ~/test/ mount/`, mounts `~/test` to `mount` with an encryption key of `1234`, and runs with FUSE in foreground mode, displaying debug statements.

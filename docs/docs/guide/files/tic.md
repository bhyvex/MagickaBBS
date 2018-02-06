# Tic File Processing

TODO.

## Example ticproc.sh

    #!/bin/sh
    /path/to/MagickaBBS/utils/ticproc/ticproc /path/to/MagickaBBS/utils/ticproc/ticproc.ini

## Example ticproc.ini

    [main]
    Ignore Password = true
    Ignore Case = false
    Inbound Directory = /path/to/MagickaBBS/ftn/in_sec
    Bad Files Directory = /path/to/MagickaBBS/ftn/tic_bad

    [SOME_AREA]
    Database = /path/to/MagickaBBS/somearea.sq3
    File Path = /path/to/MagickaBBS/files/somearea
    Password = SECRET
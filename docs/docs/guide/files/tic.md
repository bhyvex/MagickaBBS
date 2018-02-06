# Tic File Processing

To process tic files, you have a script that binkd calls when it receives a TIC file. The script simply starts the ticproc utility with an ini file that defines the settings and areas.

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

In the main section the options include:

**Ignore Password** Set this to true to ignore any passwords and import files regardless. Use this only if you are importing from the secure inbound directory.

**Ignore Case** Ignore the case used in the filename specified in the TIC file. This is useful for working with windows generated tic files which may send the file with a different case filename as what is listed in the tic file.

**Inbound Directory** The place to look for inbound tic files and their referenced files.

**Bad Files Directory** A directory to put any inbound files that collide with already existing files (and are not to be replaced).

After the main section, each area is defined into sections with the section name being the name of the file area.

**Database** The full path and name of the database to import the files into. They will be automatically approved.

**File Path** Where to put the actual files.

**Password** Password in the TIC file for this area.

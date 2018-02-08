# Local Message Bases

Local message bases are the easiest to set up, as they do not require any other utilities to work.

First you need a conference for your local bases, there is an example conference set up with to areas defined in the default install.

The conference which contains the areas is defined in it's own ini file which is linked to in the bbs.ini file.

Here the example for local bases:

    [main]
    Visible Sec Level = 10
    Networked = false
    Real Names = false

    [General]
    Read Sec Level = 10
    Write Sec Level = 10
    Path = /home/andrew/MagickaBBS/msgs/general
    Type = Local
    QWK Name = GENERAL

    [Testing]
    Read Sec Level = 10
    Write Sec Level = 10
    Path = /home/andrew/MagickaBBS/msgs/testing
    Type = Local
    QWK Name = TEST

Here you can see there is a main section which contains information about the conference itself, then sections that defines the areas.

## The Main Section

**Visible Sec Level** This is the security level required to view the conference. You should also ensure that the first defined area is able to be read by this security level, as it is the area that the user is dumped in when switching to the conference.

**Networked** Set this to false for local bases.

**Real Names** Set to true if you want to messages to be from a users "real name".

## The Area Sections

The section name is the name of the area.

**Read Sec Level** The security level required for the user to read the area.

**Write Sec Level** The security level required for the user to be able to post in the area.

**Path** The path to the JAM message base files, not including extension.

**QWK Name** This is a short name used to define an area in QWK/BlueWave offline mail. It must be unique. (This is presently just used for bluewave, as QWK is not yet implemented).


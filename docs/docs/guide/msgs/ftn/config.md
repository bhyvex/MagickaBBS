# Configuring FTN Message Bases

FTN Message base configuration is similar to Local bases, except they include an extra network section, and networked = true in the main section.

Here is an example included in the default install.

    [main]
    Visible Sec Level = 10
    Networked = true
    Real Names = false

    [network]
    type = fido
    fido node = 637:1/999
    domain = happynet

    [General]
    Read Sec Level = 10
    Write Sec Level = 10
    Path = /home/andrew/MagickaBBS/msgs/hnet_general
    Type = Echo 
    QWK Name = HN_GEN

    [Netmail]
    Read Sec Level = 10
    Write Sec Level = 10
    Path = /home/andrew/MagickaBBS/msgs/hnet_netmail
    Type = Netmail
    QWK Name = HN_NET

The network block contains the following:

**Type** This equals fido for fidonet style networks. It is currently the only option.

**Fido Node** This is your fidonet style network's node number (May or may not include a point.)

**Domain** This is the domain of the fidonet network, currently used to distinguish which node numbers belong to which network in the nodelists.sq3 database.

**Tagline** This is optional and can be used to set network specific taglines.

The area sections are identical to local mail, with the exception that Type should equal "Echo" or "Netmail" depending on the area type.

This is all for setting up networked bases in magicka, however you will still need to configure magimail and binkd.
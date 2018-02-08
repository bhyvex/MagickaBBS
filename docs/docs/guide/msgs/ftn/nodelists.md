# Nodelists and Magicka

To use nodelist functionality in Magicka, you need to import your nodelists into the **nodelists.sq3** database which lives in your main BBS directory.

You can import nodelists using the nodelistp.pl script in the utils folder.

For example:

    ./utils/nodelistp/nodelistp.pl nodelists/FSXNET.033 nodelists.sq3 fsxnet

The first argument is the nodelist you want to import, the second argument is the nodelists.sq3 database (and path if you're running it from another directory), the third argument is the domain you set up for the network in the conference INI file.

Importing two nodelists of the same domain will cause the first one to be wiped out and replaced by the second.
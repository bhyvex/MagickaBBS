# Security Levels (s10.ini)

This file defines the security level 10, you need one ini file for each security level you want to define, and it must be named sN.ini where N is the security level.

## Main

  * **Time Per Day** This is the number of minutes a user has each day.

To change a user's security level, use an sqlite3 utility to edit users.sq3. For example using the sqlite3 command line utility, enter the following to change the user "apam" to level 99

    update users set seclevel=99 where loginname LIKE "apam";

You will need to make sure there is an s99.ini for that user to be able to login.

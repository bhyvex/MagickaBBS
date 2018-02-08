# Managing Users

## Resetting a user's Password

If a user forgets their password, it can be reset to something else.

    ./utils/reset_pass/reset_pass users.sq3 username newpass

## Changing a user's Security Level

At present this must be done with an sqlite utility, for the sqlite3 command line utility:

    update users set seclevel=99 where loginname LIKE "apam";

Please ensure there is a respective sXX.ini where XX is the security level you are changing to.
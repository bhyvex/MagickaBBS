# LUA Scripting

The first thing you need to do if you want to use scripts, is set your script path in the bbs.ini file. This is set under the paths section and the name is "Script Path", see the example config.

Next, add scripts!

## Menu Scripts

## Login/Logout Scripts

## Utility Scripts

## BBS Script Commands

**bbs_write_string** Takes one string, writes that string to the user. Returns nothing.

**bbs_read_string** Takes one number (length), returns a string up to the length specified

**bbs_display_ansi** Takes one string, displays that ansi from the ansis folder (don't include path or extension). Returns nothing.

**bbs_read_char** Reads a character from the user, returns a string with one character in it.

**bbs_version** Returns a string with the BBS version in it.

**bbs_node** Returns a number with the node the user is on.

**bbs_read_last10** takes a number which is the offset (0-9) and returns user (string), location(string) unix time(number)

**bbs_get_emailcount** Returns the number of emails a user has.

**bbs_mail_scan** Performs a mail scan.

**bbs_run_door** takes two arguments, the command as a string and if it requires stdio redirection as a boolean, then runs the door.

**bbs_time_left** Returns a number with the minutes a user has remaining.

**bbs_cur_mailarea_info** Returns 4 values, The conference number, conference name, area number and area name.

**bbs_cur_filearea_info** Returns 4 values, The directory number, directory name, sub number, sub name

**bbs_display_automsg** Displays the current automessage

**bbs_get_info** Returns 4 strings, the BBS name, the Sysop name, the OS name and the system architecture.
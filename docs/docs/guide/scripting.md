# LUA Scripting

The first thing you need to do if you want to use scripts, is set your script path in the bbs.ini file. This is set under the paths section and the name is "Script Path", see the example config.

Next, add scripts!

## Menu Scripts

Each menu can have a script function that runs to display the menu, read the hotkey and return the key to the BBS.

This is the script for the mail menu.

    function menu()
	    -- clear the screen
	    bbs_write_string("\027[2J");

        -- display menu ansi
	    bbs_display_ansi("mailmenu");


	    -- display the current mail conference and area
	    local dir_no;
	    local dir_name;
	    local sub_no;
	    local sub_name;
	
	    dir_no, dir_name, sub_no, sub_name = bbs_cur_mailarea_info();
	    bbs_write_string(string.format("\r\n\027[0m   \027[0;36mConference: \027[1;34m(\027[1;37m%d\027[1;34m) \027[1;37m%-20s\027[0;36mArea: \027[1;34m(\027[1;37m%d\027[1;34m) \027[1;37m%-20s\r\n", dir_no, dir_name, sub_no, sub_name));

        -- display the prompt with the time left
	    bbs_write_string(string.format("\r\n\027[1;34m   [\027[0;36mTime Left\027[1;37m %dm\027[34m]-> \027[0m", bbs_time_left()));
	
	    -- read char entered
	    cmd = bbs_read_char();

        -- return the char entered
        return cmd;
    end


## Login/Logout Scripts

The login and logout functions can also be scripted. 

The `login_stanza.lua` script does everything from the point the user logs in until the main menu is displayed.

The `logout_stanza.lua` script runs a logout function that simply displays a good bye ansi and returns 1.

## Utility Scripts

Utility scripts are just scripts that can be called from a menu. They can do anything you like, an example would be a oneliners script.

To run a script from a menu, place the script in the scripts directory, and call it from a menu like this

    HOTKEY O
    COMMAND DOSCRIPT
    DATA oneliners

Where data is the name of the lua script to call without the extension.

## BBS Script Commands

**bbs_write_string** Takes one string, writes that string to the user. Returns nothing.

**bbs_read_string** Takes one number (length), returns a string up to the length specified

**bbs_display_ansi** Takes one string, displays that ansi from the ansis folder (don't include path or extension). Returns nothing.

**bbs_display_ansi_pause** Takes one string, displays that ansi (with a pause prompt) from the ansis folder (don't include path or extension). Returns nothing.

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

**bbs_file_scan** Performs a file scan.

**bbs_full_mail_scan** Performs a full mail scan (like a mail scan but also displays new messages)

**bbs_get_userhandle** Returns the logged in user's handle

**bbs_message_found** Takes three numbers, the conference, area and the message number, returns one number, 1 if found, 0 if not.

**bbs_read_message_hdr** Takes three numbers, the conference, area and the message number. Returns three strings, the Sender, Recipient and Subject.

**bbs_read_message** Takes three numbers, the conference, area and the message number. Returns one string, the message body.

**bbs_temp_path** Returns a temporary path for the script to store data in. This path is cleared out on login.

**bbs_post_message** Takes 2 numbers and 4 strings, The conference number, the area number, the recipient, the sender, the subject and the body.

**bbs_data_path** Returns a path for script data storage. This is shared with all scripts, so make sure your filenames are unique.

**bbs_user_security** Returns the current user's security level.

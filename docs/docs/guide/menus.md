# Menuing System

Menus in magicka are defined in menu files. Menu files consist of a header block, then blocks of instructions for menu items.

An example menu looks like this:

    LUASCRIPT examplemenu
    ANSIFILE examplemenu
    CLEARSCREEN

    HOTKEY A
    COMMAND SUBMENU
    DATA anothermenu
    SECLEVEL 10

    HOTKEY M
    COMMAND PREVMENU

Here you see the 3 lines in the header, and 2 menu items. 

    LUASCRIPT examplemenu

This sets the script to run for the menu as "examplemenu.lua" it makes the other 2 redundant as the script is meant to take care of displaying the ANSI file and clearing the screen.

If you are not using a LUA script for your menu, you should set ANSIFILE to point to an ANSI file in the ansis folder, and optionally CLEARSCREEN to clear the screen.

Menu Item blocks should always start with a HOTKEY command, which sets the key used to trigger the menu item. Note that the HOTKEY is not case sensitive.

After the HOTKEY command, a COMMAND command should follow, this is the COMMAND the menu item does, for example it could lead to a SUBMENU, or run a door, or one of many things.

Next is a DATA command, this specifies the DATA associated with the COMMAND command, not all COMMANDs require DATA.

Finally, an optional SECLEVEL command indicates the required security level for a user to be able to trigger a command.

## List of COMMANDs

**SUBMENU** loads a new menu. DATA is the name of the submenu

**PREVMENU** returns to the menu that loaded the submenu.

**LOGOFF** log off the system.

**AUTOMESSAGE_WRITE** Enter a new automessage.

**TEXTFILES** Display the text file collection.

**CHATSYSTEM** Load the multinode/interbbs chat system.

**BBSLIST** Load the BBS listings.

**LISTUSERS** List the users of the BBS

**BULLETINS** Display the bulletins.

**LAST10CALLERS** Display the last 10 callers.

**SETTINGS** Load the settings interface.

**RUNDOOR** Run a door, DATA is the name of the door in the doors.ini

**MAILSCAN** Scan for new mail.

**READMAIL** Read the messages in the current conference/area.

**POSTMESSAGE** Post a message in the current conference/area.

**CHOOSEMAILCONF** Change the current message conference.

**CHOOSEMAILAREA** Change the current mail area.

**SENDEMAIL** Send a private email to a user.

**LISTEMAIL** List the users private emails.

**NEXTMAILCONF** Changes mail conference to the next one.

**PREVMAILCONF** Changes mail conference to the previous one.

**NEXTMAILAREA** Changes mail area to the next one.

**PREVMAILAREA** Changes mail area to the previous one.

**BLUEWAVEDOWNLOAD** Download a bluewave packet.

**BLUEWAVEUPLOAD** Upload a bluewave packet.

**CHOOSEFILEDIR** Change the current file directory.

**CHOOSEFILESUB** Change the current file subdirectory.

**LISTFILES** List files in the current subdirectory.

**UPLOAD** Upload a file to the current subdirectory.

**DOWNLOAD** Download files that have been tagged.

**CLEARTAGGED** Clear tagged file list.

**NEXTFILEDIR** Changes file directory to the next one.

**PREVFILEDIR** Changes file directory to the previous one.

**NEXTFILESUB** Changes file subdirectory to the next one.

**PREVFILESUB** Changes file subdirectory to the previous one.

**LISTMESSAGES** List messages in the current area.

**DOSCRIPT** Run a script DATA is the name of the script to run.

**SENDNODEMESSAGE** Send a node message to another user online.

**SUBUNSUBCONF** Subscribe/Unsubscribe from areas in the conference.

**RESETMSGPTRS** Reset message pointers in the current area.

**RESETALLMSGPTRS** Reset message pointers in all areas.

**FILESCAN** Scan for new files.

**FULLMAILSCAN** Scan for new messages and display them.

**FILESEARCH** Search for a file.

**DISPLAYTXTFILE** Display a text / ansi file DATA is the name of the file to display.

**DISPLAYTXTPAUSE** Display a text / ansi file with pause prompt. DATA is the name of the file to display.

**GENWWWURLS** Generate and show WWW urls for the tagged files for web download.


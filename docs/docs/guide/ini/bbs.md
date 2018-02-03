# bbs.ini

This is the main bbs INI file and contains the following sections

## Main
 * **Codepage** The default codepage to use when a user logs in. (Required)
 * **Telnet Port** The port to listen to telnet connections on (Required)
 * **BBS Name** The name of your BBS (Required)
 * **Sysop Name** The Sysop's Login Name (Required)
 * **Nodes** The maximum number of nodes (Required)
 * **New User Level** The security level new users are given (See s10.ini) (Required)
 * **MagiChat Server** The MagiChat Server you want to connect to (Optional)
 * **MagiChat Port** The port of the above MagiChat Server (Required if MagiChat Server is set);
 * **MagiChat BBSTag** The (short) tag to identify users of your BBS (Required if MagiChat Server is set);
 * **Default Tagline** The tagline to use if a conference doesn't have it's own set
 * **External Editor Cmd** The script to launch for running an external editor (NOT Required - remove if you dont have one)
 * **External Editor Stdio** True or False if your editor requires stdio redirection (Only Required if External Editor CMD is set)
 * **External Editor Codepage** The codepage the external editor uses (CP437 for magichat) (Only required if External Editor CMD is set)
 * **Automessage Write Level** The security level a user needs to change the automessage (Required)
 * **Fork** True if you want the BBS to run in daemon mode false if not. (Required)
 * **Enable WWW** True to enable the WWW server, false if not. (Required)
 * **WWW Port** Port to listen for HTTP connections (Required if WWW is enabled)
 * **WWW URL** Public facing url of the BBS (Required if WWW is enabled)
 * **Enable SSH** True to enable the SSH server, false to not. (Required)
 * **SSH Port** Port to listen for SSH connection. (Required if Enable SSH is true)
 * **SSH DSA Key** Path to SSH DSA Host Key. (Required if Enable SSH is true)
 * **SSH RSA Key** Path to SSH RSA Host Key. (Required if Enable SSH is true)
 * **QWK Name** Name used for the system for Bluewave (Required - restricted to 8 characters)
 * **Main AKA** Your main network address (Required)
 * **QWK Max Messages** Maximum number of messages per Bluewave bundle (Required)
 * **Broadcast Enable** Set to true to enable the Broadcast Feature (Required)
 * **Broadcast Port** The port to send broadcast messages on (Required)
 * **Broadcast Address** The address to broadcast on (Required)
 * **IP Guard Enable** Set to true to enable the IP Guard (Required)
 * **IP Guard Timeout** Timeout between connections (Required)
 * **IP Guard Tries** Number of connections allowed within IP Guard Timeout (Required)
 * **Root Menu** The menu file for the "main menu" (Required)
 * **Date Style** The style for dates (EU or US) (Required)
 * **Enable IPv6** Enable listening on IPv6 as well as IPv4 (Required)

## Paths
 * **Config Path** Path to your config files (Required)
 * **WWW Path** Path to webserver files (Required)
 * **String File** Path and filename of the 'magicka.strings' file (Required)
 * **ANSI Path** Path to your system ansi files (Required)
 * **BBS Path** Path to your main BBS directory (Required)
 * **Script Path** Path to user configurable lua scripts (Required)
 * **Echomail Semaphore** Path to semaphore to create when a new echomail is written. (Required)
 * **Netmail Semaphore** Path to semaphore to create when a new netmail is written. (Required)
 * **PID File** Path of pid file (Required)
 * **Log Path** Path to log files (Required)
 * **Menu Path** Path to menu files (Required)

## Mail Conferences
 * Mail Conferences include a list of Name = Path to configuration INI (At least 1 is required)

## File Directories
 * Same as Mail Conferences, but for files
 
## Text Files
 * A list of text files with name = Path/Filename for text files in the text file collection

# protocols.ini

This file is where you configure your protocols, each section deals with a specific protocol, *f for file name. 

* **Internal Zmodem** True if this section is the internal zmodem (No other options required if true)
* **Upload Prompt** True if protocol requires the user to enter the filename, eg. XModem (Required)
* **Upload Command** The command to execute to receive a file. (Required)
* **Download Command** The command to execute to send a file. (Required)
* **stdio** True if using stdio redirection when running the protocol. (Required)


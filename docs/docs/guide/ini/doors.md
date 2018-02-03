# doors.ini

Door configurations are stored in sections, with the section name being the name of the door (or anything really as long as it's unique per door).

* **Command** This is the command line to run the door, it is passed the node number as the first argument and the socket as the second argument.
* **Stdio** Either true or false depending on whether stdio redirection is needed.
* **Codepage** Codepage the door uses. (Not required)

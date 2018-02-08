# archivers.ini

This file is where you configure your archivers, each section deals with a specific archiver, use *a for archive name *f for file list to pack and *d for destination directory. Users default archive type (the one used for offline mail) will default to the first defined archiver. Users can change archivers via the settings menu.

* **Extension** The file extension used to represent this archive type
* **Pack** The command to pack (for example, for info zip: zip -j *a *f)
* **Unpack** The command to unpack (for example, for info zip: unzip -j -o *a -d *d)

# File Area Setup

File directories and subdirectories are similar to message conferences and areas to set up in that they have a main section followed by sections describing each subdirectory.

## Example Config

    [main]
    Visible Sec Level = 10

    [Area One]
    Database = area_one
    Download Sec Level = 10
    Upload Sec Level = 99
    Upload Path = /path/to/MagickaBBS/files/area_one

    [Area Two]
    Database = area_two
    Download Sec Level = 10
    Upload Sec Level = 99
    Upload Path = /path/to/MagickaBBS/files/area_two

In the main section there is just one variable that applies to the whole directory, **Visible Sec Level** is the security level required to view the directory.

The section name for the sub-directory is the name of the sub directory.

**Database** is the name of the database that stores the information of the files, it is stored in the main BBS directory. No extension is required.

**Download Sec Level** This is the security level required to download files from the subdirectory.

**Upload Sec Level** This is the security level required to upload files to the subdirectory.

**Upload Path** This is the directory in which uploads are stored for the subdirectory, make sure it's writable for the BBS.

## Uploading Files

When a user (or the sysop) uploads a file, they are not *approved* by default, so they will not be shown in the listings. To approve uploaded files, the sysop should run the fileapprove utility on the database.

    ./utils/fileapprove/fileapprove -d database.sq3

This will list all the files in the database, and allow the sysop to toggle the approved status, or remove the file entirely.

## Mass Uploading

If you have a directory full of files you want to upload, you can use the massupload perl script. This script will upload and approve all files in a folder, and if they are ZIP files check for file_id.diz files and import them into the description.

    ./utils/massupload/massupload.pl /path/to/MagickaBBS/files/area_one area_one.sq3

Note that you must use the FULL path of the directory the files are in.

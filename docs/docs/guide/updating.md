# Updating Magicka BBS

## Step 1. Backup and Shutdown

Firstly, you should always backup your BBS, just incase.

   cp -r MagickaBBS MagickaBBS_Backup

Next you should shut down your BBS.

## Step 2. Pull and Build

Assuming you've installed Magicka BBS from via git rather than use a ZIP file or Tarball, the upgrade process *should* be rather painless.

    git pull
    git checkout branch

    make cleanwww
    make www

Where branch is the branch you're updating to, for example v0.9-alpha.

## Step 3. Update Scripts

Next, run any update perl scripts:

    ls utils/sql_update/

This will list any update scripts, you'll only need to run them if they've changed since your last pull.

**files_sql_update.pl** should be run on any file areas databases.

**users_sql_update.pl** should be run on your users database.

## Step 4. Update Strings

If you keep a customized magicka.strings file, then you will need to make any changes listed in STRINGS.CHANGES, if your strings file is just a link to the one in the dist folder, it is automatically updated so you can skip this step.

## Step 5. Bring the BBS Back up

At this point you can safely bring your BBS back up.

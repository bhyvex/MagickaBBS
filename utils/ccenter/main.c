#include <curses.h>
#include <cdk.h>
#include "ccenter.h"

CDKSCREEN *cdkscreen = 0;

void displayLabel() {
    CDKLABEL *label;
    char *message[] ={
"                                      ,$.",
"                                    `S$$$S'",
"                                     .$²$.         $$",
"  `Sn.                               '   `         $$",
"     $$s,sSSsSSSs ,sSSSs.  ,4S$$Ss $$ $$$ ,sS$$Ss, $$  $$ ,sSSSs.",
"      $$$² $$² $$ `'   $$  $$'  `$$.  $$' $$'  `4S $$ $$  `'   $$",
"       $$  )$  $$ ,$$$$$$  $$    .$$  $$  $$       $$$$   ,$$$$$$",
"       $$  $$  $$ $$   $$  `$$$$$$$²  $$, $$   ,sS $$ $$  $$   $$",
"      $$'      $$ `$$$$`SS       $$,  $$$ `4S$$SS' $$  $$ `$$$$`SS",
"               `$$.       ,sS$$$s`$$                    $$.        .,",
"                 `$Ss     $$      $$  sS    sS    sS$s   `$$,    ,$$",
"                          `4$$$$$$$'  $$$$. $$$$. $$,      ²S$$$$S'",
"                                      $$ $$ $$ $$  `$$",
"                                      $$$$' $$$$' 4$$'"};

    label = newCDKLabel(cdkscreen, COLS / 2 - 35, LINES / 2 - 7, message, 14, FALSE, FALSE);
    drawCDKLabel(label, FALSE);
}

void displayAbout() {
    CDKDIALOG *aboutWindow;
    char *message[] = {"<C>Control Centre for MagickaBBS",
                        "<C>Copyright (c) 2017 Andrew Pamment"};
    char *buttons[] = {"OK"};
    aboutWindow = newCDKDialog(cdkscreen, 10, 5, message, 2, buttons, 1, 0, TRUE, TRUE, FALSE);

    activateCDKDialog(aboutWindow, 0);

    destroyCDKDialog(aboutWindow);
}

int main(int argc, char **argv) {
    WINDOW *cursesWin = 0;
    CDK_PARAMS params;
    CDKMENU *mainMenu;

    int selection;
    int doexit = 0;
    char *menuItems[MAX_MENU_ITEMS][MAX_SUB_ITEMS];
    int menuLength[] = {7, 2};
    menuItems[0][0] = strdup("Main");
    menuItems[0][1] = strdup("System Config");
    menuItems[0][2] = strdup("System Paths");
    menuItems[0][3] = strdup("Mail Conferences");
    menuItems[0][4] = strdup("File Directories");
    menuItems[0][5] = strdup("Text Files");
    menuItems[0][6] = strdup("Exit");
    menuItems[1][0] = strdup("Misc");
    menuItems[1][1] = strdup("About");
    int locations[2] = {LEFT, RIGHT};
	CDKparseParams(argc, argv, &params, "c:" CDK_CLI_PARAMS);
    if (!load_ini_file(CDKparamString (&params, 'c'))) {
        fprintf(stderr, "Error opening ini file: %s\n", CDKparamString (&params, 'c'));
        exit (-1);
    }
    cursesWin = initscr();
  
    cdkscreen = initCDKScreen(cursesWin);
    initCDKColor();

    displayLabel();

    mainMenu = newCDKMenu(cdkscreen, menuItems, 2, menuLength, locations, TOP, A_NORMAL, A_NORMAL);
    setCDKMenuTitleHighlight(mainMenu, A_STANDOUT);
    setCDKMenuSubTitleHighlight(mainMenu, A_STANDOUT);

    while(!doexit) {
        selection = activateCDKMenu(mainMenu, 0);
        if (mainMenu->exitType == vESCAPE_HIT) {
            doexit = 1;
        } else if (mainMenu->exitType == vNORMAL) {
            switch(selection / 100) {
                case 0:
                    switch (selection % 100) {
                        case 0:
                            system_config();
                            break;
                        case 1:
                            system_paths();
                            break;
                        case 2:
                            mail_conferences();
                            break;
                        case 3:
                            file_directories();
                            break;
                        case 5:
                            doexit = 1;
                            break;
                    }
                    break;
                case 1:
                    switch (selection % 100) {
                        case 0:
                            displayAbout();
                            break;
                    }
                    break;                    
            }
        }
    }

    destroyCDKScreen(cdkscreen);
    endCDK();
    return 0;
}
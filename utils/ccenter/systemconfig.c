#include <curses.h>
#include <cdk.h>
#include "ccenter.h"

extern struct bbs_config conf;
extern CDKSCREEN *cdkscreen;

void bbs_name(void) {
    char *entrytext;
    CDKENTRY *bbsNameEntry = newCDKEntry(cdkscreen, 5, 5, "</B/32>BBS Name<!32>", NULL, A_NORMAL, ' ', vMIXED, 32, 1, 32, TRUE, FALSE);
    
    setCDKEntry(bbsNameEntry, conf.bbs_name, 1, 32, TRUE);

    entrytext = activateCDKEntry(bbsNameEntry, 0);

    if (bbsNameEntry->exitType == vNORMAL) {
        if (conf.bbs_name != NULL) {
            free(conf.bbs_name);
        }
        conf.bbs_name = strdup(entrytext);
    }
    destroyCDKEntry(bbsNameEntry);
}

void sysop_name(void) {
    char *entrytext;
    CDKENTRY *sysopNameEntry = newCDKEntry(cdkscreen, 5, 5, "</B/32>Sysop Name<!32>", NULL, A_NORMAL, ' ', vMIXED, 32, 1, 32, TRUE, FALSE);
    
    setCDKEntry(sysopNameEntry, conf.sysop_name, 1, 32, TRUE);

    entrytext = activateCDKEntry(sysopNameEntry, 0);

    if (sysopNameEntry->exitType == vNORMAL) {
        if (conf.sysop_name != NULL) {
            free(conf.sysop_name);
        }
        conf.sysop_name = strdup(entrytext);
    }
    destroyCDKEntry(sysopNameEntry);
}

void system_config(void) {

    char *itemList[] = {"BBS Name", 
                        "Sysop Name", 
                        "Number of Nodes", 
                        "MagiChat",
                        "Message Area Options",
                        "Auto Message Write Level",
                        "Servers and Ports"
                        };
    int selection;
    CDKSCROLL *sysConfigItemList = newCDKScroll(cdkscreen, 3, 3, RIGHT, 10, 30, "</B/32>System Config<!32>", itemList, 7,FALSE, A_STANDOUT, TRUE, FALSE);

    while (1) {
        selection = activateCDKScroll(sysConfigItemList, 0);
        if (sysConfigItemList->exitType == vESCAPE_HIT) {
            break;
        }
        switch (selection) {
            case 0:
                bbs_name();
                break;
            case 1:
                sysop_name();
                break;
            default:
                break;
        }
    }

    destroyCDKScroll(sysConfigItemList);
}
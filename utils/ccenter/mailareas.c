#include <curses.h>
#include <cdk.h>
#include "ccenter.h"

extern struct bbs_config conf;
extern CDKSCREEN *cdkscreen;

void edit_mail_areas(int confer) {
    char **itemlist;
    int selection;
    int i;

    while (1) {
        itemlist = (char **)malloc(sizeof(char *) * (conf.mail_conferences[confer]->mail_area_count + 1));

        itemlist[0] = strdup("Add Area");

        for (i=0;i<conf.mail_conferences[confer]->mail_area_count;i++) {
            itemlist[i+1] = strdup(conf.mail_conferences[confer]->mail_areas[i]->name);
        }

        CDKSCROLL *mailAreaList = newCDKScroll(cdkscreen, 7, 7, RIGHT, 12, 30, "</B/32>Mail Areas<!32>", itemlist, conf.mail_conferences[confer]->mail_area_count + 1, FALSE, A_STANDOUT, TRUE, FALSE);


        selection = activateCDKScroll(mailAreaList, 0);
        if (mailAreaList->exitType == vESCAPE_HIT) {
            destroyCDKScroll(mailAreaList);
            for (i=0;i<conf.mail_conferences[confer]->mail_area_count+1;i++) {
                free(itemlist[i]);
            }
            free(itemlist);
            break;
        }
        if (selection == 0) {
            // add new
        } else {
            // edit existing
            //edit_mail_conference(selection - 1);
        }
        destroyCDKScroll(mailAreaList);
        for (i=0;i<conf.mail_conferences[confer]->mail_area_count+1;i++) {
            free(itemlist[i]);
        }
        free(itemlist);
    }
}
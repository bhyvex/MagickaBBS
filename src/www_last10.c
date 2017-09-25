#if defined(ENABLE_WWW)

#include <string.h>
#include <time.h>
#include <stdlib.h>
#include "bbs.h"


extern struct bbs_config conf;

char *www_last10() {
	char *page;
	int max_len;
	int len;
	char buffer[4096];
	struct last10_callers callers[10];

	int i,z;
	struct tm l10_time;
	FILE *fptr = fopen("last10.dat", "rb");

	if (fptr != NULL) {

		for (i=0;i<10;i++) {
			if (fread(&callers[i], sizeof(struct last10_callers), 1, fptr) < 1) {
				break;
			}
		}

		fclose(fptr);
	} else {
		i = 0;
	}

	page = (char *)malloc(4096);
	max_len = 4096;
	len = 0;
	memset(page, 0, 4096);
	
	sprintf(buffer, "<div class=\"content-header\"><h2>Last 10 Callers</h2></div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);

	sprintf(buffer, "<div class=\"div-table\">\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
	for (z=0;z<i;z++) {
		localtime_r(&callers[z].time, &l10_time);
		sprintf(buffer, "<div class=\"last10-row\"><div class=\"last10-name\">%s</div><div class=\"last10-location\">%s</div><div class=\"last10-date\">%.2d:%.2d %.2d-%.2d-%.2d</div></div>\n", callers[z].name, callers[z].location, l10_time.tm_hour, l10_time.tm_min, l10_time.tm_mday, l10_time.tm_mon + 1, l10_time.tm_year - 100);
		if (len + strlen(buffer) > max_len - 1) {
			max_len += 4096;
			page = (char *)realloc(page, max_len);
		}
		strcat(page, buffer);
		len += strlen(buffer);
	}
	
	sprintf(buffer, "</div>\n");
	if (len + strlen(buffer) > max_len - 1) {
		max_len += 4096;
		page = (char *)realloc(page, max_len);
	}
	strcat(page, buffer);
	len += strlen(buffer);
	
	return page;
}

#endif

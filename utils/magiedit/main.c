#if WIN32
#   define _MSC_VER 1
#	define _CRT_SECURE_NO_WARNINGS
#   include <Windows.h>
#else
#
#endif

#include <OpenDoor.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

char **quote_lines;
int quote_line_count;

char *msgfrom;
char *msgto;
char *msgsubj;
char *msgarea;
int msgpriv;

char *message_editor() {
    char **body_lines;
    int body_line_count;
    int position_x;
    int position_y;
    int done;
    char line[81];
    char line_cpy[81];
    int top_of_screen = 0;
    int i, j;
    char *return_body;
    int body_len;
    tODInputEvent ch;
    int q_done;
    char q_marker;
    int q_start;
    int q_position;
    int *q_lines;
    int q_line_count;
    int q_unquote;
    int z;
    int redraw;
    int old_top_of_screen = 0;

    position_x = 0;
    position_y = 0;

    body_line_count = 0;
    done = 0;

    memset(line, 0, 81);
    memset(line_cpy, 0, 81);

    while (!done) {
        od_clr_scr();
        od_set_cursor(1, 1);
        od_set_color(L_WHITE, D_BLUE);
        od_printf("  TO: %-32.32s AREA: %-32.32s", msgto, msgarea);
        od_clr_line();
        od_set_cursor(2, 1);
        od_printf("SUBJ: %s", msgsubj);
        od_clr_line();
        od_set_cursor(24, 1);
        od_printf("Enter /S to save, /Q to quote or /A to abort on a new line.");
        od_clr_line();
        od_set_cursor(3, 1);
        od_set_color(L_WHITE, D_BLACK);
        while (1) {
            od_get_input(&ch, OD_NO_TIMEOUT, GETIN_NORMAL);
            if (ch.EventType == EVENT_EXTENDED_KEY) {
                if (ch.chKeyPress == OD_KEY_UP) {
                    if (position_y > 0) {
                        strcpy(line_cpy, body_lines[position_y - 1]);
                        free(body_lines[position_y - 1]);
                        body_lines[position_y - 1] = strdup(line);
                        strcpy(line, line_cpy);
                        position_y--;

                        if (position_x >= strlen(line)) {
                            position_x = strlen(line);
                        }

                        if (position_y < top_of_screen) {
                            top_of_screen--;

                        }
                        if (old_top_of_screen != top_of_screen) {
                            od_set_cursor(3, 1);

                            for (i=top_of_screen;i<position_y;i++) {
                                od_set_cursor(i - top_of_screen + 3, 1);
                                od_printf("%s", body_lines[i]);
                                od_clr_line();
                            }
                            od_set_cursor(i - top_of_screen + 3, 1);
                            od_printf("%s", line);
                            od_clr_line();
                            for (i=position_y;i<body_line_count && i < top_of_screen + 20;i++) {
                                od_set_cursor(i - top_of_screen + 4, 1);
                                od_printf("%s", body_lines[i]);
                                od_clr_line();
                            }

                        }
                        old_top_of_screen = top_of_screen;

                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    }
                } else if (ch.chKeyPress == OD_KEY_DOWN) {
                    if (position_y < body_line_count) {
                        strcpy(line_cpy, body_lines[position_y]);
                        free(body_lines[position_y]);
                        body_lines[position_y] = strdup(line);
                        strcpy(line, line_cpy);
                        position_y++;

                        if (position_x >= strlen(line)) {
                            position_x = strlen(line);
                        }

                        if (position_y > top_of_screen + 20) {
                            top_of_screen++;

                        }
                        if (old_top_of_screen != top_of_screen) {
                            od_set_cursor(3, 1);

                            for (i=top_of_screen;i<position_y;i++) {
                                od_set_cursor(i - top_of_screen + 3, 1);
                                od_printf("%s", body_lines[i]);
                                od_clr_line();
                            }
                            od_set_cursor(i - top_of_screen + 3, 1);
                            od_printf("%s", line);
                            od_clr_line();
                            for (i=position_y;i<body_line_count && i < top_of_screen + 20;i++) {
                                od_set_cursor(i - top_of_screen + 4, 1);
                                od_printf("%s", body_lines[i]);
                                od_clr_line();
                            }
                        }
                        old_top_of_screen = top_of_screen;
                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    }
                } else if (ch.chKeyPress == OD_KEY_LEFT) {
                    if (position_x > 0) {
                        position_x--;
                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    }

                } else if (ch.chKeyPress == OD_KEY_RIGHT) {
                    if (position_x < strlen(line)) {
                        position_x++;
                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    }
                }
            } else if (ch.EventType == EVENT_CHARACTER) {
                if (ch.chKeyPress == '\r' || strlen(line) >= 73) {
                    if (strcasecmp(line, "/S") == 0) {
                        // save message
                        body_len = 0;
                        for (i=0;i<body_line_count;i++) {
                                body_len = body_len + strlen(body_lines[i]) + 2;
                        }
                        body_len++;

                        return_body = (char *)malloc(body_len);
                        memset(return_body, 0, body_len);

                        for (i=0;i<body_line_count;i++) {
                                strcat(return_body, body_lines[i]);
                                strcat(return_body, "\r\n");
                        }
                        if (body_line_count > 0) {
                            for (i=0;i<body_line_count;i++) {
                                free(body_lines[i]);
                            }
                            free(body_lines);
                        }

                        return return_body;
                    } else if (strcasecmp(line, "/A") == 0) {
                        // abort message
                        if (body_line_count > 0) {
                            for (i=0;i<body_line_count;i++) {
                                free(body_lines[i]);
                            }
                            free(body_lines);
                        }
                        return NULL;
                    } else if (strcasecmp(line, "/Q") == 0) {
                        // quote
                        if (quote_line_count > 0) {
                            redraw = 1;
                            od_clr_scr();
                            od_set_cursor(1, 1);
                            od_set_color(L_WHITE, D_BLUE);
                            od_printf("Quoting Message --");
                            od_clr_line();
                            od_set_cursor(2, 1);
                            od_printf("Press Enter to Select a line, Q to Quote, A to Abort");
                            od_clr_line();
                            od_set_cursor(3, 1);
                            od_set_color(L_WHITE, D_BLACK);

                            q_start = 0;
                            q_position = 0;
                            q_line_count = 0;
                            q_done = 0;
                            q_marker = ' ';
                            while (!q_done) {
                                if (redraw) {
                                    for (i=q_start;i<q_start + 21 && i < quote_line_count;i++) {
                                        q_marker = ' ';
                                        for (j=0;j<q_line_count;j++) {
                                            if (q_lines[j] == i) {
                                                q_marker = '+';
                                                break;
                                            }
                                        }
                                        od_set_cursor(i - q_start + 3, 1);
                                        if (i == q_position) {
                                           od_set_color(D_BLACK, D_GREEN);
                                           od_printf("`bright yellow`%c`black green`%s", q_marker, quote_lines[i]);
                                           od_clr_line();
                                        } else {
                                           od_set_color(L_WHITE, D_BLACK);
                                           od_printf("`bright yellow`%c`bright white`%s", q_marker, quote_lines[i]);
                                           od_clr_line();
                                        }
                                    }
                                    redraw = 0;
                                }
                                od_get_input(&ch, OD_NO_TIMEOUT, GETIN_NORMAL);
                                if (ch.EventType == EVENT_EXTENDED_KEY) {
                                    if (ch.chKeyPress == OD_KEY_UP) {
                                        q_position--;
                                        if (q_position < 0) {
                                            q_position = 0;
                                        }
                                        if (q_position < q_start) {
                                            q_start = q_start - 21;
                                            if (q_start < 0) {
                                                q_start = 0;
                                            }
                                            redraw = 1;
                                        }
                                        
                                        if (!redraw) {
                                            q_marker = ' ';
                                            for (j=0;j<q_line_count;j++) {
                                                if (q_lines[j] == q_position) {
                                                    q_marker = '+';
                                                    break;
                                                }
                                            }
                                            od_set_cursor(q_position - q_start + 3, 1);
                                            od_set_color(D_BLACK, D_GREEN);
                                            od_printf("`bright yellow`%c`black green`%s", q_marker, quote_lines[q_position]);
                                            od_clr_line();
                                            q_marker = ' ';
                                            for (j=0;j<q_line_count;j++) {
                                                if (q_lines[j] == q_position + 1) {
                                                    q_marker = '+';
                                                    break;
                                                }
                                            }
                                            
                                            od_set_cursor(q_position + 1 - q_start + 3, 1);
                                            od_set_color(D_BLACK, D_GREEN);
                                            od_printf("`bright yellow`%c`bright white`%s", q_marker, quote_lines[q_position + 1]);
                                            od_clr_line();                                            
                                        }
                                    } else if (ch.chKeyPress == OD_KEY_DOWN) {
                                        q_position++;
                                        if (q_position >= quote_line_count) {
                                            q_position = quote_line_count - 1;
                                        }

                                        if (q_position >= q_start + 21) {
                                            q_start = q_start + 21;
                                            if (q_start + 21 >= quote_line_count) {
                                                q_start = quote_line_count - 21;
                                            }
                                            redraw = 1;
                                        }
                                        if (!redraw) {
                                            q_marker = ' ';
                                            for (j=0;j<q_line_count;j++) {
                                                if (q_lines[j] == q_position) {
                                                    q_marker = '+';
                                                    break;
                                                }
                                            }
                                            od_set_cursor(q_position - q_start + 3, 1);
                                            od_set_color(D_BLACK, D_GREEN);
                                            od_printf("`bright yellow`%c`black green`%s", q_marker, quote_lines[q_position]);
                                            od_clr_line();
                                            q_marker = ' ';
                                            for (j=0;j<q_line_count;j++) {
                                                if (q_lines[j] == q_position - 1) {
                                                    q_marker = '+';
                                                    break;
                                                }
                                            }
                                            od_set_cursor(q_position - 1 - q_start + 3, 1);
                                            od_set_color(D_BLACK, D_GREEN);
                                            od_printf("`bright yellow`%c`bright white`%s", q_marker, quote_lines[q_position - 1]);
                                            od_clr_line();                                            
                                        }                                        
                                    }
                                } else if (ch.EventType == EVENT_CHARACTER) {
                                    if (ch.chKeyPress == 13) {
                                        q_unquote = 0;
                                        if (q_line_count > 0) {
                                            for (i=0;i<q_line_count;i++) {
                                                if (q_lines[i] == q_position) {
                                                    for (j=i;j<q_line_count-1;j++) {
                                                        q_lines[j] = q_lines[j + 1];
                                                    }
                                                    q_line_count--;
                                                    if (q_line_count == 0) {
                                                        free(q_lines);
                                                    } else {
                                                        q_lines = realloc(q_lines, q_line_count * sizeof(int));
                                                    }
                                                    q_unquote = 1;
                                                    break;
                                                }
                                            }
                                        }
                                        if (!q_unquote) {
                                            if (q_line_count == 0) {
                                                q_lines = (int *)malloc(sizeof(int *));
                                            } else {
                                                q_lines = (int *)realloc(q_lines, sizeof(int *) * (q_line_count + 1));
                                            }

                                            q_lines[q_line_count] = q_position;
                                            q_line_count++;
                                            od_set_cursor(q_position - q_start + 3, 1);
                                            od_set_color(D_BLACK, D_GREEN);
                                            od_printf("`bright yellow`%c`black green`%s", '+', quote_lines[q_position]);
                                            od_clr_line();                                              
                                        } else {
                                            od_set_cursor(q_position - q_start + 3, 1);
                                            od_set_color(D_BLACK, D_GREEN);
                                            od_printf("`bright yellow`%c`black green`%s", ' ', quote_lines[q_position]);
                                            od_clr_line();  
                                        }
                                        
                                        
                                    } else if (tolower(ch.chKeyPress) == 'q') {
                                        for (i=0;i<q_line_count;i++) {
                                            if (body_line_count == 0) {
                                                body_lines = (char **)malloc(sizeof(char *));
                                            } else {
                                                body_lines = (char **)realloc(body_lines, sizeof(char *) * (body_line_count + 1));
                                            }
                                            body_lines[body_line_count] = strdup(quote_lines[q_lines[i]]);
                                            body_line_count++;

                                            position_y++;
                                        }
                                        if (q_line_count) {
                                            free(q_lines);
                                        }
                                        position_x = 0;
                                        q_done = 1;
                                    } else if (tolower(ch.chKeyPress) == 'a') {
                                        if (q_line_count) {
                                            free(q_lines);
                                        }
                                        q_done = 1;
                                    }
                                }

                            }

                        }
                        // restore screen
                        od_set_color(L_WHITE, D_BLACK);
                        od_clr_scr();
                        od_set_cursor(1, 1);
                        od_set_color(L_WHITE, D_BLUE);
                        od_printf("  TO: %-32.32s AREA: %-32.32s", msgto, msgarea);
                        od_clr_line();
                        od_set_cursor(2, 1);
                        od_printf("SUBJ: %s", msgsubj);
                        od_clr_line();
                        od_set_cursor(24, 1);
                        od_printf("Enter /S to save, /Q to quote or /A to abort on a new line.");
                        od_clr_line();
                        od_set_cursor(3, 1);
                        od_set_color(L_WHITE, D_BLACK);

                        for (i=top_of_screen;i<top_of_screen + 20;i++) {
                            od_set_cursor(i - top_of_screen + 3, 1);
                            if (i < body_line_count) {
                                od_printf("%s", body_lines[i]);
                            }
                            od_clr_line();
                        }
                        position_x = 0;
                        memset(line, 0, 81);
                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    } else {
                        if (strlen(line) >= 73 && ch.chKeyPress != '\r') {
                            if (position_x == strlen(line)) {
                                strncat(line, &ch.chKeyPress, 1);
                                z = 1;
                            } else {
                                strncpy(line_cpy, line, position_x);
                                line_cpy[position_x] = '\0';
                                strncat(line_cpy, &ch.chKeyPress, 1);
                                strcat(line_cpy, &line[position_x]);
                                memset(line, 0, 81);
                                strcpy(line, line_cpy);
                                memset(line_cpy, 0, 81);
                                z = 0;
                            }

                            for (i=strlen(line)-1;i>0;i--) {
                                if (line[i] == ' ') {
                                    line[i] = '\0';
                                    strcpy(line_cpy, &line[i+1]);
                                    if (body_line_count == 0) {
                                        body_lines = (char **)malloc(sizeof(char *));
                                    } else {
                                        body_lines = (char **)realloc(body_lines, sizeof(char *) * (body_line_count + 1));
                                    }
                                    if (z == 1) {
                                        for (j=body_line_count;j>position_y;j--) {
                                            body_lines[j] = body_lines[j-1];
                                        }
                                        body_line_count++;
                                        body_lines[j] = strdup(line);

                                        position_y++;
                                        if (position_y - top_of_screen > 20) {
                                            top_of_screen++;
                                        }

                                        strcpy(line, line_cpy);
                                        memset(line_cpy, 0, 81);
                                        position_x = strlen(line);
                                    } else {
                                        if (strlen(body_lines[position_y]) + strlen(line_cpy) + 1 <= 73) {
                                            strcat(line_cpy, " ");
                                            strcat(line_cpy, body_lines[position_y]);
                                            free(body_lines[position_y]);
                                            body_lines[position_y] = strdup(line_cpy);
                                            memset(line_cpy, 0, 81);
                                            position_x++;
                                        } else {
                                            for (j=body_line_count;j>position_y;j--) {
                                                body_lines[j] = body_lines[j-1];
                                            }
                                            body_line_count++;
                                            body_lines[j] = strdup(line_cpy);

                                            memset(line_cpy, 0, 81);
                                            position_x++;
                                        }
                                    }
                                    break;
                                }
                            }
                            if (i==0) {
                                position_x++;
                                if (body_line_count == 0) {
                                    body_lines = (char **)malloc(sizeof(char *));
                                } else {
                                    body_lines = (char **)realloc(body_lines, sizeof(char *) * (body_line_count + 1));
                                }

                                for (i=body_line_count;i>position_y;i--) {
                                    body_lines[i] = body_lines[i-1];
                                }
                                body_line_count++;
                                body_lines[i] = strdup(line);
                                if (z == 1) {
                                    position_y++;
                                    if (position_y - top_of_screen > 20) {
                                        top_of_screen++;
                                    }
                                    position_x = 0;
                                }
                                memset(line, 0, 81);
                            }
                        } else {

                            if (position_x < strlen(line)) {
                                // insert line
                                if (body_line_count == 0) {
                                    body_lines = (char **)malloc(sizeof(char *));
                                } else {
                                    body_lines = (char **)realloc(body_lines, sizeof(char *) * (body_line_count + 1));
                                }

                                for (i=body_line_count;i>position_y;i--) {
                                    body_lines[i] = body_lines[i-1];
                                }

                                body_line_count++;
                                body_lines[i] = (char *)malloc(sizeof(char) * (position_x + 1));
                                strncpy(body_lines[i], line, position_x);
                                body_lines[i][position_x] = '\0';
                                strcpy(line_cpy, &line[position_x]);
                                memset(line, 0, 81);
                                strcpy(line, line_cpy);
                                memset(line_cpy, 0, 81);

                                position_y++;
                                if (position_y - top_of_screen > 20) {
                                    top_of_screen++;
                                }

                            } else {
                                if (body_line_count == 0) {
                                    body_lines = (char **)malloc(sizeof(char *));
                                } else {
                                    body_lines = (char **)realloc(body_lines, sizeof(char *) * (body_line_count + 1));
                                }

                                for (i=body_line_count;i>position_y;i--) {
                                    body_lines[i] = body_lines[i-1];
                                }
                                body_line_count++;
                                body_lines[i] = strdup(line);

                                position_y++;
                                if (position_y - top_of_screen > 20) {
                                    top_of_screen++;
                                }
                                position_x = 0;
                                memset(line, 0, 81);
                            }
                        }


                        if (old_top_of_screen != top_of_screen) {
                            od_set_cursor(3, 1);

                            for (i=top_of_screen;i<position_y;i++) {
                                od_set_cursor(i - top_of_screen + 3, 1);
                                od_printf("%s", body_lines[i]);
                                od_clr_line();
                            }
                            od_set_cursor(i - top_of_screen + 3, 1);
                            od_printf("%s", line);
                            od_clr_line();
                            for (i=position_y;i<body_line_count && i - top_of_screen < 20;i++) {
                                od_set_cursor(i - top_of_screen + 4, 1);
                                od_printf("%s", body_lines[i]);
                                od_clr_line();
                            }
                        } 
                        old_top_of_screen = top_of_screen;
                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    }
                } else {
                    if (ch.chKeyPress == '\b') {
                        if (position_x == 0) {
                            // TODO
                        } else {
                            if (position_x >= strlen(line)) {
                                strncpy(line_cpy, line, strlen(line) - 1);
                                line_cpy[strlen(line) - 1] = '\0';
                                memset(line, 0, 81);
                                strcpy(line, line_cpy);
                                memset(line_cpy, 0, 81);
                                position_x--;
                            } else {
                                strncpy(line_cpy, line, position_x -1);
                                line_cpy[position_x - 1] = '\0';
                                strcat(line_cpy, &line[position_x]);
                                strcpy(line, line_cpy);
                                memset(line_cpy, 0, 81);
                                position_x--;

                            }
                        }
                    } else if (ch.chKeyPress != '\n') {
                        if (position_x >= strlen(line)) {
                            strncat(line, &ch.chKeyPress, 1);
                        } else {
                            strncpy(line_cpy, line, position_x);
                            line_cpy[position_x] = '\0';
                            strncat(line_cpy, &ch.chKeyPress, 1);
                            strcat(line_cpy, &line[position_x]);
                            memset(line, 0, 81);
                            strcpy(line, line_cpy);
                            memset(line_cpy, 0, 81);
                        }
                        position_x++;

                    }

                    if (position_x > 1) {
                        if (position_y > 20) {
                            od_set_cursor(23, position_x - 1);
                            od_clr_line();
                        } else {
                            od_set_cursor(position_y + 3, position_x - 1);
                            od_clr_line();
                        }

                        for (i = position_x - 2; i < position_x; i++) {
                            od_printf("%c", line[i]);
                        }
                    } else {
                        if (position_y > 20) {
                            od_set_cursor(23, 1);
                            od_clr_line();
                        } else {
                            od_set_cursor(position_y + 3, 1);
                            od_clr_line();
                        }

                        for (i = 0; i < position_x; i++) {
                            od_printf("%c", line[i]);
                        }                        
                    }
                    if (position_x < strlen(line) ) {
                        for (i = position_x; i < strlen(line); i++) {
                            od_printf("%c", line[i]);
                        }
                    }

                    if (position_y > 20) {
                        od_set_cursor(23, position_x + 1);
                    } else {
                        od_set_cursor(position_y - top_of_screen + 3, position_x + 1);
                    }
                }
            }
        }
    }
}

#if _MSC_VER
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
#else
int main(int argc, char **argv)
{
#endif
    char *msgtmp;
    char *msginf;
    char path_sep;
    char *msgpath;
    int noquote;
    char buffer[256];
    FILE *fptr;
    char *body;
    int i;
    
    msgpath = NULL;
    
#if _MSC_VER
    int j;

	for (i=0;i<strlen(lpszCmdLine);i++) {
        if (strncmp(&lpszCmdLine[i], "-MSGTMP ", 8) == 0 || strncmp(&lpszCmdLine[i], "/MSGTMP ", 8) == 0) {
            msgpath = strdup(&lpszCmdLine[i + 8]);
            for (j=0;j<strlen(msgtmp);j++) {
                if (msgpath[j] == ' ') {
                    msgpath[j] = '\0';
                    break;
                }
            }
        }
	}
    od_parse_cmd_line(lpszCmdLine);
    path_sep = '\';'
#else
	for (i=0;i<argc;i++) {
		if (strcmp(argv[i], "-MSGTMP") == 0 || strcmp(argv[i], "/MSGTMP") == 0) {

			msgpath = strdup(argv[i+1]);
		}
	}
    od_parse_cmd_line(argc, argv);
    path_sep = '/';
#endif

    if (msgpath == NULL) {
        fprintf(stderr, "No MSGTMP switch specified!\n");
        exit(0);
    }

    od_init();

    msgtmp = (char *)malloc(strlen(msgpath) + 8);
    if (!msgtmp) {
        od_printf("Out of Memory!\r\n");
        od_exit(-1, FALSE);
    }

    msginf = (char *)malloc(strlen(msgpath) + 8);
    if (!msginf) {
        od_printf("Out of Memory!\r\n");
        od_exit(-1, FALSE);
    }

    sprintf(msgtmp, "%s%cMSGTMP", msgpath, path_sep);
    sprintf(msginf, "%s%cMSGINF", msgpath, path_sep);


    fptr = fopen(msginf, "r");

    if (!fptr) {
        sprintf(msginf, "%s%cmsginf", msgpath, path_sep);
        fptr = fopen(msginf, "r");
        if (!fptr) {
            od_printf("Unable to open MSGINF!\r\n");
            od_exit(-1, FALSE);
            return -1;
        }
    }

    fgets(buffer, 256, fptr);
    for (i=strlen(buffer) - 1; i > 0; i--) {
        if (buffer[i] != '\r' && buffer[i] != '\n') {
            break;
        } else {
            buffer[i] = '\0';
        }
    }

    msgfrom = strdup(buffer);

    fgets(buffer, 256, fptr);
    for (i=strlen(buffer) - 1; i > 0; i--) {
        if (buffer[i] != '\r' && buffer[i] != '\n') {
            break;
        } else {
            buffer[i] = '\0';
        }
    }

    msgto = strdup(buffer);

    fgets(buffer, 256, fptr);
    for (i=strlen(buffer) - 1; i > 0; i--) {
        if (buffer[i] != '\r' && buffer[i] != '\n') {
            break;
        } else {
            buffer[i] = '\0';
        }
    }

    msgsubj = strdup(buffer);

    fgets(buffer, 256, fptr); // msg no, we don't care

    fgets(buffer, 256, fptr);
    for (i=strlen(buffer) - 1; i > 0; i--) {
        if (buffer[i] != '\r' && buffer[i] != '\n') {
            break;
        } else {
            buffer[i] = '\0';
        }
    }

    msgarea = strdup(buffer);

    fgets(buffer, 256, fptr);
    for (i=strlen(buffer) - 1; i > 0; i--) {
        if (buffer[i] != '\r' && buffer[i] != '\n') {
            break;
        } else {
            buffer[i] = '\0';
        }
    }

    if (strcasecmp(buffer, "YES") == 0) {
        msgpriv = 1;
    } else {
        msgpriv = 0;
    }


    fclose(fptr);

    noquote = 0;

    fptr = fopen(msgtmp, "r");

    if (!fptr) {
        sprintf(msgtmp, "%s%cmsgtmp", msgpath, path_sep);
        fptr = fopen(msgtmp, "r");
        if (!fptr) {
            sprintf(msgtmp, "%s%cMSGTMP", msgpath, path_sep);
            noquote = 1;
        }
    }
    quote_line_count = 0;
    if (!noquote) {
        fgets(buffer, 73, fptr);
        while (!feof(fptr)) {
            for (i=strlen(buffer) - 1; i >= 0; i--) {
                if (buffer[i] != '\r' && buffer[i] != '\n') {
                    break;
                } else {
                    buffer[i] = '\0';
                }
            }

            if (quote_line_count == 0) {
                quote_lines = (char **)malloc(sizeof(char *));
            } else {
                quote_lines = (char **)realloc(quote_lines, sizeof(char *) * (quote_line_count + 1));
            }

            quote_lines[quote_line_count] = (char *)malloc(strlen(buffer) + 5);

            sprintf(quote_lines[quote_line_count], " %c> %s", msgto[0], buffer);

            quote_line_count++;

            fgets(buffer, 73, fptr);
        }
        fclose(fptr);
        unlink(msgtmp);
    }

    body = message_editor();

    if (body == NULL) {
        od_printf("Message Aborted!\r\n");
        od_exit(0, FALSE);
        return 0;
    }

    for (i=0;i<quote_line_count;i++) {
        free(quote_lines[i]);
    }

    free(quote_lines);

    fptr = fopen(msgtmp, "w");

    if (!fptr) {
        od_printf("Error saving message!\r\n");
        od_exit(-1, FALSE);
        return -1;
    }

    fwrite(body, 1, strlen(body), fptr);

    fclose(fptr);

    od_exit(0, FALSE);
    return 0;
}

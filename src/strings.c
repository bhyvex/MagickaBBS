#include "bbs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct bbs_config conf;

char *undefined = "Undefined String";
char **strings;
int string_count;

void chomp(char *string) {
	while (strlen(string) && (string[strlen(string)-1] == '\r' || string[strlen(string)-1] == '\n')) {
		string[strlen(string)-1] = '\0';
	}
}

char *parse_newlines(char *string) {
	char *newstring = (char *)malloc(strlen(string) + 1);
	int pos = 0;
	int i;
	for (i=0;i<strlen(string);i++) {
		if (string[i] == '\\') {
			if (i < strlen(string) - 1) {
				i++;
				if (string[i] == 'n') {
					newstring[pos++] = '\n';
				} else if (string[i] == 'r') {
					newstring[pos++] = '\r';
				} else if (string[i] == '\\') {
					newstring[pos++] = '\\';
				} else if (string[i] == 'e') {
					newstring[pos++] = '\e';
				}
				newstring[pos] = '\0';
			}
		} else {
			newstring[pos++] = string[i];
			newstring[pos] = '\0';
		}
	}
	return newstring;
}

char *get_string(int offset) {
	if (offset >= string_count) {
		return undefined;
	}

	return strings[offset];
}

void load_strings() {
	FILE *fptr;
	char buffer[1024];

	if (conf.string_file == NULL) {
		fprintf(stderr, "Strings file can not be undefined!\n");
		exit(-1);
	}

	fptr = fopen(conf.string_file, "r");
	if (!fptr) {
		fprintf(stderr, "Unable to open strings file!\n");
		exit(-1);
	}

	string_count = 0;

	fgets(buffer, 1024, fptr);
	while (!feof(fptr)) {
		chomp(buffer);
		if (string_count == 0) {
			strings = (char **)malloc(sizeof(char *));
		} else {
			strings = (char **)realloc(strings, sizeof(char *) * (string_count + 1));
		}
		strings[string_count] = parse_newlines(buffer);
		string_count++;
		fgets(buffer, 1024, fptr);
	}
	fclose(fptr);
}

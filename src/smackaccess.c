/*
 * Copyright (C) 2007 Casey Schaufler <casey@schaufler-ca.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 *
 * Authors:
 *	Casey Schaufler <casey@schaufler-ca.com>
 *
 * Returns 1 on success, 0 on failure, -1 on error.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "smack.h"

struct rule {
	struct rule	*next;
	char		subject[SMACK_SIZE];
	char		object[SMACK_SIZE];
	char		access[10];
};

static struct rule *rules;

static void read_load(void)
{
	char subject[SMACK_SIZE];
	char object[SMACK_SIZE];
	char mode[10];
	struct rule *rp;
	FILE *fp;

	if (rules != NULL)
		return;

	fp = fopen(SMACK_LOAD, "r");
	if (fp == NULL)
		return;

	while (fscanf(fp, "%24s %24s %10s", subject, object, mode) == 3) {
		rp = malloc(sizeof(struct rule));
		if (rp == NULL)
			break;
		strncpy(rp->subject, subject, SMACK_SIZE);
		strncpy(rp->object, object, SMACK_SIZE);
		strncpy(rp->access, mode, 10);
		rp->next = rules;
		rules = rp;
	}

	fclose(fp);
}

char *strcasefind(char *subject, char c)
{
	char lower = (c >= 'A' && c <= 'Z') ? (c - 'A' + 'a') : c;
	char upper = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
	while (*subject && *subject != lower && *subject != upper)
		++subject;
	if (*subject)
		return subject;
	return NULL;
}

int smackaccess(char *subject, char *object, char *access)
{
	struct rule *rp;
	int r = 0;
	int w = 0;
	int x = 0;
	int a = 0;
	int i;

	for (i = 0; access[i] != '\0'; i++) {
		switch (access[i]) {
		case '-':
			break;
		case 'R':
		case 'r':
			r = 1;
			break;
		case 'W':
		case 'w':
			w = 1;
			break;
		case 'X':
		case 'x':
			x = 1;
			break;
		case 'A':
		case 'a':
			a = 1;
			break;
		default:
			return -1;
		}
	}
	if (!r && !w && !x && !a)
		return -1;
	/*
	 * Hardcoded comparisons.
	 *
	 * A star subject can't access any object.
	 */
	if (strcmp(subject, SMACK_STAR) == 0)
		return 0;
	/*
	 * A star object can be accessed by any subject.
	 */
	if (strcmp(object, SMACK_STAR) == 0)
		return 1;
	/*
	 * An object can be accessed in any way by a subject
	 * with the same label.
	 */
	if (strcmp(subject, object) == 0)
		return 1;
	/*
	 * A hat subject can read any object.
	 * A floor object can be read by any subject.
	 */
	if (!w && !a) {
		if (strcmp(object, SMACK_FLOOR) == 0)
			return 1;
		if (strcmp(subject, SMACK_HAT) == 0)
			return 1;
	}
	/*
	 * Read in the rules if it hasn't happened yet.
	 */
	if (rules == NULL)
		read_load();

	/*
	 * Beyond here an explicit relationship is required.
	 * If the requested access is contained in the available
	 * access (e.g. read is included in readwrite) it's
	 * good.
	 */
	for (rp = rules; rp != NULL; rp = rp->next)
		if (strcmp(rp->subject, subject) == 0 &&
		    strcmp(rp->object, object) == 0)
			break;

	if (rp == NULL)
		return 0;
	if (r && strcasefind(rp->access, 'r') == 0)
		return 0;
	if (w && strcasefind(rp->access, 'w') == 0)
		return 0;
	if (x && strcasefind(rp->access, 'x') == 0)
		return 0;
	if (a && strcasefind(rp->access, 'a') == 0)
		return 0;

	return 1;
}

/*
 * smackcipso - properly format smack access cipsos for
 * loading into the kernel by writing to /smack/cipso.
 *
 * Copyright (C) 2007 Casey Schaufler <casey@schaufler-ca.com>
 * Copyright (C) 2011 Nokia Corporation.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 *
 *	This program is distributed in the hope that it will be useful, but
 *	WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *	General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public
 *	License along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 *	02110-1301 USA
 *
 * Authors:
 *	Casey Schaufler <casey@schaufler-ca.com>
 *	Ahmed S. Darwish <darwish.07@gmail.com>
 *	Jarkko Sakkinen <jarkko.sakkinen@intel.com>
 *
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LSIZE 23
#define NSIZE 4
#define MAXCATNUM 239
#define MAXCATVAL 63
#define MAXLEVEL 255

int
writecipso(FILE *infp)
{
	int cipsofd;
	char line[512];
	char cipso[LSIZE + NSIZE + NSIZE + (NSIZE * MAXCATNUM)];
	char cats[MAXCATNUM+1][NSIZE+1];
	char *cp;
	int level;
	int cat;
	int i;
	int err;

	cipsofd = open("/smack/cipso", O_RDWR);
	if (cipsofd < 0) {
		perror("opening /smack/cipso");
		return -1;
	}

	while (fgets(line, sizeof(line), infp) != NULL) {
		err = 0;

		if ((cp = strchr(line, '\n')) == NULL) {
			fprintf(stderr, "missing newline \"%s\"\n", line);
			continue;
		}
		*cp = '\0';
		cp = strtok(line, " \t");
		if (cp == NULL) {
			fprintf(stderr, "Empty line: \"%s\"\n", line);
			continue;
		}
		sprintf(cipso, "%-23s ", line);
		if (strlen(cipso) != 24) {
			fprintf(stderr, "Bad label starting: \"%s\"\n", line);
			continue;
		}
		cp = strtok(NULL, " \t");
		if (cp == NULL) {
			fprintf(stderr, "Missing level: \"%s\"\n", line);
			continue;
		}
		if (!isdigit(*cp)) {
			fprintf(stderr, "Bad level: \"%s\"\n", cp);
			continue;
		}
		level = atoi(cp);
		if (level > MAXLEVEL) {
			fprintf(stderr, "Bad level: \"%s\"\n", cp);
			continue;
		}
		sprintf(cipso+LSIZE+1, "%-4d", level);

		cp = strtok(NULL, " \t");
		for (i = 0; cp != NULL; cp = strtok(NULL, " \t"), i++) {
			if (!isdigit(*cp)) {
				fprintf(stderr, "Bad category \"%s\"\n", cp);
				err = 1;
				break;
			}
			cat = atoi(cp);
			if (i >= MAXCATNUM) {
				fprintf(stderr, "Maximum number of categories"
					"exceeded \"%d\"\n", i);
				err = 1;
				break;
			}
			if (cat > MAXCATVAL) {
				fprintf(stderr, "Bad category \"%s\"\n", cp);
				err = 1;
				break;
			}
			sprintf(cats[i], "%-4d", cat);
		}
		if (err)
			continue;

		sprintf(cipso+LSIZE+1+NSIZE, "%-4d", i);
		while (i > 0)
			strcat(cipso, cats[--i]);
		err = write(cipsofd, cipso, strlen(cipso));
		if (err < 0)
			perror("writing /smack/cipso");
	}
	return 0;
}

int
main(int argc, char *argv[])
{
	int i = 1;
	FILE *infp = NULL;

	if (argc == 1) {
		if (writecipso(stdin) == -1)
			exit(1);
	} else {
		for (; i < argc; i++) {
			infp = fopen(argv[i], "r");
			if (infp == NULL) {
				fprintf(stderr, "opening %s: %s\n", argv[i], strerror(errno));
				exit(1);
			}
			if (writecipso(infp) == -1)
				exit(1);
			fclose(infp);
		}
	}

	exit(0);
}

/*
 * smackload - properly format smack access rules for
 * loading into the kernel by writing to /smack/load.
 *
 *
 * Copyright (C) 2007 Casey Schaufler <casey@schaufler-ca.com>
 * Copyright (C) 2011 Nokia Corporation.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, version 2.
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
 *      Casey Schaufler <casey@schaufler-ca.com>
 *      Ahmed S. Darwish <darwish.07@gmail.com>
 *      Jarkko Sakkinen <jarkko.sakkinen@intel.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define LSIZE 23
#define ASIZE 5

#define WRITE_NEW (LSIZE + LSIZE + ASIZE + 2)
#define WRITE_OLD (WRITE_NEW - 1)

static struct option opts[] = {
	{"help",   no_argument, NULL, 'h'},
	{"clear",  no_argument, NULL, 'c'},
	{NULL, 0, NULL, 0}
};

int writeload(FILE *infp, int clearflag)
{
	int loadfd;
	char line[80];
	char rule[WRITE_NEW + 1];
	char subject[LSIZE + 1];
	char object[LSIZE + 1];
	char accesses[ASIZE + 1];
	char real[ASIZE + 1];
	char *cp;
	int i;
	int err;
	int writesize = WRITE_NEW;

	loadfd = open("/smack/load", O_RDWR);
	if (loadfd < 0) {
		perror("opening /smack/load");
		return -1;
	}

	while (fgets(line, 80, infp) != NULL) {
		err = 0;
		if ((cp = strchr(line, '\n')) != NULL)
			*cp = '\0';

		if (sscanf(line,"%23s %23s %5s",subject,object,accesses) != 3) {
			fprintf(stderr, "Bad input line \"%s\"\n", line);
			continue;
		}

		strcpy(real, "-----");
		if (!clearflag) {
			for (i = 0; i < ASIZE && accesses[i] != '\0' && err == 0; i++) {
				switch (accesses[i]) {
				case 'r':
				case 'R':
					real[0] = 'r';
					break;
				case 'w':
				case 'W':
					real[1] = 'w';
					break;
				case 'x':
				case 'X':
					real[2] = 'x';
					break;
				case 'a':
				case 'A':
					real[3] = 'a';
					break;
				case 't':
				case 'T':
					real[4] = 't';
					break;
				case '\0':
				case '-':
					break;
				default:
					err = 1;
					break;
				}
			}
		}

		if (err == 1) {
			fprintf(stderr, "Bad access specification \"%s\"\n",
				line);
			continue;
		}

		sprintf(rule, "%-23s %-23s %5s", subject, object, real);

		if (writesize == WRITE_OLD && real[4] != '-')
			fprintf(stderr, "Kernel does not support 't' "
					"specification \"%s\"\n", line);

		err = write(loadfd, rule, writesize);
		if (err == writesize)
			continue;

		if (err == -1 && errno == EINVAL && writesize == WRITE_NEW) {
			writesize = WRITE_OLD;
			if (real[4] != '-')
				fprintf(stderr, "Kernel does not support 't' "
						"specification \"%s\"\n", line);
			err = write(loadfd, rule, writesize);
			if (err == writesize)
				continue;
		}
		perror("writing /smack/load");
	}
	return 0;
}

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s [options] files...\n", arg0);
	fprintf(target,
	"options:\n"
	"  -h, --help            show this help message\n"
	"  -c, --clear           replace all accesses with -\n"
	);
	exit(exitstatus);
}

int main(int argc, char *argv[])
{
	int c;
	int clearflag = 0;
	int i = 1;
	FILE *infp = NULL;

	while ((c = getopt_long(argc, argv, "c", opts, NULL)) != -1) {
		switch (c) {
		case 'c':
			clearflag = 1;
			break;
		case 'h':
			usage(argv[0], stdout, 0);
			break;
		default:
			usage(argv[0], stderr, 1);
			exit(1);
		}
		i++;
	}

	if (i == argc) {
		if (writeload(stdin, clearflag) == -1)
			exit(1);
	} else {
		for (; i < argc; i++) {
			infp = fopen(argv[i], "r");
			if (infp == NULL) {
				fprintf(stderr, "opening %s: %s\n", argv[i], strerror(errno));
				exit(1);
			}
			if (writeload(infp, clearflag) == -1)
				exit(1);
			fclose(infp);
		}
	}

	exit(0);
}

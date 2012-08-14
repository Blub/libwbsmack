#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "smack.h"

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s < rules > output\n", arg0);
	exit(exitstatus);
}

#define isspace(x) ( (x) == ' ' || \
                     (x) == '\t' || \
                     (x) == '\r' || \
                     (x) == '\f' || \
                     (x) == '\v' )

static int extract(const char *arg0, char **start, char *target)
{
	size_t el = 0;
	char *pos = *start;
	// Extract subject
	while (*pos && !isspace(*pos) && el < SMACK_SIZE-1) {
		target[el++] = *pos;
		++pos;
	}
	// Check length
	if (el == SMACK_SIZE-1) {
		*pos = 0;
		fprintf(stderr, "%s: label `%s' exceeds length limit\n"
		        "For long labels there's no need to use this tool\n", arg0, *start);
		return 0;
	}
	// Fill up
	while (el != SMACK_SIZE)
		target[el++] = ' ';
	*start = pos;
	return 1;
}

int main(int argc, char **argv)
{
	char *line = NULL;
	size_t alen = 0;
	ssize_t len;
	int result = 0;

	struct {
		char subject[SMACK_SIZE];
		char object[SMACK_SIZE];
		char rwxat[5];
	} rule;

	if (argc > 1) {
		if (!strcmp(argv[1], "-h") ||
		    !strcmp(argv[1], "--help"))
			usage(argv[0], stdout, 0);
		usage(argv[0], stdout, 1);
	}

	while ((len = getline(&line, &alen, stdin)) >= 0)
	{
		int err;
		size_t i;
		char *pos = line;

		if (!len) // skip empty
			continue;

		// skip white
		while (*pos && isspace(*pos))
			++pos;

		if (!*pos || *pos == '#') {
			// skip comment and empty lines
			continue;
		}

		// read subject
		if (!extract(argv[0], &pos, rule.subject)) {
			result = 1;
			continue;
		}

		// skip white
		while (*pos && isspace(*pos))
			++pos;
		// check for early end
		if (!*pos) {
			fprintf(stderr, "%s: invalid line: %s", argv[0], line);
			result = 1;
			continue;
		}

		// read object
		if (!extract(argv[0], &pos, rule.object)) {
			result = 1;
			continue;
		}

		// skip white
		while (*pos && isspace(*pos))
			++pos;
		// check for early end
		if (!*pos) {
			fprintf(stderr, "%s: rule without permissions stated, use - to state no permissions\n",
			        argv[0]);
			result = 1;
			continue;
		}

		for (i = 0; i < sizeof(rule.rwxat); ++i)
			rule.rwxat[i] = '-';
		err = 0;
		// check for no-access rule
		if (*pos == '-')
		{
			++pos;
			// skip white
			while (*pos && isspace(*pos))
				++pos;
			// should end now
			if (*pos) {
				fprintf(stderr, "%s: garbage after permissions in line: %s\n", argv[0], line);
				result = 1;
				continue;
			}
		} else {
			--pos;
			do {
				++pos;
				// read permissions:
				switch (*pos) {
					case 'r':
					case 'R':
						rule.rwxat[0] = 'r';
						break;
					case 'w':
					case 'W':
						rule.rwxat[1] = 'w';
						break;
					case 'x':
					case 'X':
						rule.rwxat[2] = 'x';
						break;
					case 'a':
					case 'A':
						rule.rwxat[3] = 'a';
						break;
					case 't':
					case 'T':
						rule.rwxat[4] = 't';
						break;
					case 0:
						break;
					case '\n':
						*pos = 0;
						break;
					default:
						fprintf(stderr, "%s: invalid permission character (%i:) %c\n", argv[0], (int)*pos, *pos);
						err = 1;
						break;
				};
				if (err) {
					result = 1;
					break;
				}
			} while(*pos);
		}
		if (err)
			continue;
		write(1, (void*)&rule, sizeof(rule));
	}

	return result;
}

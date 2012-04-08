#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
//include <ctype.h> <- No. This can be influenced by locales

#include "smack.h"

// the manpage talks about locales... that would be ugly
#define isupper(x) ( (x) >= 'A' && (x) <= 'Z' )
#define islower(x) ( (x) >= 'a' && (x) <= 'z' )
#define isalpha(x) (isupper(x) || islower(x))
#define isspace(x) ( (x) == ' ' || \
                     (x) == '\t' || \
                     (x) == '\r' || \
                     (x) == '\f' || \
                     (x) == '\v' )

int getsmackuser_r(const char *username, struct smackuser *out,
                   char *buffer, size_t buflen)
{
	FILE *fp;
	char   *line;
	size_t linelen;
	int    retval = -1;

	fp = fopen(SMACK_USERS, "r");
	if (!fp) {
		perror("opening " SMACK_USERS);
		errno = ENOSYS;
		return -1;
	}

	line = malloc(32); // because it should suffice in theory
	linelen = 32;
	errno = ENOENT;
	while (getline(&line, &linelen, fp) != -1)
	{
		const char *userstart = NULL;
		const char *labelstart = NULL;

		char *inl = line;

		// skip initial whitespace
		while (isspace(*inl))
			++inl;

		if (!*inl)
			continue;

		// extract username:
		userstart = inl;
		// it may not be "standard" but nothing keeps me from using
		// _ or - in /etc/passwd
		while (isalpha(*inl) || *inl == '_' || *inl == '-')
			++inl;
		// insert a nulbyte to end the username
		*inl = '\0';
		++inl;

		// Check the username now
		if (strcmp(username, userstart))
			continue;

		while (isspace(*inl))
			++inl;

		if (!*inl)
			continue;

		// extract label:
		labelstart = inl;
		while (*inl && !isspace(*inl))
			++inl;
		*inl = '\0';

		if (!buffer)
		{
			out->su_name = strdup(userstart);
			out->su_label = strdup(labelstart);
			if (!out->su_name || !out->su_label)
				errno = ENOMEM;
			else
				retval = 0;
			break;
		}
		else
		{
			size_t ulen = strlen(userstart);
			size_t llen = strlen(labelstart);
			if (buflen < (ulen + llen + 2)) {
				errno = ENOMEM;
				break;
			}
			memcpy(buffer, userstart, ulen);
			buffer[ulen] = 0;
			memcpy(buffer + ulen + 1, labelstart, llen);
			buffer[ulen + 1 + llen] = 0;
			retval = 0;
			break;
		}
	}

	free(line);
	return retval;
}

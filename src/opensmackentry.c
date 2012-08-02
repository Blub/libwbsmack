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
                     (x) == '\n' || \
                     (x) == '\v' )

static struct smackentry* newentry(const char *username)
{
	struct smackentry *entry;
	entry = (struct smackentry*)malloc(sizeof(*entry));
	entry->su_name = strdup(username);
	entry->su_labelcount = 0;
	entry->_su_allocated = 4;
	entry->su_labels = (char**)malloc(sizeof(char*)*entry->_su_allocated);
	entry->su_any = 0;
	return entry;
}

static void addentry(struct smackentry *entry, const char *label)
{
	if (!strcmp(label, "*ANY")) {
		entry->su_any = 1;
		return;
	}
	if (entry->su_labelcount == entry->_su_allocated) {
		entry->_su_allocated *= 2;
		entry->su_labels = (char **)
			realloc(entry->su_labels, sizeof(char*)*entry->_su_allocated);
	}
	entry->su_labels[entry->su_labelcount++] = strdup(label);
}

void closesmackentry(struct smackentry *entry)
{
	size_t i;
	for (i = 0; i < entry->su_labelcount; ++i)
		free((void*)entry->su_labels[i]);
	free((void*)entry->su_labels);
	free((void*)entry->su_name);
	free((void*)entry);
}

const char *smackentryget(struct smackentry const *entry, size_t index)
{
	if (index >= entry->su_labelcount)
		return NULL;
	return entry->su_labels[index];
}

int smackentrycontains(struct smackentry const *entry, const char *label)
{
	size_t i;
	if (entry->su_any)
		return 1;
	for (i = 0; i < entry->su_labelcount; ++i) {
		if (!strcmp(label, entry->su_labels[i]))
			return 1;
	}
	return 0;
}

struct smackentry* opensmackentry(const char *username)
{
	FILE *fp;
	char   *line;
	size_t linelen;
	struct smackentry *entry = NULL;

	fp = fopen(SMACK_USERS, "r");
	if (!fp) {
		perror("opening " SMACK_USERS);
		errno = ENOSYS;
		return NULL;
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

		// extract all labels:

		entry = newentry(username);
		do
		{
			labelstart = inl;
			while (*inl && !isspace(*inl))
				++inl;
			if (!*inl) {
				// end of line
				addentry(entry, labelstart);
				break;
			}
			// more following:
			*inl = '\0';
			addentry(entry, labelstart);
			++inl;
			while (isspace(*inl))
				++inl;
		} while(*inl);
		break;
	}

	free(line);
	fclose(fp);
	return entry;
}

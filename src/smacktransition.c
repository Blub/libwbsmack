#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>

#include "smack.h"

typedef struct rule_s {
	char subject[SMACK_SIZE];
	char object[SMACK_SIZE];
	struct rule_s *next;
} rule_t;

typedef struct filecache_s {
	char   filename[256];
	time_t readtime;
	time_t checktime;
	struct filecache_s *next;
} filecache_t;

static rule_t *rules;
#ifdef SMACK_TRANSITION_FILE
static filecache_t fc_file = { {0}, 0 };
#endif
#ifdef SMACK_TRANSITION_DIR
static filecache_t *fc_dir = NULL;

static void addcache(const char *filename, time_t now)
{
	filecache_t *ent = (filecache_t*)malloc(sizeof(filecache_t));
	strncpy(ent->filename, filename, sizeof(ent->filename));
	ent->readtime = now;
	ent->next = fc_dir;
	ent->checktime = now;
	fc_dir = ent;
}
#endif

static void clearcache()
{
#ifdef SMACK_TRANSITION_FILE
	fc_file.readtime = 0;
#endif
#ifdef SMACK_TRANSITION_DIR
	while (fc_dir) {
		filecache_t *cur = fc_dir;
		fc_dir = cur->next;
		free(cur);
	}
#endif
	while (rules) {
		rule_t *cur = rules;
		rules = rules->next;
		free(cur);
	}
}

static void cacherule(const char *sub, const char *obj)
{
	rule_t *ent = (rule_t*)malloc(sizeof(rule_t));
	strncpy(ent->subject, sub, sizeof(ent->subject));
	strncpy(ent->object, obj, sizeof(ent->object));
	ent->next = rules;
	rules = ent;
}

static void readtransfile(const char *filename,
                          const char *subject, const char *object,
                          int *allowed, int *forbidden,
                          int is_nondirfile, time_t now)
{
	char *line = NULL;
	size_t n = 0;
	FILE *fp;
	char linesub[SMACK_SIZE+1];
	char lineobj[SMACK_SIZE+1];

	linesub[SMACK_SIZE] = 0;
	lineobj[SMACK_SIZE] = 0;

	fp = fopen(filename, "r");
	if (!fp)
		return;

#ifdef SMACK_TRANSITION_FILE
	if (is_nondirfile)
		fc_file.readtime = now;
#endif
#ifdef SMACK_TRANSITION_DIR
	else
		addcache(filename, now);
#endif

	while (getline(&line, &n, fp) != -1) {
		size_t n = strspn(line, " \t\r\n\f");
		// # are comments
		if (line[n] == '#')
			continue;
		// skip empty lines
		if (line[n] == '\0')
			continue;
		// read the rule
		if (sscanf(line,
		           " %" SMACK_SIZE_STR
		           "[a-zA-Z0-9_-] -> %" SMACK_SIZE_STR
		           "[a-zA-Z0-0_-] ",
		           linesub, lineobj) != 2)
		{
			fprintf(stderr, "Error in " SMACK_TRANSITION_FILE "\n");
			continue;
		}
		cacherule(linesub, lineobj);
		if (!allowed &&
		    !strcmp(subject, linesub) &&
		    !strcmp(object, lineobj))
		{
			*allowed = 1;
		}
	}

	fclose(fp);
}

static int checkcache(time_t now, int *allowed, int *forbidden)
{
	struct stat stbuf;
	int rc;
	int found;
	int existed;
	filecache_t *fi;

#ifdef SMACK_TRANSITION_FILE
	do {
		existed = (fc_file.readtime != 0);

		rc = stat(SMACK_TRANSITION_FILE, &stbuf);
		found = (rc == 0);

		// if the state of existance changed: reload
		if (existed != found) {
			// reload
			return 0;
		}

		// if it doesn't exist: ignore
		if (!existed)
			break;

		// otherwise check
		if (stbuf.st_mtime >= fc_file.readtime)
			return 0; // reload
	} while (0);
#endif

#ifdef SMACK_TRANSITION_DIR
	// then transition.d/* non-recursively
	do {
		DIR           *dir;
		struct dirent *entry;

		dir = opendir(SMACK_TRANSITION_DIR);
		if (!dir)
			break;

		for (entry = readdir(dir); entry; entry = readdir(dir))
		{
			// only read regular files
			if (entry->d_type != DT_REG)
				continue;

			// find it in cache:
			for (fi = fc_dir; fi; fi = fi->next)
			{
				if (!strcmp(fi->filename, entry->d_name))
					break;
			}
			if (!fi) {
				// a new file appeared
				// reload
				return 0;
			}
			// check the file:
			rc = stat(fi->filename, &stbuf);
			if (!rc) {
				// stat failed
				// reload
				return 0;
			}
			if (stbuf.st_mtime >= fi->readtime) {
				// changed, reload
				return 0;
			}
			// remember that we checkd this file
			fi->checktime = now;
		}

		closedir(dir);
	} while(0);

	// look for any files we haven't checked:
	for (fi = fc_dir; fi; fi = fi->next)
	{
		if (fi->checktime != now)
			return 0;
	}
#endif
	return 1;
}

int smackchecktrans(const char *subject, const char *object)
{
	int allowed = 0;
	int forbidden = 0;

	time_t now = time(NULL);

	if (checkcache(now, &allowed, &forbidden)) {
		// no need to reload
		return forbidden ? 0 : allowed;
	}

	clearcache();

#ifdef SMACK_TRANSITION_FILE
	// first check the file
	readtransfile(SMACK_TRANSITION_FILE, subject, object, &allowed, &forbidden, 1, now);
#endif

#ifdef SMACK_TRANSITION_DIR
	// then transition.d/* non-recursively
	do {
		DIR           *dir;
		struct dirent *entry;

		dir = opendir(SMACK_TRANSITION_DIR);
		if (!dir)
			break;

		for (entry = readdir(dir); entry; entry = readdir(dir))
		{
			// only read regular files
			if (entry->d_type != DT_REG)
				continue;

			// read
			readtransfile(entry->d_name, subject, object, &allowed, &forbidden, 0, now);
		}

		closedir(dir);
	} while(0);
#endif

	return forbidden ? 0 : allowed;
}

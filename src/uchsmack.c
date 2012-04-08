#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <attr/xattr.h>

#include "smack.h"

/* Change the smack label for a file - provided we have access to it.
 */

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s label files...\n", arg0);
	exit(exitstatus);
}

static int sanelabel(const char *label)
{
	while (*label) {
		if (*label == ' ' ||
		    *label == '\t' ||
		    *label == '\r' ||
		    *label == '\n' ||
		    *label == '\f')
		    	return 0;
		++label;
	}
	return 1;
}

int main(int argc, char **argv)
{
	int i;
	const char *label;
	size_t labellen;
	char self[SMACK_SIZE];

	if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
	{
		usage(argv[0], stdout, 0);
	}
	if (argc < 3) {
		usage(argv[0], stderr, 1);
	}

	label = argv[1];
	if (!sanelabel(label)) {
		fprintf(stderr, "%s: Invalid label: '%s'\n", argv[0], label);
		exit(1);
	}

	if (getsmack(self, sizeof(self)) == -1) {
		fprintf(stderr, "%s: Unable to determin own smack label.\n", argv[0]);
		exit(1);
	}

	labellen = strlen(label);
	for (i = 2; i < argc; ++i) {
		char filelabel[SMACK_SIZE];
		int fd = open(argv[i], O_RDWR);
		if (fd < 0) {
			perror("open");
			continue;
		}
		// Lock
		if (flock(fd, LOCK_EX) != 0) {
			perror("lock");
			close(fd);
			continue;
		}
		// Check for write-access to the SMACK label
		if (fgetxattr(fd, "security.SMACK64", filelabel, sizeof(filelabel)) == -1) {
			perror("getxattr");
			goto out;
		}
		if (!smackaccess(self, filelabel, "w")) {
			fprintf(stderr, "%s: no smack access to '%s' for '%s'\n", argv[0], filelabel, self);
			goto out;
		}
		// Update the SMACK label
		if (fsetxattr(fd, "security.SMACK64", label, labellen, 0) == -1) {
			perror("setxattr");
			goto out;
		}

out:
		(void)flock(fd, LOCK_UN);
		close(fd);

	}

	return 0;
}

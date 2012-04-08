#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
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
	cap_t caps;
	cap_flag_value_t capvalue = 0;

	uid_t myuid = geteuid();

	if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
	{
		usage(argv[0], stdout, 0);
	}
	if (argc < 3) {
		usage(argv[0], stderr, 1);
	}

#if 0
	// Drop DAC override capabilities (unless root)
	if (myuid != 0) {
		cap_value_t      capwhich = CAP_DAC_OVERRIDE;
		cap_flag_value_t capvalue = 0;
		cap_t caps = cap_get_proc();
		if (cap_get_flag(caps, capwhich, CAP_EFFECTIVE, &capvalue) != 0) {
			perror("cap_get_flag");
			exit(1);
		}
		if (capvalue) {
			if (cap_set_flag(caps, CAP_EFFECTIVE, 1, &capwhich, 0) != 0) {
				perror("cap_set_flag");
				exit(1);
			}
		}
		if (cap_get_flag(caps, capwhich, CAP_PERMITTED, &capvalue) != 0) {
			perror("cap_get_flag");
			exit(1);
		}
		if (capvalue) {
			if (cap_set_flag(caps, CAP_PERMITTED, 1, &capwhich, 0) != 0) {
				perror("cap_set_flag");
				exit(1);
			}
		}
		if (cap_set_proc(caps) != 0) {
			perror("cap_set_proc");
			exit(1);
		}
	}
#endif

	label = argv[1];
	if (!sanelabel(label)) {
		fprintf(stderr, "%s: Invalid label: '%s'\n", argv[0], label);
		exit(1);
	}

	if (getsmack(self, sizeof(self)) == -1) {
		fprintf(stderr, "%s: Unable to determin own smack label.\n", argv[0]);
		exit(1);
	}

	// Like chmod, only allow changing to labels we have write access to.
	// but honor CAP_MAC_OVERRIDE rather than checking for root.
	caps = cap_get_proc();
	if (cap_get_flag(caps, CAP_MAC_OVERRIDE, CAP_EFFECTIVE, &capvalue) != 0) {
		perror("cap_get_flag");
		exit(1);
	}
	if (!capvalue && !smackaccess(self, label, "w")) {
		fprintf(stderr, "%s: No write access for subject '%s' to object '%s'\n",
		        argv[0], self, label);
		exit(1);
	}

	labellen = strlen(label);
	for (i = 2; i < argc; ++i) {
#if 1
		char filelabel[SMACK_SIZE];
		int fd = open(argv[i], O_RDONLY); //RDWR, but directories don't want that
		if (fd < 0) {
			perror("open");
			continue;
		}
		/* Disabling this for now.
		 * A user should be able to change the label of a file within a folder
		 * he has write-access to.
		 * DAC will make sure the user doesn't change any *other* labels.
		 */
		// Lock
		if (flock(fd, LOCK_EX) != 0) {
			if (errno == EPERM) {
				fprintf(stderr, "%s: no access to for '%s'\n", argv[0], self);
			} else
				perror("lock");
			close(fd);
			continue;
		}
#if 0
		// Check for write-access to the SMACK label
		if (myuid != 0) {
			if (fgetxattr(fd, "security.SMACK64", filelabel, sizeof(filelabel)) == -1) {
				perror("getxattr");
				goto out;
			}
			if (!smackaccess(self, filelabel, "w")) {
				fprintf(stderr, "%s: no smack access to '%s' for '%s'\n", argv[0], filelabel, self);
				goto out;
			}
		}
#endif
		// Update the SMACK label
		if (fsetxattr(fd, "security.SMACK64", label, labellen, 0) == -1) {
			perror("setxattr");
			goto out;
		}

out:
		(void)flock(fd, LOCK_UN);
		close(fd);
#else
		if (lsetxattr(argv[i], "security.SMACK64", label, labellen, 0) == -1) {
			perror("setxattr");
		}
#endif
	}

	return 0;
}

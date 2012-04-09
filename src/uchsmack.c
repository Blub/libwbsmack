#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/prctl.h>
#include <linux/securebits.h>
#include <fcntl.h>
#include <attr/xattr.h>

#include "smack.h"

#define SMACKLABEL SMACK_XATTR

/* Change the smack label for a file - provided we have access to it.
 */

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s label files...\n", arg0);
	fprintf(target, "       %s -r|--remove files...\n", arg0);
	fprintf(target, "With -r or --remove, the " SMACKLABEL " attribute is removed.\n");
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
	char filelabel[SMACK_SIZE+1];
	cap_t caps;
	cap_flag_value_t capvalue = 0;
	int remove_label = 0;

	// uid_t myuid = geteuid();

	if (argc == 2 && (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")))
	{
		usage(argv[0], stdout, 0);
	}
	if (argc < 3) {
		usage(argv[0], stderr, 1);
	}

	label = argv[1];

	if (!strcmp(label, "-r") || !strcmp(label, "--remove"))
		remove_label = 1;
	else if (!sanelabel(label)) {
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
	cap_free(caps);

	// If our euid is not root however, we are using a setuid binary,
	// In any case we can drop any capability except CAP_MAC_ADMIN.
	if (getuid() != geteuid())
	{
		cap_flag_value_t has_setcap;
		cap_value_t what = CAP_MAC_ADMIN;
		capvalue = 1;

		// setuid-root binaries do have that
		if (cap_get_flag(caps, CAP_SETPCAP, CAP_EFFECTIVE, &has_setcap) != 0) {
			perror("cap_get_flag");
			exit(1);
		}

		caps = cap_init();
		if (cap_set_flag(caps, CAP_PERMITTED, 1, &what, capvalue) != 0 ||
		    cap_set_flag(caps, CAP_EFFECTIVE, 1, &what, capvalue) != 0 )
		{
			fprintf(stderr, "%s: cap_set_flag failed for some reason.\n", argv[0]);
			exit(1);
		}
		if (cap_set_proc(caps) != 0) {
			perror("cap_set_proc");
			fprintf(stderr, "%s: failed to drop privileges, aborting\n", argv[0]);
			exit(1);
		}
		cap_free(caps);

		// at last, if we can, change securebits:
		// We have already set the capabilities, do not KEEP_CAPS
		if (has_setcap) {
			if (prctl(PR_SET_SECUREBITS,
			          SECBIT_NOROOT | SECBIT_NOROOT_LOCKED |
			          SECBIT_NO_SETUID_FIXUP | SECBIT_NO_SETUID_FIXUP_LOCKED)
			    != 0)
			{
				perror("prctl");
				fprintf(stderr, "%s: failed to enforce unprivileged mode\n", argv[0]);
				exit(1);
			}
		}
	}

	// To remove a label we must have write access to the file's label
	if (!remove_label && !capvalue && (!smackaccess(self, label, "w"))) {
		fprintf(stderr, "%s: No write access for subject '%s' to object '%s'\n",
		        argv[0], self, label);
		exit(1);
	}

	labellen = strlen(label);
	for (i = 2; i < argc; ++i) {
		int fd = open(argv[i], O_RDONLY); //RDWR, but directories don't want that
		if (fd < 0) {
			perror("open");
			fprintf(stderr, "%s: error opening %s\n", argv[0], argv[i]);
			continue;
		}
		// Lock
		if (flock(fd, LOCK_EX) != 0) {
			if (errno == EPERM) {
				fprintf(stderr, "%s: no access for '%s'\n", argv[0], self);
			} else
				perror("lock");
			close(fd);
			continue;
		}
		if (remove_label) {
			// Check for write-access to the file's label
			if (fgetxattr(fd, SMACKLABEL, filelabel, sizeof(filelabel)) == -1) {
				perror("getxattr");
				goto out;
			}
			if (!smackaccess(self, filelabel, "w")) {
				fprintf(stderr, "%s: no write access for '%s' to '%s'\n", argv[0], self, filelabel);
				goto out;
			}
			// If the test passes we can remove the label
			if (fremovexattr(fd, SMACKLABEL) == -1) {
				perror("removexattr");
				goto out;
			}
		}
		else
		{
			// Update the SMACK label
			if (fsetxattr(fd, SMACKLABEL, label, labellen, 0) == -1) {
				perror("setxattr");
				goto out;
			}
		}

out:
		(void)flock(fd, LOCK_UN);
		close(fd);
	}

	return 0;
}

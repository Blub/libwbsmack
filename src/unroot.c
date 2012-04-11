#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <linux/securebits.h>

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s command [parameters...]\n", arg0);
	exit(exitstatus);
}

static void checkargs(int argc, char **argv)
{
	if (argc < 2) {
		usage(argv[0], stderr, 1);
	}
	if (!strcmp(argv[1], "-h") ||
	    !strcmp(argv[1], "--help")) {
	    	usage(argv[0], stdout, 0);
	}
}

int main(int argc, char **argv)
{
	pid_t euid = geteuid();
	int i;
	cap_value_t v[CAP_LAST_CAP+1];

	checkargs(argc, argv);

	if (getuid() != euid)
		setuid(euid);

	cap_t caps = cap_get_proc();
	for (i = 0; i <= CAP_LAST_CAP; ++i)
		v[i] = i;

	if (cap_set_flag(caps, CAP_INHERITABLE, CAP_LAST_CAP+1, v, CAP_SET) != 0) {
		exit(-1);
	}
	cap_free(caps);

	i = prctl(PR_SET_SECUREBITS,
	          SECBIT_NOROOT | SECBIT_NOROOT_LOCKED |
	          SECBIT_NO_SETUID_FIXUP | SECBIT_NO_SETUID_FIXUP_LOCKED);
	if (i != 0) {
		perror("prctl securebits");
		exit(i);
	}

	i = execvp(argv[1], &argv[1]);
	perror("exec");
	exit(i);
	return i;
}

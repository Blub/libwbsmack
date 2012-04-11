#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/capability.h>
#include <sys/prctl.h>
#include <linux/securebits.h>
#include <getopt.h>

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s [options ]command [parameters...]\n", arg0);
	fprintf(target,
	"options:\n"
	"   -h, --help           show this message\n"
	"   -n, --nocaps         use an empty inheritable set\n"
	"   -i, --icaps=capstr   set inheritable set to these capabilities\n"
	"                        see cap_from_text(3) for a description of\n"
	"                        the textual representation of capabilities\n"
	);
	exit(exitstatus);
}

static struct option lopts[] = {
	{ "help",     no_argument,       NULL, 'h' },
	{ "nocaps",   no_argument,       NULL, 'n' },
	{ "icaps",    required_argument, NULL, 'i' },

	{ NULL, 0, NULL, 0 }
};

static int         opt_nocaps = 0;
static const char *opt_icaps = NULL;

static void checkargs(int argc, char **argv)
{
	int o;
	int lind = 0;

	while ( (o = getopt_long(argc, argv, "+hni:", lopts, &lind)) != -1 ) {
		switch (o)
		{
			case 'h':
				usage(argv[0], stdout, 0);
				break;
			case 'n':
				opt_nocaps = 1;
				break;
			case 'i':
				if (opt_icaps) {
					fprintf(stderr, "%s:"
					        " multiple definition of"
					        " --icaps, using last.\n",
					        argv[0]);
				}
				opt_icaps = optarg;
				break;
		};
	}

	if (argc - optind < 2) {
		fprintf(stderr, "%s: command missing\n", argv[0]);
		usage(argv[0], stderr, 1);
	}
}

int main(int argc, char **argv)
{
	pid_t euid = geteuid();
	int i;
	cap_t caps;
	cap_value_t v[CAP_LAST_CAP+1];

	checkargs(argc, argv);

	if (getuid() != euid)
		setuid(euid);

	if (opt_icaps)
	{
		caps = cap_from_text(opt_icaps);
		if (NULL == caps) {
			fprintf(stderr,
			        "%s: invalid capability string: %s\n"
			        "(see cap_from_text(3) for info)\n",
			        argv[0], opt_icaps);
			exit(1);
		}
	}
	else
	{
		caps = cap_get_proc();
		for (i = 0; i <= CAP_LAST_CAP; ++i)
			v[i] = i;

		if (cap_set_flag(caps, CAP_INHERITABLE, CAP_LAST_CAP+1, v, CAP_SET) != 0) {
			exit(-1);
		}
	}
	if (!caps)
		abort();
	if (0 != cap_set_proc(caps)) {
		perror("cap_set_proc");
		fprintf(stderr, "%s: failed to set process capabilities\n",
		        argv[0]);
		exit(1);
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

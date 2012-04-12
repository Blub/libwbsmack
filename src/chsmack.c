#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/xattr.h>
#include <linux/xattr.h>

#define SMACK_SIZE 24

static struct option lopts[] = {
	{ "help",      no_argument,       NULL, 'h' },
	{ "access",    required_argument, NULL, 'a' },
	{ "exec",      required_argument, NULL, 'x' },
	{ "mmap",      required_argument, NULL, 'm' },
	{ "transmute", required_argument, NULL, 't' },
	{ NULL, 0, NULL, 0}
};

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s [options] files...\n", arg0);
	fprintf(target,
	"options:\n"
	"  -h, --help            show this help message\n"
	"label options:\n"
	"  -a, --access=label    access label\n"
	"  -e, --exec=label      execute-as label\n"
	"  -m, --mmap=label      mmap label\n"
	"  -t, --transmute=bool  transmute label\n"
	"'bool': 1 | 0 | true | false | TRUE | FALSE\n"
	);
	exit(exitstatus);
}

static const char *opt_access = NULL;
static const char *opt_exec = NULL;
static const char *opt_mmap = NULL;
static const char *opt_transmute = NULL;
static int opt_argstart = 1;

static void checkargs(int argc, char **argv)
{
	int o, lind = 0;
	while ((o = getopt_long(argc, argv, "+ha:e:m:t:", lopts, &lind)) != -1)
	{
		switch (o)
		{
			case 'h':
				usage(argv[0], stdout, 0);
				break;
			case 'a':
				opt_access = optarg;
				break;
			case 'e':
				opt_exec = optarg;
				break;
			case 'm':
				opt_mmap = optarg;
				break;
			case 't':
				opt_transmute = optarg;
				break;
		};
	}
	if (argc - optind < 1) {
		fprintf(stderr, "%s: files mising\n", argv[0]);
		usage(argv[0], stderr, 1);
	}

	opt_argstart = optind;
}

int main(int argc, char **argv)
{
	int i, rc;
	int justprint = 0;
	//int addfile = 0;
	const char *trans = NULL;
	char buffer[SMACK_SIZE];
	checkargs(argc, argv);

	if (opt_access && strlen(opt_access) >= SMACK_SIZE) {
		fprintf(stderr, "%s: Access label `%s' exceeds %d characters.\n",
		        argv[0], opt_access, SMACK_SIZE-1);
		exit(1);
	}
	if (opt_exec && strlen(opt_exec) >= SMACK_SIZE) {
		fprintf(stderr, "%s: Execute label `%s' exceeds %d characters.\n",
		        argv[0], opt_exec, SMACK_SIZE-1);
		exit(1);
	}
	if (opt_mmap && strlen(opt_mmap) >= SMACK_SIZE) {
		fprintf(stderr, "%s: MMAP label `%s' exceeds %d characters.\n",
		        argv[0], opt_mmap, SMACK_SIZE-1);
		exit(1);
	}

	if (opt_transmute) {
		if (!strcmp(opt_transmute, "1") ||
		    !strcmp(opt_transmute, "true") ||
		    !strcmp(opt_transmute, "TRUE") ||
		    !strcmp(opt_transmute, "t") ||
		    !strcmp(opt_transmute, "T") )
		{
			trans = "TRUE";
		}
		else if (!strcmp(opt_transmute, "0") ||
		         !strcmp(opt_transmute, "false") ||
		         !strcmp(opt_transmute, "FALSE") ||
		         !strcmp(opt_transmute, "f") ||
		         !strcmp(opt_transmute, "F") )
		{
			trans = "FALSE";
		}
		else
		{
			fprintf(stderr, "%s: invalid transmute argument `%s'\n",
			        argv[0], opt_transmute);
			usage(argv[0], stderr, 1);
			exit(1);
		}
	}

	justprint = !opt_access && !opt_exec && !opt_mmap && !opt_transmute;
	//addfile = (opt_argstart+1 != argc);
	for (i = opt_argstart; i < argc; ++i)
	{
		if (justprint) {
			//if (addfile)
			printf("%s", argv[i]);
			rc = lgetxattr(argv[i], XATTR_NAME_SMACK, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" access=\"%s\"", buffer);
			}
			rc = lgetxattr(argv[i], XATTR_NAME_SMACKEXEC, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" execute=\"%s\"", buffer);
			}
			rc = lgetxattr(argv[i], XATTR_NAME_SMACKMMAP, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" mmap=\"%s\"", buffer);
			}
			rc = lgetxattr(argv[i], XATTR_NAME_SMACKTRANSMUTE, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" transmute=\"%s\"", buffer);
			}
			printf("\n");
			continue;
		}
		if (opt_access) {
			rc = lsetxattr(argv[i], XATTR_NAME_SMACK,
			               opt_access, strlen(opt_access) + 1, 0);
			if (rc < 0)
				perror(argv[i]);
		}
		if (opt_exec) {
			rc = lsetxattr(argv[i], XATTR_NAME_SMACKEXEC,
			               opt_exec, strlen(opt_exec) + 1, 0);
			if (rc < 0)
				perror(argv[i]);
		}
		if (opt_mmap) {
			rc = lsetxattr(argv[i], XATTR_NAME_SMACKMMAP,
			               opt_mmap, strlen(opt_mmap) + 1, 0);
			if (rc < 0)
				perror(argv[i]);
		}
		if (trans) {
			rc = lsetxattr(argv[i], XATTR_NAME_SMACKTRANSMUTE,
			               trans, strlen(trans) + 1, 0);
			if (rc < 0)
				perror(argv[i]);
		}
	}
	exit(0);
	return 0;
}

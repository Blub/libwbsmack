#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <attr/xattr.h>
#include <linux/xattr.h>
#include <errno.h>

#define SMACK_SIZE 24

static struct option lopts[] = {
	{ "help",      no_argument,       NULL, 'h' },
	{ "link",      no_argument,       NULL, 'l' },
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
	"  -l, --link            do not follow symlinks.\n"
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
static int opt_link = 0;

static void checkargs(int argc, char **argv)
{
	int o, lind = 0;
	while ((o = getopt_long(argc, argv, "+hla:e:m:t:", lopts, &lind)) != -1)
	{
		switch (o)
		{
			case 'h':
				usage(argv[0], stdout, 0);
				break;
			case 'l':
				opt_link = 1;
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

	ssize_t (*getxa)(const char*, const char*, void*, size_t) = &getxattr;
	int     (*setxa)(const char*, const char*, const void*, size_t, int) = &setxattr;
	int     (*remxa)(const char*, const char*) = &removexattr;

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
			trans = NULL; // gets removed in this case "FALSE";
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

	if (opt_link) {
		getxa = &lgetxattr;
		setxa = &lsetxattr;
		remxa = &lremovexattr;
	}
	for (i = opt_argstart; i < argc; ++i)
	{
		if (justprint) {
			//if (addfile)
			printf("%s", argv[i]);
			rc = getxa(argv[i], XATTR_NAME_SMACK, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" access=\"%s\"", buffer);
			}
			rc = getxa(argv[i], XATTR_NAME_SMACKEXEC, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" execute=\"%s\"", buffer);
			}
			rc = getxa(argv[i], XATTR_NAME_SMACKMMAP, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" mmap=\"%s\"", buffer);
			}
			rc = getxa(argv[i], XATTR_NAME_SMACKTRANSMUTE, buffer,
			               sizeof(buffer));
			if (rc > 0) {
				buffer[rc] = 0;
				printf(" transmute=\"%s\"", buffer);
			}
			printf("\n");
			continue;
		}
		if (opt_access) {
			rc = setxa(argv[i], XATTR_NAME_SMACK,
			               opt_access, strlen(opt_access), 0);
			if (rc < 0)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACK,
				        strerror(errno));
		}
		if (opt_exec) {
			rc = setxa(argv[i], XATTR_NAME_SMACKEXEC,
			               opt_exec, strlen(opt_exec), 0);
			if (rc < 0)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKEXEC,
				        strerror(errno));
		}
		if (opt_mmap) {
			rc = setxa(argv[i], XATTR_NAME_SMACKMMAP,
			               opt_mmap, strlen(opt_mmap), 0);
			if (rc < 0)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKMMAP,
				        strerror(errno));
		}
		if (opt_transmute) {
			if (trans)
				rc = setxa(argv[i], XATTR_NAME_SMACKTRANSMUTE,
				           trans, strlen(trans), 0);
			else
				rc = remxa(argv[i], XATTR_NAME_SMACKTRANSMUTE);
			if (rc < 0 && (trans || errno != ENOATTR))
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKTRANSMUTE,
				        strerror(errno));
		}
	}
	exit(0);
	return 0;
}

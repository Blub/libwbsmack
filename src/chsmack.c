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

#include "smack.h"

static struct option lopts[] = {
	{ "help",      no_argument,       NULL, 'h' },
	{ "link",      no_argument,       NULL, 'l' },
	{ "store",     no_argument,       NULL, 'S' },
	{ "restore",   no_argument,       NULL, 'R' },

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
	"  -l, --link            do not follow symlinks\n"
	"  -S, --store           print in a format usable as input\n"
	"  -R, --restore         read labels from stdin\n"
	"The -S and -R options only allow 1 file per run\n"
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
static int opt_store = 0;
static int opt_restore = 0;
static int opt_argstart = 1;
static int opt_link = 0;

static int _opt_singlefile = 0;
static int _opt_nolabels = 0;
static int _opt_gotlabel = 0;
static int _opt_readlabels = 0;

static void checkargs(int argc, char **argv)
{
	int o, lind = 0;
	while ((o = getopt_long(argc, argv, "+hlSRa:e:m:t:", lopts, &lind)) != -1)
	{
		switch (o)
		{
			case 'h':
				usage(argv[0], stdout, 0);
				break;
			case 'l':
				opt_link = 1;
				break;
			case 'S':
				opt_store = 1;
				_opt_singlefile = 1;
				_opt_nolabels = 1;
				break;
			case 'R':
				opt_restore = 1;
				_opt_singlefile = 1;
				_opt_nolabels = 1;
				_opt_readlabels = 1;
				break;
			case 'a':
				opt_access = optarg;
				_opt_gotlabel = 1;
				break;
			case 'e':
				opt_exec = optarg;
				_opt_gotlabel = 1;
				break;
			case 'm':
				opt_mmap = optarg;
				_opt_gotlabel = 1;
				break;
			case 't':
				opt_transmute = optarg;
				_opt_gotlabel = 1;
				break;
		};
	}
	if (argc - optind < 1) {
		fprintf(stderr, "%s: files mising\n", argv[0]);
		usage(argv[0], stderr, 1);
	}

	opt_argstart = optind;

	if (_opt_nolabels && _opt_gotlabel) {
		fprintf(stderr, "%s: one or more of the provided parameters "
		        "exclude each other.\n", argv[0]);
		exit(1);
	}

	if (_opt_singlefile && (argc - optind) != 1) {
		fprintf(stderr, "%s: the provided options can only be used "
		        "on one file at a time.\n", argv[0]);
		exit(1);
	}
}

static char *dupcontent(char *from)
{
	char *end;
	while (*from && *from != '=')
		++from;
	if (*from != '=')
		return NULL;
	++from;
	end = from;
	while (*end &&
	       *end != '\r' &&
	       *end != '\n' &&
	       *end != '\t' &&
	       *end != '\f' &&
	       *end != ' ')
		++end;
	*end = 0;
	if (!*from)
		return NULL;
	return strdup(from);
}

static void readlabels(const char *arg0)
{
	char *line = NULL;
	size_t len = 0;

	while (getline(&line, &len, stdin) != -1)
	{
		if(!opt_access && 0 == strncmp(line, "access=", 7)) {
			opt_access = dupcontent(line);
		}
		else if (!opt_exec && 0 == strncmp(line, "execute=", 8)) {
			opt_exec = dupcontent(line);
		}
		else if (!opt_mmap && 0 == strncmp(line, "mmap=", 5)) {
			opt_mmap = dupcontent(line);
		}
		else if (!opt_transmute && 0 == strncmp(line, "transmute=", 10)) {
			opt_transmute = dupcontent(line);
		}
		else {
			fprintf(stderr, "%s: invalid input line: `%s'\n",
			        arg0, line);
			free(line);
			line = NULL;
			len = 0;
			exit(1);
		}
	}
	if (line)
		free(line);
}

static inline void showlabel(int rc, const char *label, const char *content)
{
	if (opt_store) {
		if (rc > 0)
			printf("%s=%s\n", label, content);
		else
			printf("%s=\n", label);
	} else if (rc > 0)
		printf(" %s=\"%s\"", label, content);
}

int main(int argc, char **argv)
{
	int i, rc;
	int justprint = 0;
	//int addfile = 0;
	const char *trans = NULL;
	char buffer[SMACK_LONGLABEL];

	ssize_t (*getxa)(const char*, const char*, void*, size_t) = &getxattr;
	int     (*setxa)(const char*, const char*, const void*, size_t, int) = &setxattr;
	int     (*remxa)(const char*, const char*) = &removexattr;

	checkargs(argc, argv);
	if (_opt_readlabels)
		readlabels(argv[0]);

	if (opt_access && strlen(opt_access) >= SMACK_LONGLABEL-1) {
		fprintf(stderr, "%s: Access label `%s' exceeds %d characters.\n",
		        argv[0], opt_access, SMACK_LONGLABEL-1);
		exit(1);
	}
	if (opt_exec && strlen(opt_exec) >= SMACK_LONGLABEL-1) {
		fprintf(stderr, "%s: Execute label `%s' exceeds %d characters.\n",
		        argv[0], opt_exec, SMACK_SIZE-1);
		exit(1);
	}
	if (opt_mmap && strlen(opt_mmap) >= SMACK_LONGLABEL-1) {
		fprintf(stderr, "%s: MMAP label `%s' exceeds %d characters.\n",
		        argv[0], opt_mmap, SMACK_LONGLABEL-1);
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

	if (opt_link) {
		getxa = &lgetxattr;
		setxa = &lsetxattr;
		remxa = &lremovexattr;
	}
	for (i = opt_argstart; i < argc; ++i)
	{
		if (justprint) {
			if (!opt_store)
				printf("%s:", argv[i]);
			rc = getxa(argv[i], XATTR_NAME_SMACK, buffer,
			               sizeof(buffer));
			if (rc >= 0)
				buffer[rc] = 0;
			showlabel(rc, "access", buffer);

			rc = getxa(argv[i], XATTR_NAME_SMACKEXEC, buffer,
			               sizeof(buffer));
			if (rc >= 0)
				buffer[rc] = 0;
			showlabel(rc, "execute", buffer);

			rc = getxa(argv[i], XATTR_NAME_SMACKMMAP, buffer,
			               sizeof(buffer));
			if (rc >= 0)
				buffer[rc] = 0;
			showlabel(rc, "mmap", buffer);

			rc = getxa(argv[i], XATTR_NAME_SMACKTRANSMUTE, buffer,
			               sizeof(buffer));
			if (rc >= 0)
				buffer[rc] = 0;
			showlabel(rc, "transmute", buffer);
			if (!opt_store)
				printf("\n");
			continue;
		}
		if (opt_access && *opt_access)
		{
			rc = setxa(argv[i], XATTR_NAME_SMACK,
			               opt_access, strlen(opt_access), 0);
			if (rc < 0)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACK,
				        strerror(errno));
		}
		else if (opt_restore || (opt_access && !*opt_access))
		{
			rc = remxa(argv[i], XATTR_NAME_SMACK);
			if (rc < 0 && errno != ENOATTR)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACK,
				        strerror(errno));
		}
		if (opt_exec && *opt_exec)
		{
			rc = setxa(argv[i], XATTR_NAME_SMACKEXEC,
			               opt_exec, strlen(opt_exec), 0);
			if (rc < 0)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKEXEC,
				        strerror(errno));
		}
		else if (opt_restore || (opt_exec && !*opt_exec))
		{
			rc = remxa(argv[i], XATTR_NAME_SMACKEXEC);
			if (rc < 0 && errno != ENOATTR)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKEXEC,
				        strerror(errno));
		}
		if (opt_mmap && *opt_mmap)
		{
			rc = setxa(argv[i], XATTR_NAME_SMACKMMAP,
			               opt_mmap, strlen(opt_mmap), 0);
			if (rc < 0)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKMMAP,
				        strerror(errno));
		}
		else if (opt_restore || (opt_mmap && !*opt_mmap))
		{
			rc = remxa(argv[i], XATTR_NAME_SMACKMMAP);
			if (rc < 0 && errno != ENOATTR)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKMMAP,
				        strerror(errno));
		}
		if (opt_transmute)
		{
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
		else if (opt_restore)
		{
			rc = remxa(argv[i], XATTR_NAME_SMACKTRANSMUTE);
			if (rc < 0 && errno != ENOATTR)
				fprintf(stderr, "%s (%s): %s\n",
				        argv[i], XATTR_NAME_SMACKTRANSMUTE,
				        strerror(errno));
		}
	}
	exit(0);
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <linux/capability.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
#include <sys/xattr.h>
#include <linux/xattr.h>

#include "smack.h"

static void usage(const char *arg0, FILE *target, int exitstatus)
{
	fprintf(target, "usage: %s [options] command [parameters...]\n", arg0);
	fprintf(target,
		"options:\n"
		"    -l label       Use this label rather than the file's label.\n"
	       );
	exit(exitstatus);
}

static char *findExe(const char *bin)
{
	char *path;
	char *tok;
	char *fullpath;
	size_t fplen;
	size_t binlen;
	size_t toklen;
	if (strchr(bin, '/') != NULL)
		return strdup(bin);

	path = getenv("PATH");
	if (!path)
		return strdup(bin);
	path = strdup(path);

	binlen = strlen(bin);

	fplen = 256;
	fullpath = (char*)malloc(fplen);
	for (tok = strtok(path, ":"); tok; tok = strtok(NULL, ":"))
	{
		toklen = strlen(tok);
		if (binlen + 1 + toklen >= fplen) {
			fplen *= 2;
			fullpath = realloc(fullpath, fplen);
		}
		memcpy(fullpath, tok, toklen);
		while (toklen && fullpath[toklen-1] == '/')
			--toklen;
		fullpath[toklen] = '/';
		memcpy(fullpath + toklen + 1, bin, binlen);
		fullpath[toklen + 1 + binlen] = 0;
		if (access(fullpath, X_OK) == 0) {
			free(path);
			return fullpath;
		}
	}
	free(path);
	free(fullpath);
	return NULL;
}

static int cantransition(const char *arg0, const char *mylabel, const char *label)
{
	int contains;
	struct passwd *pwd;
	struct smackentry *myentry;

	if (!smackaccess(mylabel, label, "x")) {
		fprintf(stderr, "%s: do not have execute permissions for '%s'\n",
		        arg0, label);
		return 0;
	}

	if (!smackchecktrans(mylabel, label)) {
		fprintf(stderr, "%s: transition '%s' -> '%s' is not allowed\n",
		        arg0, mylabel, label);
		return 0;
	}

	pwd = getpwuid(getuid());
	if (!pwd) {
		perror("getpw");
		return 0;
	}
	myentry = opensmackentry(pwd->pw_name);
	if (!myentry) {
		fprintf(stderr, "%s: No smack entry found in smack-users file.\n", arg0);
		return 0;
	}
	contains = smackentrycontains(myentry, label);
	closesmackentry(myentry);
	if (!contains) {
		fprintf(stderr, "%s: '%s' is not a valid label for this user.\n", arg0, label);
		return 0;
	}
	return 1;
}

int main(int argc, char **argv, char **envp)
{
	char *binary;
	char **arglist;
	char *label = NULL;
	char filelabel[SMACK_SIZE];
	char mylabel[SMACK_SIZE];
	int o;
	int lind = 0;
	//int binfd;

	static struct option lopts[] = {
		{ "help",  no_argument,       NULL, 'h' },
		{ "label", required_argument, NULL, 'l' },

		{ NULL, 0, NULL, 0}
	};

	while ( (o = getopt_long(argc, argv, "+hl:", lopts, &lind)) != -1) {
		switch (o) {
			case 'h':
				usage(argv[0], stdout, 0);
				break;
			case 'l':
				if (label) {
					free(label);
					usage(argv[0], stderr, 1);
				}
				label = strdup(optarg);
				break;
			default:
				if (label)
					free(label);
				usage(argv[0], stderr, 1);
				break;
		}
	}

	if (getsmack(mylabel, sizeof(mylabel)) == -1) {
		fprintf(stderr, "%s: failed to get smack label\n", argv[0]);
		if (label) free(label);
		exit(1);
	}

	if (label) {
		if (strlen(label) >= SMACK_SIZE) {
			fprintf(stderr, "%s: label '%s' exceeds length of %d\n",
			        argv[0], label, SMACK_SIZE-1);
			exit(1);
		}
		if (!cantransition(argv[0], mylabel, label)) {
			free(label);
			exit(1);
		}
	}

	// Find the executable in $PATH:
	binary = findExe(argv[optind]);
	if (!binary) {
		fprintf(stderr, "%s: Cannot find %s\n", argv[0], argv[optind]);
		if (label) free(label);
		exit(1);
	}

	/*
	// Open first, then use the open fd to check AS WELL as exec using fexecve
	// to avoid race conditions between checks.
	binfd = open(binary, O_RDONLY);
	if (binfd < 0) {
		perror("open");
		free(binary);
		if (label) free(label);
		exit(1);
	}
	*/

	// Get the program's label
	if (getxattr(binary, XATTR_NAME_SMACK, filelabel, sizeof(filelabel)) == -1) {
		perror("getxattr");
		if (label) free(label);
		free(binary);
		exit(1);
	}

	// We need to be able to execute the program,
	// and the label we execute the program AS must also be allowed to execute
	// it.
	if (!label) {
		if (!cantransition(argv[0], mylabel, filelabel)) {
			free(binary);
			exit(1);
		}
	} else {
		if (!smackaccess(label, filelabel, "x")) {
			fprintf(stderr, "%s: '%s' cannot execute '%s'\n", argv[0], label, filelabel);
			free(binary);
			exit(1);
		}

	}

	// Set smack label:
	if (label) {
		if (setsmack(label) == -1) {
			perror("setsmack");
			free(label);
			exit(1);
		}
		free(label);
	} else if (setsmack(filelabel) == -1) {
		perror("setsmack");
		exit(1);
	}

	{
		pid_t pid = fork();
		if (pid) {
			int st;
			free(binary);
			while (waitpid(pid, &st, 0) != pid) {
				perror("wait");
			}
			exit(WEXITSTATUS(st));
		}
		if (getuid() != geteuid())
			seteuid(getuid());
	}


	// Prepare parameters:
	arglist = (char**)malloc(sizeof(arglist[0]) * (argc - optind + 1));
	//arglist[0] = binary;
	arglist[0] = argv[optind];
	for (o = optind + 1; o < argc; ++o) {
		arglist[o - optind] = argv[o];
	}
	arglist[o - optind] = NULL;

	execve(binary, arglist, envp);
	//fexecve(binfd, arglist, envp);
	perror("exec");
	exit(1);
	return 1;
}

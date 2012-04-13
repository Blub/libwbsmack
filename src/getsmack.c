#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "smack.h"

int getsmack(char *label, size_t n)
{
	char rlabel[SMACK_LONGLABEL];

	int rc;
	int fd;

	fd = open(SMACK_PROCSELFATTRCURRENT, O_RDONLY);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	rc = read(fd, rlabel, sizeof(rlabel)-1);
	close(fd);
	if (rc < 0) {
		perror("read");
		return -1;
	}

	if (rc == 0) {
		errno = ENOSYS;
		return -1;
	}

	// rc contains size of the label (nr of bytes read)
	if (n < rc + 1) {
		errno = ENOMEM;
		return -1;
	}

	memcpy(label, rlabel, rc);
	label[rc] = 0;
	return rc;
}

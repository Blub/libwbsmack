#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "smack.h"

int setsmack(const char *label)
{
	int fd, rc;

	fd = open(SMACK_PROCSELFATTRCURRENT, O_WRONLY);
	if(fd <= 0)
		return -1;

	rc = write(fd, label, strlen(label) + 1);
	if (rc < 0) {
		int eno = errno;
		close(fd);
		errno = eno;
		return -1;
	}
	// fsync should not be required, but the manpage
	// says close() doesn't actually flush.
	fsync(fd);
	close(fd);
	errno = 0;
	return 0;
}

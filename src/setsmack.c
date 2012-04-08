#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "smack.h"

int setsmack(const char *label)
{
	int fd, rc;

	fd = open(SMACK_PROCSELFATTRCURRENT, O_WRONLY);
	if(fd <= 0)
		return -1;

	rc = write(fd, label, strlen(label) + 1);
	if (rc < 0) {
		close(fd);
		return -1;
	}
	// fsync should not be required, but the manpage
	// says close() doesn't actually flush.
	fsync(fd);
	close(fd);
	return 0;
}

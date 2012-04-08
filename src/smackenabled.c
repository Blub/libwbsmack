#include <stdio.h>
#include <string.h>

int smackenabled(void) {
	FILE *fp;
	size_t len;
	char *buf = NULL;
	int enabled = 0;

	fp = fopen("/proc/mounts", "r");
	if (!fp)
		return 0;

	while (getline(&buf, &len, fp) != -1)
	{
		// device mountpoint filesystem
		char *tmp;

		tmp = strchr(buf, ' ');
		if (!tmp)
			break;

		tmp = strchr(tmp+1, ' ');
		if (!tmp)
			break;

		if (!strncmp(tmp + 1, "smackfs ", 8))
		{
			enabled = 1;
			break;
		}
	}

	if (buf)
		free((void*)buf);

	if(fp) {
		fclose(fp);
		fp = NULL;
	}

	return enabled;
}

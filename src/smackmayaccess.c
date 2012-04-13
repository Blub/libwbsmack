/*
 * Copyright (C) 2007 Casey Schaufler <casey@schaufler-ca.com>
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, version 2.
 *
 * Authors:
 *	Casey Schaufler <casey@schaufler-ca.com>
 *
 * Returns 1 on success, 0 on failure, -1 on error.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "smack.h"

static void setaccess(int may, char *out)
{
	out[0] = (may & SMACK_MAY_R) ? 'r' : '-';
	out[1] = (may & SMACK_MAY_W) ? 'w' : '-';
	out[2] = (may & SMACK_MAY_X) ? 'x' : '-';
	out[3] = (may & SMACK_MAY_A) ? 'a' : '-';
	out[4] = (may & SMACK_MAY_T) ? 't' : '-';
}

int smackmayaccess2(const char *subject, const char *object, int may)
{
	char *buffer;
	char *accesspart;
	size_t sublen, objlen, bufsize;
	char reply[16];

	int fd;

	errno = 0;

	sublen = strlen(subject);
	if (sublen >= SMACK_LONGLABEL-1) {
		errno = EINVAL;
		return 0;
	}
	objlen = strlen(object);
	if (objlen >= SMACK_LONGLABEL-1) {
		errno = EINVAL;
		return 0;
	}

	bufsize = sublen + 1 + objlen + 1 + 5;
	buffer = (char*)malloc(bufsize);

	strcpy(buffer, subject);
	strcpy(buffer + sublen + 1, object);
	accesspart = buffer + sublen + 1 + objlen + 1;
	setaccess(may, accesspart);

	fd = open(SMACK_ACCESS, O_RDWR);
	if (fd < 0) {
		int eno = errno;
		free(buffer);
		errno = eno;
		if (errno == ENOENT)
			errno = ENOSYS;
		return 0;
	}

	if (bufsize != write(fd, buffer, bufsize)) {
		free(buffer);
		errno = EINVAL;
		close(fd);
		return 0;
	}
	free(buffer);

	if (read(fd, &reply, sizeof(reply)) < 1) {
		close(fd);
		errno = 0;
		return 0;
	}
	close(fd);

	return (reply[0] == '1') ? 1 : 0;
}

int smackmayaccess(const char *subject, const char *object, int may)
{
	int rc;
	union {
		struct {
			char subject[SMACK_SIZE];
			char object[SMACK_SIZE];
			char access[SMACK_ACCESSLEN];
		} data;
		char result;
	} data;

	int fd;
	int i;

	rc = smackmayaccess2(subject, object, may);
	if (!errno)
		return rc;

	errno = 0;

	memset(&data, 0, sizeof(data));
	for (i = 0; i < 5; ++i)
		data.data.access[i] = '-';

	if (strlen(subject) >= SMACK_SIZE-1)
	{
		errno = EINVAL;
		return 0;
	}

	if (strlen(object) >= SMACK_SIZE-1)
	{
		errno = EINVAL;
		return 0;
	}

	strncpy(data.data.subject, subject, sizeof(data.data.subject));
	strncpy(data.data.object, object, sizeof(data.data.object));
	setaccess(may, data.data.access);

	fd = open(SMACK_ACCESS, O_RDWR);
	if (fd < 0) {
		if (errno == ENOENT)
			errno = ENOSYS;
		return 0;
	}

	if (sizeof(data) != write(fd, &data, sizeof(data))) {
		errno = EINVAL;
		close(fd);
		return 0;
	}

	if (read(fd, &data, sizeof(data)) < 1) {
		close(fd);
		errno = 0;
		return 0;
	}
	close(fd);

	return (data.result == '1') ? 1 : 0;
}

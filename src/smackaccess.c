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

static int parseaccess(char *access, char *out)
{
	int i;

	out[0] = '-';
	out[1] = '-';
	out[2] = '-';
	out[3] = '-';
	out[4] = '-';
	for (i = 0; access[i] != '\0'; i++) {
		switch (access[i]) {
		case '-':
			break;
		case 'R':
		case 'r':
			out[0] = 'r';
			break;
		case 'W':
		case 'w':
			out[1] = 'w';
			break;
		case 'X':
		case 'x':
			out[2] = 'x';
			break;
		case 'A':
		case 'a':
			out[3] = 'a';
			break;
		case 'T':
		case 't':
			out[4] = 't';
			break;
		default:
			return -1;
		}
	}
	return 0;
}

int smackaccess2(const char *subject, const char *object, char *access)
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

	if (parseaccess(access, accesspart) != 0) {
		free(buffer);
		errno = EINVAL;
		return 0;
	}

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

int smackaccess(const char *subject, const char *object, char *access)
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

	rc = smackaccess2(subject, object, access);
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
	if (parseaccess(access, data.data.access) != 0) {
		errno = EINVAL;
		return 0;
	}

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

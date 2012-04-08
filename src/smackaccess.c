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

int smackaccess(char *subject, char *object, char *access)
{
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

	for (i = 0; i < sizeof(data.data.access); ++i)
		data.data.access[i] = '-';

	for (i = 0; access[i] != '\0'; i++) {
		switch (access[i]) {
		case '-':
			break;
		case 'R':
		case 'r':
			data.data.access[0] = 'r';
			break;
		case 'W':
		case 'w':
			data.data.access[1] = 'w';
			break;
		case 'X':
		case 'x':
			data.data.access[2] = 'x';
			break;
		case 'A':
		case 'a':
			data.data.access[3] = 'a';
			break;
		case 'T':
		case 't':
			data.data.access[4] = 't';
			break;
		default:
			return -1;
		}
	}

	fd = open(SMACK_ACCESS, O_RDWR);
	if (fd < 0)
		return 0;

	if (sizeof(data) != write(fd, &data, sizeof(data))) {
		errno = EINVAL;
		close(fd);
		return 0;
	}

	if (read(fd, &data, sizeof(data)) < 1) {
		errno = 0;
		close(fd);
		return 0;
	}
	close(fd);

	return (data.result == '1');
}

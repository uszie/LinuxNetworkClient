/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * Routines for LIBMOUNT_TEST_PROGRAM
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifndef LIBMOUNT_TEST_PROGRAM
#define LIBMOUNT_TEST_PROGRAM
#endif

#include "mountP.h"

int mnt_run_test(struct mtest *tests, int argc, char *argv[])
{
	int rc = -1;
	struct mtest *ts;

	assert(tests);
	assert(argc);
	assert(argv);

	if (argc < 2 ||
	    strcmp(argv[1], "--help") == 0 ||
	    strcmp(argv[1], "-h") == 0)
		goto usage;

	mnt_init_debug(0);

	for (ts = tests; ts->name; ts++) {
		if (strcmp(ts->name, argv[1]) == 0) {
			printf("TEST (%s):\n", ts->name + 2);
			rc = ts->body(ts, argc - 1, argv + 1);
			if (rc)
				printf("FAILED [rc=%d]", rc);
			else
				printf("DONE");
			printf(" (%s)\n", ts->name + 2);
			break;
		}
	}

	if (rc == -1 && ts->name == NULL)
		goto usage;

	return rc == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
usage:
	printf("\nUsage:\n\t%s <test> [testoptions]\nTests:\n",
			program_invocation_short_name);
	for (ts = tests; ts->name; ts++) {
		printf("\t%-15s", ts->name);
		if (ts->usage)
			printf(" %s\n", ts->usage);
	}
	printf("\n");
	return EXIT_FAILURE;
}


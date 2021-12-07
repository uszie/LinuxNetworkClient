/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mountP.h"

/**
 * mnt_new_miter:
 * @direction: MNT_INTER_{FOR,BACK}WARD direction
 *
 * Returns newly allocated generic libmount iterator.
 */
miter *mnt_new_iter(int direction)
{
	miter *itr = calloc(1, sizeof(struct _miter));
	if (!itr)
		return NULL;
	itr->direction = direction;
	return itr;
}

/**
 * mnt_free_miter:
 * @itr: iterator pointer
 *
 * Deallocates iterator.
 */
void mnt_free_iter(miter *itr)
{
	free(itr);
}

/**
 * mnt_reset_miter:
 * @itr: iterator pointer
 * @direction: MNT_INTER_{FOR,BACK}WARD iterator direction
 *
 * Resets iterator.
 */
void mnt_reset_iter(miter *itr, int direction)
{
	assert(itr);

	if (itr) {
		memset(itr, 0, sizeof(struct _miter));
		itr->direction = direction;
	}
}


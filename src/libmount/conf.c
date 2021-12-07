/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <stdlib.h>

#include "mountP.h"
#include "pathnames.h"

#ifdef CONFIG_LIBMOUNT_DEBUG
int libmount_debug_mask;

void mnt_init_debug(int mask)
{
	if (libmount_debug_mask & DEBUG_INIT)
		return;
	if (!mask)
	{
		char *str = mnt_getenv_safe("LIBMOUNT_DEBUG");
		if (str)
			libmount_debug_mask = strtoul(str, 0, 0);
	} else
		libmount_debug_mask = mask;

	if (libmount_debug_mask)
		printf("libmount: debug mask set to 0x%04x.\n",
				libmount_debug_mask);
	libmount_debug_mask |= DEBUG_INIT;
}
#endif

/*
 * TODO:
 *	MTAB_PATH=<path>
 *	FSTAB_PATH=<path>
 *
 *	# userspace specific options (on systems without mtab)
 *	# default /var/run/libmount/otab
 *	OTAB_PATH=<path>
 *
 *	# canonicalize source and target paths for mount(2) and mtab ?
 *	CANONICALIZE_PATHS=<yes|not>
 *
 * mconfig *mnt_new_config(const char *path);
 * void mnt_free_config(mconfig *mc);
 *
 * mnt_config_get_{mtab,fstab,otab}_path(mconfig *mc);
 *
 */

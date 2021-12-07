/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * This is low-level API to modify /etc/mtab. See also high-level 'mrequest' API.
 *
 * Note that /etc/mtab is obsolete.
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>
#include <limits.h>

#include "pathnames.h"
#include "nls.h"

#include "mountP.h"

/**
 * mnt_mtab_remove_line:
 * @target: mountpoint
 *
 * Removes (umount) a line from /etc/mtab. You have to lock the file before you
 * call mnt_mtab_* functions. See also mnt_lock().
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mtab_remove_line(const char *target)
{
	mfile	*mf;
	mentry	*ent;
	int	rc = 0;

	mf = mnt_new_mfile_parse(_PATH_MOUNTED);
	if (!mf)
		return -1;

	ent = mnt_mfile_find_target(mf, target, MNT_ITER_BACKWARD);
	if (ent) {
		mnt_mfile_remove_entry(mf, ent);
		rc = mnt_rewrite_mfile(mf);
	}

	mnt_free_mfile(mf);
	return rc;
}

/**
 * mnt_mtab_append_line:
 * @source: device name
 * @target: mountpoint
 * @fstype: filesystem type (name)
 * @options: mount potions
 *
 * Appends (mount) a line to /etc/mtab. You have to lock the file
 * before you call mnt_mtab_* functions. See also mnt_lock().
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mtab_append_line(const char *source, const char *target,
		         const char *fstype, const char *options)
{
	FILE *f;
	int rc;

	f = fopen(_PATH_MOUNTED, "a");
	if (!f)
		return -1;

	rc = mnt_fprintf_line(f, MNT_MFILE_PRINTFMT,
			source, target, fstype, options, 0, 0);
	fclose(f);
	return rc;
}

/**
 * mnt_mtab_modify_options:
 * @target: mountpoint
 * @options: new mount potions
 *
 * Replaces (MS_REMOUNT) @options for mountpoint @target. You have to lock the
 * file before you call mnt_mtab_* functions. See also mnt_lock().
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mtab_modify_options(const char *target, const char *options)
{
	mfile	*mf;
	mentry	*ent;
	int	rc = 0;

	mf = mnt_new_mfile_parse(_PATH_MOUNTED);
	if (!mf)
		return -1;

	ent = mnt_mfile_find_target(mf, target, MNT_ITER_BACKWARD);
	if (ent) {
		rc = mnt_entry_set_optstr(ent, options);
		if (rc == 0)
			rc = mnt_rewrite_mfile(mf);
	}

	mnt_free_mfile(mf);
	return rc;
}

/**
 * mnt_mtab_modify_target:
 * @old_target: old mountpoint
 * @new_target: new mountpoint
 *
 * Changes (MS_MOVE) mountpoint. You have to lock the file before
 * you call mnt_mtab_* functions. See also mnt_lock().
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mtab_modify_target(const char *old_target, const char *new_target)
{
	mfile	*mf;
	mentry	*ent;
	int	rc = 0;

	mf = mnt_new_mfile_parse(_PATH_MOUNTED);
	if (!mf)
		return -1;

	ent = mnt_mfile_find_target(mf, old_target, MNT_ITER_BACKWARD);
	if (ent) {
		rc = mnt_entry_set_target(ent, new_target);
		if (rc == 0)
			rc = mnt_rewrite_mfile(mf);
	}

	mnt_free_mfile(mf);
	return rc;
}


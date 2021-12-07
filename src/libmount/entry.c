/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * The mfile entry is representation of one line in a mfile (fstab / mtab /
 * mountinfo). It's not recommended to use this low-level API to store runtime
 * information that you want to use for mount(2) syscall. Please, high-level
 * "mrequest" API.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <blkid/blkid.h>

#include "nls.h"
#include "mountP.h"

/**
 * mnt_new_entry:
 *
 * Returns newly allocated mfile entry.
 */
mentry *mnt_new_entry(void)
{
	mentry *ent = calloc(1, sizeof(struct _mentry));
	if (!ent)
		return NULL;

	INIT_LIST_HEAD(&ent->ents);
	return ent;
}

/**
 * mnt_free_entry:
 * @ent: entry pointer
 *
 * Deallocates the entry.
 */
void mnt_free_entry(mentry *ent)
{
	if (!ent)
		return;
	list_del(&ent->ents);

	free(ent->source);
	free(ent->tagname);
	free(ent->tagval);
	free(ent->mntroot);
	free(ent->target);
	free(ent->fstype);
	free(ent->optstr);
	free(ent->optvfs);
	free(ent->optfs);

	free(ent);
}

/**
 * mnt_entry_get_srcpath:
 * @ent: mfile (fstab/mtab/mountinfo) entry
 *
 * The mount "source path" is:
 *	- a directory for 'bind' mounts
 *	- a device name for standard mounts
 *	- NULL when path is not set (for example when TAG
 *	  (LABEL/UUID) is defined only)
 *
 * See also mnt_entry_get_tag() and mnt_entry_get_source().
 *
 * Returns mount "source" path or NULL in case of error or when the path
 * is not defined.
 *
 */
const char *mnt_entry_get_srcpath(mentry *ent)
{
	assert(ent);
	if (!ent)
		return NULL;

	if (!ent->mntroot) {
		/* fstab-like entry */
		if (ent->tagname)
			return NULL;	/* the source contains a "NAME=value" */
		return ent->source;
	}
	/* mountinfo-like entry */
	if (strcmp(ent->mntroot, "/"))	/* "bind mount" or so */
		return ent->mntroot;

	return ent->source;
}

/**
 * @ent: mfile (fstab/mtab/mountinfo) entry
 *
 * Returns mount "source". Note that the source could be unparsed TAG
 * (LABEL/UUID).  See also mnt_entry_get_srcpath() and mnt_entry_get_tag().
 */
const char *mnt_entry_get_source(mentry *ent)
{
	return ent ? ent->source : NULL;
}

/* Used by parser mfile ONLY */
int __mnt_entry_set_source(mentry *ent, char *source)
{
	assert(ent);

	if (!source)
		return -1;

	if (strchr(source, '=')) {
		char *name, *val;

		if (blkid_parse_tag_string(source, &name, &val) != 0)
			return -1;

		ent->tagval = val;
		ent->tagname = name;
	}

	ent->source = source;
	return 0;
}

/**
 * mnt_entry_set_source:
 * @ent: mfile entry
 * @source: new source
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_entry_set_source(mentry *ent, const char *source)
{
	char *p;

	if (!ent && !source)
		return -1;

	p = strdup(source);
	if (!p)
		return -1;

	free(ent->tagval);
	free(ent->tagname);
	free(ent->source);
	ent->tagval = ent->tagname = ent->source = NULL;

	return __mnt_entry_set_source(ent, p);
}

/**
 * mnt_entry_is_bind:
 * @ent: mfile (fstab/mtab/mountinfo) entry
 *
 * Return 1 when the @ent describes a "bind mount" or 0.
 */
int mnt_entry_is_bind(mentry *ent)
{
	if (ent->mntroot && strcmp(ent->mntroot, "/"))
		/* data from "mountinfo", the mount root is not "/",
		 * so it has to be "bind" :-) */
		return 1;

	if (mnt_entry_get_option(ent, "bind", NULL, NULL) == 0 ||
	    mnt_entry_get_option(ent, "rbind", NULL, NULL) == 0)
		/* fstab/mtab uses a "bind" or "rbind" option */
		return 1;

	return 0;
}

/**
 * mnt_entry_get_tag:
 * @ent: mfile (fstab/mtab/mountinfo) entry
 * @name: returns pointer to NAME string
 * @value: returns pointer to VALUE string
 *
 * "TAG" is NAME=VALUE (e.g. LABEL=foo)
 *
 * The TAG is the first column in the fstab or mtab file. The TAG
 * or "srcpath" has to be always set for all entries.
 *
 * See also mnt_entry_get_source().
 *
 * Example:
 *	char *src;
 *	mentry *ent = mnt_mfile_find_target(mf, "/home");
 *
 *	if (!ent)
 *		goto err;
 *
 *	src = mnt_entry_get_srcpath(ent);
 *	if (!src) {
 *		char *tag, *val;
 *		if (mnt_entry_get_tag(ent, &tag, &val) == 0)
 *			printf("%s: %s\n", tag, val);	// * LABEL or UUID
 *	} else
 *		printf("device: %s\n", src);		// device or bind path
 *
 * Returns 0 on success or -1 in case that a TAG is not defined.
 */
int mnt_entry_get_tag(mentry *ent, const char **name, const char **value)
{
	if (ent == NULL || !ent->tagname)
		return -1;
	if (name)
		*name = ent->tagname;
	if (value)
		*value = ent->tagval;
	return 0;
}

/**
 * mnt_entry_get_target:
 * @ent: mfile entry pointer
 *
 * Returns pointer to mountpoint path or NULL
 */
const char *mnt_entry_get_target(mentry *ent)
{
	assert(ent);
	return ent ? ent->target : NULL;
}

/**
 * mnt_entry_set_target:
 * @ent: mfile entry
 * @target: mountpoint
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_entry_set_target(mentry *ent, const char *target)
{
	char *p;

	assert(ent);

	if (!ent || !target)
		return -1;

	p = strdup(target);
	if (!p)
		return -1;
	free(ent->target);
	ent->target = p;

	return 0;
}

/**
 * mnt_entry_get_fstype:
 * @ent: mfile entry pointer
 *
 * Returns pointer to filesystem type.
 */
const char *mnt_entry_get_fstype(mentry *ent)
{
	assert(ent);
	return ent ? ent->fstype : NULL;
}

/* Used by mfile parser only */
int __mnt_entry_set_fstype(mentry *ent, char *fstype)
{
	assert(ent);

	if (!fstype)
		return -1;

	ent->fstype = fstype;
	ent->flags &= ~MNT_ENTRY_PSEUDOFS;
	ent->flags &= ~MNT_ENTRY_NETFS;

	/* save info about pseudo filesystems */
	if (mnt_fstype_is_pseudofs(ent->fstype))
		ent->flags |= MNT_ENTRY_PSEUDOFS;
	else if (mnt_fstype_is_netfs(ent->fstype))
		ent->flags |= MNT_ENTRY_NETFS;

	return 0;
}

/**
 * mnt_entry_set_fstype:
 * @ent: mfile entry
 * @fstype: filesystem type
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_entry_set_fstype(mentry *ent, const char *fstype)
{
	char *p;

	if (!ent || !fstype)
		return -1;

	p = strdup(fstype);
	if (!p)
		return -1;
	free(ent->fstype);

	return __mnt_entry_set_fstype(ent, p);
}

/**
 * mnt_entry_get_optstr:
 * @ent: mfile entry pointer
 *
 * Returns pointer to mount option string with all options (FS and VFS)
 */
const char *mnt_entry_get_optstr(mentry *ent)
{
	assert(ent);
	return ent ? ent->optstr : NULL;
}

/**
 * mnt_entry_set_optstr:
 * @ent: mfile entry
 * @optstr: options string
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_entry_set_optstr(mentry *ent, const char *optstr)
{
	assert(ent);

	if (!ent || !optstr)
		return -1;
	free(ent->optstr);
	free(ent->optfs);
	free(ent->optvfs);
	ent->optfs = ent->optvfs = NULL;

	ent->optstr = strdup(optstr);

	return ent->optstr ? 0 : -1;
}

/**
 * mnt_entry_get_optfs:
 * @ent: mfile entry pointer
 *
 * This function works for "mountinfo" files only.
 *
 * Returns pointer to superblock (fs-depend) mount option string or NULL.
 */
const char *mnt_entry_get_optfs(mentry *ent)
{
	assert(ent);
	return ent ? ent->optfs : NULL;
}

/**
 * mnt_entry_get_optvfs:
 * @ent: mfile entry pointer
 *
 * This function works for "mountinfo" files only.
 *
 * Returns pointer to fs-independent (VFS) mount option string or NULL.
 */
const char *mnt_entry_get_optvfs(mentry *ent)
{
	assert(ent);
	return ent ? ent->optvfs : NULL;
}


/**
 * mnt_entry_get_freq:
 * @ent: mfile entry pointer
 *
 * Returns "dump frequency in days".
 */
int mnt_entry_get_freq(mentry *ent)
{
	assert(ent);
	return ent ? ent->freq : 0;
}

/**
 * mnt_entry_set_freq:
 * @ent: mfile entry pointer
 * @freq: dump frequency in days
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_entry_set_freq(mentry *ent, int freq)
{
	assert(ent);

	if (!ent)
		return -1;
	ent->freq = freq;
	return 0;
}

/**
 * mnt_entry_get_passno:
 * @ent: mfile entry pointer
 *
 * Returns "pass number on parallel fsck".
 */
int mnt_entry_get_passno(mentry *ent)
{
	assert(ent);
	return ent ? ent->passno: 0;
}

/**
 * mnt_entry_set_passno:
 * @ent: mfile entry pointer
 * @passno: pass number
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_entry_set_passno(mentry *ent, int passno)
{
	assert(ent);

	if (!ent)
		return -1;
	ent->passno = passno;
	return 0;
}

/**
 * mnt_entry_get_option:
 * @ent: mfile entry pointer
 * @name: option name
 * @value: returns pointer to the begin of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of options value or 0
 *
 * Returns 0 on success, 1 when not found the @name or -1 in case of error.
 */
int mnt_entry_get_option(mentry *ent, const char *name,
		const char **value, size_t *valsz)
{
	const char *optstr = mnt_entry_get_optstr(ent);
	return optstr ? mnt_optstr_find_option(optstr, name, value, valsz) : 1;
}


/* Unfortunately the classical Unix /etc/mtab and /etc/fstab
   do not handle directory names containing spaces.
   Here we mangle them, replacing a space by \040.
   What do other Unices do? */

static unsigned char need_escaping[] = { ' ', '\t', '\n', '\\' };

static char *mangle(const char *s)
{
	char *ss, *sp;
	int n;

	n = strlen(s);
	ss = sp = malloc(4*n+1);
	if (!sp)
		return NULL;
	while(1) {
		for (n = 0; n < sizeof(need_escaping); n++) {
			if (*s == need_escaping[n]) {
				*sp++ = '\\';
				*sp++ = '0' + ((*s & 0300) >> 6);
				*sp++ = '0' + ((*s & 070) >> 3);
				*sp++ = '0' + (*s & 07);
				goto next;
			}
		}
		*sp++ = *s;
		if (*s == 0)
			break;
	next:
		s++;
	}
	return ss;
}

/**
 * mnt_fprintf_line:
 * @f: FILE
 * @fmt: printf-like format string (see MNT_MFILE_PRINTFMT)
 * @source: (spec) device name or tag=value
 * @target: mountpoint
 * @fstype: filesystem type
 * @options: mount options
 * @freq: dump frequency in days
 * @passno: pass number on parallel fsck
 *
 * Returns return value from fprintf().
 */
int mnt_fprintf_line(	FILE *f,
			const char *fmt,
			const char *source,
			const char *target,
			const char *fstype,
			const char *options,
			int freq,
			int passno)
{
	char *m1 = NULL, *m2 = NULL, *m3 = NULL, *m4 = NULL;
	int rc = -1;

	if (!f || !fmt || !source || !target || !fstype || !options)
		return -1;

	m1 = mangle(source);
	m2 = mangle(target);
	m3 = mangle(fstype);
	m4 = mangle(options);

	if (!m1 || !m2 || !m3 || !m4)
		goto done;

	rc = fprintf(f, fmt, m1, m2, m3, m4, freq, passno);
done:
	free(m1);
	free(m2);
	free(m3);
	free(m4);

	return rc;
}

/**
 * mnt_fprintf_ent:
 * @f: FILE
 * @fmt: printf-like format string (see MNT_MFILE_PRINTFMT)
 * @ent: mfile entry
 *
 * Returns return value from fprintf().
 */
int mnt_fprintf_entry(FILE *f, const char *fmt, mentry *ent)
{
	assert(ent);
	assert(f);
	assert(fmt);

	if (!ent || !f)
		return -1;

	return mnt_fprintf_line(f, fmt,
			mnt_entry_get_source(ent),
			mnt_entry_get_target(ent),
			mnt_entry_get_fstype(ent),
			mnt_entry_get_optstr(ent),
			mnt_entry_get_freq(ent),
			mnt_entry_get_passno(ent));
}


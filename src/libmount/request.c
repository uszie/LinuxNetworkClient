/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 *
 * The mrequest struct is core of the libmount -- it describes whole mount
 * operation and it's glue between all low-level parts of the library.
 *
 *
 *                   $ mount /dev/source /mnt/target -o foo,bar
 *                                  |       |           |
 *  [libblkid]    +------------+    |       |           |
 *      ||        |  mrequest  |    |       |           |
 *      ||        |            |    |       |           |
 * +----------+   |  -source   | <--'       |           |
 * |   mfile  |   |  -target   | <----------'       +--------+
 * |-fstab    |<=>|  -optstr   | <------------------| mopts  |
 * |-mountinfo|   |  -fstype   |                    | parser |
 * +----------+   |            |                    +--------+
 *                +------------+
 *                    |    ^
 *     +------+       |    |       +-------+
 *     | mtab | <-----'    `-----> | mtask |
 *     +------+  mlock             +-------+
 *                                   | | |
 *                                   | | `-----> [u]mount(2) syscall
 *                                   | |
 *                                   | '-------> exec(/sbin/mount.<type>)
 *                                   |
 *                                   '--?-?-?--> dlopen(/lib/mount/<type>.so)
 *
 * Typical mount:
 *
 *  1/ set spec
 *  2/ apply data from /etc/fstab
 *  3/ append mount options from command line
 *  4/ prepare mount options
 *  5/ check users permissions (append user= option)
 *  6/ initialize device (convert TAGs to devname, init loopdevs, ...)
 *  7/ recognize FS type
 *  8/ call mount(2) or /sbin/mount.<type>
 *  9/ initialize lock
 * 10/ lock, update, unlock mtab
 *
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <blkid/blkid.h>
#include <sys/fcntl.h>

#include "nls.h"
#include "mountP.h"
#include "pathnames.h"

/*
 * Mount request
 * -- this basic description of whole mount/umount/swap/swapon operation.
 */
struct _mrequest
{
	int	task_type;	/* mount, umount, swapon, .. */

	int	restricted;	/* root or not? */

	char	*source;	/* TAG, devname, path, .. */
	char	*target;	/* mountpoint */
	char	*fstype;	/* file system type */
	char	*optstr;	/* options */

	char	*srcpath;	/* the final device name or NULL */

	mfile	*fstab;		/* fstab */
	mentry	*fstab_ent;	/* fstab entry */
	mfile	*mtab;		/* mtab/mountinfo */
	mentry	*mtab_ent;	/* mtab entry (for MS_REMOUNT or umount) */

	mopts	*opts;		/* parsed options container or NULL */

	int	mountflags;		/* final mount(2) flags */
	char	*mountdata;	/* final mount(2) data, string or binary data */
	size_t	mountdata_len;	/* mountdata length */

	char	*mtab_optstr;	/* options for mtab or NULL */

	mlock	*lock;		/* global mtab lock */
	mpcache *pcache;	/* paths cache */

	blkid_probe probe;	/* prober or NULL */
	int	dev_fd;

	char	errbuf[BUFSIZ];	/* buffer for error messages */
	int	errsv;		/* saved errno */

	int	flags;
};

/* request flags (TODO) */
#define MNT_REQUEST_NOMTAB		(1 << 1)
#define MNT_REQUEST_FAKE		(1 << 2)
#define MNT_REQUEST_SLOPPY		(1 << 3)
#define MNT_REQUEST_VERBOSE		(1 << 4)
#define MNT_REQUEST_FORK		(1 << 5)
#define MNT_REQUEST_NOHELPERS		(1 << 6)
#define MNT_REQUEST_DELOOP		(1 << 7)
#define MNT_REQUEST_LAZY		(1 << 8)
#define MNT_REQUEST_FORCE		(1 << 9)
#define MNT_REQUEST_ASSEMBLED		(1 << 10)
#define MNT_REQUEST_PRIVATE_OPTS	(1 << 11)
#define MNT_REQUEST_FSTAB_FAILED	(1 << 12)
#define MNT_REQUEST_MTAB_FAILED		(1 << 13)
#define MNT_REQUEST_HAS_LOOPDEV		(1 << 15)
#define MNT_REQUEST_PROBE_FAILED	(1 << 16)

/**
 * mnt_new_mrequest:
 * @tasktype: MNT_TASK_* enum (default is MNT_TASK_MOUNT)
 *
 * Returns newly allocated mrequest struct.
 */
mrequest *mnt_new_request(int tasktype)
{
	mrequest *req;
	uid_t ruid, euid;

	req = calloc(1, sizeof(*req));
	if (!req)
		return NULL;

	req->task_type = tasktype;
	req->dev_fd = -1;

	ruid = getuid();
	euid = geteuid();

	/* if we're really root and aren't running setuid */
	if (((uid_t)0 == ruid) && (ruid == euid))
		req->restricted = 0;
	else
		req->restricted = 1;
	return req;
}

/**
 * mnt_free_mrequest:
 * @req: mrequest pointer
 *
 * Deallocates mrequest struct.
 */
void mnt_free_request(mrequest *req)
{
	if (!req)
		return;

	free(req->source);
	free(req->target);
	free(req->fstype);
	free(req->optstr);
	free(req->mtab_optstr);
	free(req->mountdata);
	free(req->srcpath);

	mnt_free_mfile(req->fstab);
	mnt_free_mfile(req->mtab);

	if (req->flags & MNT_REQUEST_PRIVATE_OPTS)
		mnt_free_opts(req->opts);

	if (req->probe)
		blkid_free_probe(req->probe);
	if (req->dev_fd != -1)
		close(req->dev_fd);

	free(req);
}

/**
 * mnt_request_is_restricted:
 * @req: mrequest pointer
 *
 * Return 1 if permissions are restricted, 0 when executed by root.
 */
int mnt_request_is_restricted(mrequest *req)
{
	return req && req->restricted == 0 ? 0 : 1;
}

static void mnt_request_reset_errbuf(mrequest *req)
{
	if (!req)
		return;

	*req->errbuf = '\0';
	req->errsv = 0;
}

/**
 * mnt_request_strerror:
 * @req: mrequest pointer
 * @buf: error message buffer
 * @buflen: size of  buffer
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_strerror(mrequest *req, char *buf, size_t buflen)
{
	if (!req || !buf || !buflen || (!req->errsv && !*req->errbuf))
		return -1;

	if (req->errsv && !*req->errbuf) {
		size_t sz = *req->errbuf ? strlen(req->errbuf) : 0;
		char *p = req->errbuf + sz;

		if (sz) {
			*p++ = ':';
			*p++ = ' ';
		}
		strerror_r(req->errsv, p, sizeof(req->errbuf) - sz);
		/* don't compose the error message next time */
		req->errsv = 0;
	}
	strncpy(buf, req->errbuf, buflen);
	buf[buflen - 1] = '\0';
	return 0;
}

/**
 * mnt_request_set_source:
 * @req: mrequest pointer
 * @source: new source
 *
 * Deallocates old source string and copies a new @source into request.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_source(mrequest *req, const char *source)
{
	if (!req)
		return -1;
	free(req->source);
	req->source = NULL;
	if (source) {
		req->source = strdup(source);
		if (!req->source)
			return -1;
	}
	return 0;
}

/**
 * mnt_request_get_source:
 * @req: mrequest pointer
 *
 * Returns source or NULL.
 */
const char *mnt_request_get_source(mrequest *req)
{
	return req ? req->source : NULL;
}

/*
 * this is not exported by API, you have to call mnt_request_apply_fstab()
 * or mnt_request_assemble_srcpath() to setup the path.
 */
static int mnt_request_set_srcpath(mrequest *req, const char *srcpath)
{
	if (!req)
		return -1;
	free(req->srcpath);
	req->srcpath = NULL;
	if (srcpath) {
		req->srcpath = strdup(srcpath);
		if (!req->srcpath)
			return -1;
	}
	return 0;
}


/**
 * mnt_request_set_target:
 * @req: mrequest pointer
 * @target: new target
 *
 * Deallocates old target string and copies a new @target into request.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_target(mrequest *req, const char *target)
{
	if (!req)
		return -1;
	free(req->target);
	req->target = NULL;
	if (target) {
		req->target = strdup(target);
		if (!req->target)
			return -1;
	}
	return 0;
}

/**
 * mnt_request_get_target:
 * @req: mrequest pointer
 *
 * Returns target (mountpoint) or NULL.
 */
const char *mnt_request_get_target(mrequest *req)
{
	return req ? req->target : NULL;
}

/**
 * mnt_request_set_fstype:
 * @req: mrequest pointer
 * @fstype: new fstype
 *
 * Deallocates old fstype string and copies a new @fstype into request.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_fstype(mrequest *req, const char *fstype)
{
	if (!req)
		return -1;
	free(req->fstype);
	req->fstype = NULL;
	if (fstype) {
		req->fstype = strdup(fstype);
		if (!req->fstype)
			return -1;
	}
	return 0;
}

/**
 * mnt_request_get_fstype:
 * @req: mrequest pointer
 *
 * Returns filesystem type or NULL.
 */
const char *mnt_request_get_fstype(mrequest *req)
{
	return req ? req->fstype : NULL;
}

/**
 * mnt_request_set_optstr:
 * @req: mrequest pointer
 * @optstr: new mount options string
 *
 * Deallocates old options string and copies a new @optstr into request. The
 * library will automatically parse this string when 'mountdata', 'mountflags' or
 * 'opts' are not defined.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_optstr(mrequest *req, const char *optstr)
{
	if (!req)
		return -1;
	free(req->optstr);
	req->optstr = NULL;
	if (optstr) {
		req->optstr = strdup(optstr);
		if (!req->optstr)
			return -1;
	}
	return 0;
}

/**
 * mnt_request_get_optstr:
 * @req: mrequest pointer
 *
 * Returns options string or NULL.
 */
const char *mnt_request_get_optstr(mrequest *req)
{
	return req ? req->optstr : NULL;
}

/**
 * mnt_request_append_optstr:
 * @req: mrequest pointer
 * @optstr: mount options string
 *
 * Appends @optstr to the request. Typical use:
 *
 *	mnt_request_apply_fstab(req);
 *	mnt_request_append_optstr(req, cmdline_options);
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_append_optstr(mrequest *req, const char *optstr)
{
	char *p;

	if (!req)
		return -1;
	if (!optstr)
		return -1;
	if (!req->optstr)
		return mnt_request_set_optstr(req, optstr);

	p = req->optstr;
	req->optstr = mnt_strconcat3(req->optstr, ",", optstr);
	if (!req->optstr) {
		req->optstr = p;	/* to avoid leak-after-failed-realloc */
		return -1;
	}
	return 0;
}

/**
 * mnt_request_set_mtab_optstr:
 * @req: mrequest pointer
 * @mtab_optstr: new options string for mtab
 *
 * Deallocates old mtab options string and copies a new @mtab_optstr into
 * request.  The library will use this options always for mtab independently on
 * "optstr" or "opts" setting.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_mtab_optstr(mrequest *req, const char *mtab_optstr)
{
	if (!req)
		return -1;
	free(req->mtab_optstr);
	req->mtab_optstr = NULL;
	if (mtab_optstr) {
		req->mtab_optstr = strdup(mtab_optstr);
		if (!req->mtab_optstr)
			return -1;
	}
	return 0;
}

/**
 * mnt_request_get_mtab_optstr:
 * @req: mrequest pointer
 *
 * Returns mtab options string or NULL.
 */
const char *mnt_request_get_mtab_optstr(mrequest *req)
{
	return req ? req->mtab_optstr : NULL;
}

/**
 * mnt_request_set_mountflags:
 * @req: mrequest pointer
 * @mountflags: new mount(2) flags
 *
 * Setups directly mount(2) flags. The library will ignore "optstr" and "opts"
 * for mount(2) syscall. The mountflags has the highest priority for mount(2) calls.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_mountflags(mrequest *req, int mountflags)
{
	if (!req)
		return -1;
	req->mountflags = mountflags;
	return 0;
}

/**
 * mnt_request_get_mountflags:
 * @req: mrequest pointer
 *
 * Returns mount flags (@flags from mount(2)) or NULL.
 */
int mnt_request_get_mountflags(mrequest *req)
{
	return req ? req->mountflags : 0;
}

/**
 * mnt_request_set_mountdata:
 * @req: mrequest pointer
 * @mountdata: new raw mount(2) data
 * @len: size of mountdata
 *
 * Setups directly mount(2) data. The library will ignore 'optstr' and 'opts'
 * for mount(2) syscall. The mountdata has the highest priority for mount(2) calls.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_mountdata(mrequest *req, const char *mountdata, size_t len)
{
	if (!req)
		return -1;
	free(req->mountdata);
	req->mountdata = NULL;
	req->mountdata_len = 0;

	if (mountdata && len) {
		req->mountdata = malloc(len);
		if (!req->mountdata)
			return -1;
		req->mountdata_len = len;
		memcpy(req->mountdata, mountdata, len);
	}
	return 0;
}

/**
 * mnt_request_get_mountdata:
 * @req: mrequest pointer
 *
 * Returns mount data (@data from mount(2)) or NULL.
 */
const void *mnt_request_get_mountdata(mrequest *req)
{
	return req ? req->mountdata : NULL;
}

/**
 * mnt_request_set_opts:
 * @req: mrequest pointer
 * @opts: options container
 *
 * Sets reference to you @opts container. The library will generate mountflags, mountdata
 * and mtab optionts string from this container. The 'opts' setting has higher
 * priority then 'optstr' setting, but lower than 'mountflags', 'mountdata' and 'mtab_optsr'.
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_request_set_opts(mrequest *req, mopts *opts)
{
	if (!req)
		return -1;
	if (req->flags & MNT_REQUEST_PRIVATE_OPTS)
		mnt_free_opts(req->opts);
	req->opts = opts;
	return 0;
}

/**
 * mnt_request_get_opts:
 * @req: mrequest pointer
 *
 * Returns options container or NULL.
 */
mopts *mnt_request_get_opts(mrequest *req)
{
	return req ? req->opts : NULL;
}

/**
 * mnt_request_set_lock:
 * @req: mrequest pointer
 * @lock: mlock or NULL
 *
 * Sets reference (or creates new if @lock is NULL) to lock object.
 *
 * The lock is exported by API cause the mnt_unlock() function
 * has to be called from signal handler or before unexpected exit() call.
 *
 * See also mnt_lock(), mnt_unlock() and mnt_new_lock()
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_request_set_lock(mrequest *req, mlock *lock)
{
	if (!req)
		return -1;
	if (req->lock)
		return 0;
	if (!lock)
		lock = mnt_new_lock(NULL, 0);
	if (!lock)
		return -1;
	req->lock = lock;
	return 0;
}

/**
 * mnt_request_get_lock:
 * @req: mrequest pointer
 *
 * Returns lock.
 */
mlock *mnt_request_get_lock(mrequest *req)
{
	return req ? req->lock : NULL;
}

/**
 * mnt_request_set_pcache:
 * @req: mrequest pointer
 * @cache: mpcache or NULL
 *
 * Sets reference (or creates new if @cache is NULL) to cache object. The cache
 * is reusable in more requests.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_request_set_pcache(mrequest *req, mpcache *cache)
{
	if (!req)
		return -1;
	if (req->pcache)
		return 0;
	if (!cache)
		cache = mnt_new_pcache();
	if (!cache)
		return -1;
	req->pcache = cache;
	return 0;
}

/**
 * mnt_request_get_pcache:
 * @req: mrequest pointer
 *
 * Returns pcache.
 */
mpcache *mnt_request_get_pcache(mrequest *req)
{
	return req ? req->pcache : NULL;
}

static int mnt_request_tag_to_srcpath(mrequest *req,
		const char *tag, const char *value)
{
	char *t = NULL, *v = NULL;
	int rc = -1;

	assert(req);
	assert(tag);

	if (req->srcpath || !tag)
		return 0;

	if (!value && !blkid_parse_tag_string(tag, &t, &v)) {
		tag = t;
		value = v;
	}

	if (value) {
		char *p = mnt_resolve_tag(tag, value, req->pcache);

		if (p) {
			mnt_request_set_srcpath(req, p);
			if (!req->pcache)
				free(p);
			rc = 0;
		}
	}

	free(t);
	free(v);
	return rc;
}


static mfile *mnt_request_read_mfile(mrequest *req,
			int failflag, const char *filename)
{
	mfile *mf = NULL;

	mnt_request_reset_errbuf(req);

	if (req->flags & failflag)
		return NULL;

	mf = mnt_new_mfile(filename);
	if (!mf)
		goto saverr;

	if (mnt_parse_mfile(mf) != 0) {
		if (!mnt_mfile_get_nerrs(mf)) /* non-parse error */
			goto saverr;

		mnt_mfile_strerror(mf, req->errbuf, sizeof(req->errbuf));
		goto failed;
	}

	if (req->pcache)
		mnt_mfile_set_pcache(mf, req->pcache);
	return mf;
saverr:
	req->errsv = errno;
	strncpy(req->errbuf, filename, sizeof(req->errbuf));
	req->errbuf[sizeof(req->errbuf)-1] = '\0';
failed:
	mnt_free_mfile(mf);
	req->flags |= failflag;
	return NULL;
}

static int mnt_request_read_fstab(mrequest *req)
{
	if (req->fstab)
		return 0;

	req->fstab = mnt_request_read_mfile(req,
			MNT_REQUEST_FSTAB_FAILED, _PATH_MNTTAB);
	return req->fstab ? 0 : -1;
}

static int mnt_request_read_mtab(mrequest *req)
{
	if (req->mtab)
		return 0;

	/* TODO: we need to read /proc/self/mountinfo when mtab does
	 *       not exist or it's link to /proc/mounts
	 */

	req->mtab = mnt_request_read_mfile(req,
			MNT_REQUEST_MTAB_FAILED, _PATH_MOUNTED);
	return req->mtab ? 0 : -1;
}

/**
 * mnt_request_get_fstab_entry:
 * @req: mrequest pointer
 * @ent: returns fstab entry or NULL when not found.
 *
 * Look ups /etc/fstab by mount source or target.
 *
 * Returns 0 on success or -1 in case of error (usully parse errors).
 */
int mnt_request_get_fstab_entry(mrequest *req, mentry **ent)
{
	if (!req || !ent)
		goto error;

	if (req->fstab_ent)
		goto done;

	if (mnt_request_read_fstab(req) != 0)
		goto error;

	if (req->source)
		req->fstab_ent = mnt_mfile_find_source(req->fstab,
					req->source, MNT_ITER_FORWARD);

	if (!req->fstab_ent && req->target)
		req->fstab_ent = mnt_mfile_find_target(req->fstab,
					req->target, MNT_ITER_FORWARD);
done:
	*ent = req->fstab_ent;
	return 0;
error:
	return -1;
}

/**
 * mnt_request_get_mtab_entry:
 * @req: mrequest pointer
 * @ent: returns mtab entry or NULL when not found.
 *
 * Look ups /etc/mtab or /proc/self/mountinfo by mount target or source.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_request_get_mtab_entry(mrequest *req, mentry **ent)
{
	if (!req || !ent)
		goto error;

	if (req->mtab_ent)
		goto done;

	if (mnt_request_read_mtab(req) != 0)
		goto error;

	if (req->target)
		req->mtab_ent = mnt_mfile_find_target(req->mtab,
					req->target, MNT_ITER_BACKWARD);

	if (!req->mtab_ent && req->source)
		req->mtab_ent = mnt_mfile_find_source(req->mtab,
					req->source, MNT_ITER_BACKWARD);
done:
	*ent = req->mtab_ent;
	return 0;
error:
	return -1;
}

/**
 * mnt_request_apply_fstab:
 * @req: mrequest pointer
 *
 * Sets source, target, fstype and optstr from /etc/fstab to the request. This
 * function does not overwrites already in the request defined information.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_request_apply_fstab(mrequest *req)
{
	mentry *ent;

	if (mnt_request_get_fstab_entry(req, &ent) != 0)
		return -1;
	if (!ent)
		return 0;	/* not found */
	if (!req->source)
		mnt_request_set_source(req, mnt_entry_get_source(ent));
	if (!req->srcpath) {
		const char *p = mnt_entry_get_srcpath(ent);

		if (p)
			mnt_request_set_srcpath(req, p);
		else {
			const char *v, *t;

			if (!mnt_entry_get_tag(ent, &t, &v))
				mnt_request_tag_to_srcpath(req, t, v);
		}
	}
	if (!req->target)
		mnt_request_set_target(req, mnt_entry_get_target(ent));
	if (!req->fstype)
		mnt_request_set_fstype(req, mnt_entry_get_fstype(ent));
	if (!req->optstr)
		mnt_request_set_optstr(req, mnt_entry_get_optstr(ent));
	return 0;
}

/**
 * mnt_request_apply_mtab:
 * @req: mrequest pointer
 *
 * Sets source, target, fstype and optstr from /etc/mtab or
 * /proc/self/mountinfo to the request. This function does not overwrites
 * already in the request defined information.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_request_apply_mtab(mrequest *req)
{
	mentry *ent;

	if (mnt_request_get_mtab_entry(req, &ent) != 0)
		return -1;
	if (!ent)
		return 0;	/* not found */
	if (!req->source)
		mnt_request_set_source(req, mnt_entry_get_source(ent));
	if (!req->srcpath)
		mnt_request_set_srcpath(req, mnt_entry_get_srcpath(ent));
	if (!req->target)
		mnt_request_set_target(req, mnt_entry_get_target(ent));
	if (!req->fstype)
		mnt_request_set_fstype(req, mnt_entry_get_fstype(ent));
	if (!req->optstr)
		mnt_request_set_optstr(req, mnt_entry_get_optstr(ent));
	return 0;
}

static char *mnt_request_setup_loopdev(mrequest *req, const char *loopdev)
{
	/* TODO */
	fprintf(stderr, "libmount: loop device initialization is unsupported yet\n");
	return NULL;
}

/**
 * mnt_request_assemble_srcpath:
 * @req: mrequest pointer
 *
 * Evaluates request and sets srcpath (device name). This function depends on:
 *
 *   - properly defined source, see also:
 *     mnt_request_set_source() or mnt_request_apply_fstab()
 *
 *   - already parsed options (required for loopdev initialization), see also:
 *     mnt_request_assemble_options()
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_request_assemble_srcpath(mrequest *req)
{
	moption *op = NULL;

	if (!req)
		return -1;
	if (!req->source)
		return -1;

	/* TODO setup cdrom speed */

	/* source is TAG */
	if (!req->srcpath && !mnt_request_tag_to_srcpath(req, req->source, NULL))
		return 0;

	/* source is LOOP device */
	if (req->opts)
		op = mnt_opts_get_option(req->opts, "loop");
	if (op) {
		const char *opval = NULL;
		char *loopdev;

		mnt_option_get_string(op, &opval);

		loopdev = mnt_request_setup_loopdev(req, opval);
		if (!loopdev)
			return -1;
		mnt_request_set_srcpath(req, loopdev);
		if (!opval)
			free(loopdev); /* generated by mnt_request_setup_loopdev() */
		return 0;
	}

	/* let use source as a srcpath without any conversion */
	if (!req->srcpath)
		mnt_request_set_srcpath(req, req->source);
	return 0;
}

/* calls low-level probing stuff from libblkid
 * -- this function does not deallocate the req->probe, you have to call
 *    mnt_free_request().
 */
static int mnt_request_probe_device(mrequest *req)
{
	assert(req);
	assert(req->srcpath);

	if (req->flags & MNT_REQUEST_PROBE_FAILED)
		return -1;	/* already failed */

	if (req->probe && blkid_probe_numof_values(req->probe) > 0)
		return 0;	/* already succesfully probed */

	mnt_request_reset_errbuf(req);

	if (req->dev_fd < 0)
		req->dev_fd = mnt_open_device(
				req->srcpath,
				O_RDONLY | O_CLOEXEC);
	if (req->dev_fd < 0)
		goto error;
	if (!req->probe)
		req->probe = blkid_new_probe();
	if (!req->probe)
		goto error;

	/* TODO: use offsett and size for loopdevs */
	if (blkid_probe_set_device(req->probe, req->dev_fd, 0, 0))
		goto error;

	if (blkid_probe_set_request(req->probe, BLKID_PROBREQ_LABEL |
			BLKID_PROBREQ_UUID | BLKID_PROBREQ_TYPE))
		goto error;
	if (blkid_do_safeprobe(req->probe))
		goto error;

	return 0;
error:
	req->flags |= MNT_REQUEST_PROBE_FAILED;
        req->errsv = errno;
        strncpy(req->errbuf, req->srcpath, sizeof(req->errbuf));
        req->errbuf[sizeof(req->errbuf)-1] = '\0';

	return -1;
}

static int is_existing_file(const char *s)
{
	struct stat statbuf;

	assert(s);
	return (stat(s, &statbuf) == 0);
}

/**
 * mnt_request_assemble_fstype:
 * @req: mrequest pointer
 *
 * This function depends on:
 *
 * - properly defined source, see also:
 *   mnt_request_set_source() or mnt_request_apply_fstab()
 *
 * - properly initialized device (required for loopdevs only), see also:
 *   mnt_request_assemble_srcpath()
 *
 * Note that it isn't error when FS type can't be detected -- the fallback
 * solution is to read type from /{etc,proc}/filesystems and try mount(2). This
 * fallback is implemented by mtask API.
 *
 * Returns 0 on success or -1 in case of error. Open/read errors are stored and
 * avaialble by mnt_request_strerror().
 */
int mnt_request_assemble_fstype(mrequest *req)
{
	const char *type = NULL;

	if (!req)
		return -1;
	if (req->fstype && strcmp(req->fstype, "auto"))
		return 0;

	mnt_request_reset_errbuf(req);

	if (!req->srcpath)
		return -1;

	mnt_request_set_fstype(req, NULL);	/* remove "auto" */

	/* probe FS on the device */
	if (is_existing_file(req->srcpath)) {
		if (blkid_probe_numof_values(req->probe) <= 0)
			mnt_request_probe_device(req);
		if (req->probe)
			blkid_probe_lookup_value(req->probe, "TYPE", &type, NULL);
	/* assume NFS */
	} else if (strchr(req->srcpath, ':') != NULL) {
		type = "nfs";
		DBG(DEBUG_REQUEST, fprintf(stderr,
			"libmount: no type was given - "
			"I'll assume nfs because of the colon\n"));
	/* assume CIFS */
	} else if(!strncmp(req->srcpath, "//", 2)) {
		type = "cifs";
		DBG(DEBUG_REQUEST, fprintf(stderr,
			"libmount: no type was given - "
			"I'll assume cifs because of the // prefix\n"));
	}

	if (type)
		mnt_request_set_fstype(req, type);

	return req->fstype ? 0 : -1;
}

/**
 * mnt_request_assemble_options:
 * @req: mrequest pointe
 *
 * Parses "optstr" if "opts" is not set yet, see also
 * mnt_request_set_optstr() and mnt_request_set_opts()
 *
 * Returns 0 on success or -1 in case of error. See mnt_request_strerror()
 * for parse errors.
 */
int mnt_request_assemble_options(mrequest *req)
{
	if (!req)
		return -1;

	if (!req->opts && !req->optstr)
		/* we are optimists and missing options are not error */
		return 0;

	mnt_request_reset_errbuf(req);

	if (!req->opts) {
		req->opts = mnt_new_opts();
		if (!req->opts)
			return -1;
		req->flags |= MNT_REQUEST_PRIVATE_OPTS;

		mnt_opts_add_builtin_map(req->opts, MNT_LINUX_MAP);
		mnt_opts_add_builtin_map(req->opts, MNT_USERSPACE_MAP);

		if (mnt_opts_parse(req->opts, req->optstr)) {
			snprintf(req->errbuf, sizeof(req->errbuf),
				"failed to parse mount options: %s",
				req->optstr);
			return -1;
		}
	}

	return 0;
}

/**
 * mnt_request_assemble_permissions:
 * @req: mrequest pointer
 *
 * This function requires already parsed options, see also:
 * mnt_request_assemble_options()
 *
 * Returns 0 on success, -1 in case of error or 1 in case user has no
 * permissions to mount/umount/...
 */
int mnt_request_assemble_permissions(mrequest *req)
{
	int flags;
	const struct mopt_map *map = mnt_get_builtin_map(MNT_USERSPACE_MAP);

	if (!req)
		return -1;
	if (!mnt_request_is_restricted(req)) {
		mnt_opts_remove_option_by_flags(req->opts, map,
			MNT_MS_OWNER | MNT_MS_GROUP |
			MNT_MS_USER | MNT_MS_USERS);
		return 0;
	}
	if (!req->opts || !req->srcpath)
		return -1;

	flags = mnt_opts_get_ids(req->opts, map);

	/*
	* MS_OWNER: Allow owners to mount when fstab contains
	* the owner option.  Note that this should never be used
	* in a high security environment, but may be useful to give
	* people at the console the possibility of mounting a floppy.
	* MS_GROUP: Allow members of device group to mount. (Martin Dickopp)
	*/
	if (flags & (MNT_MS_OWNER | MNT_MS_GROUP)) {
		struct stat sb;

		if (strncmp(req->srcpath, "/dev/", 5) &&
		    stat(req->srcpath, &sb))
			goto nouser;

		if ((flags & MNT_MS_OWNER) && getuid() == sb.st_uid)
			flags |= MNT_MS_USER;

		if (flags & MNT_MS_GROUP) {
			if (getgid() == sb.st_gid)
				flags |= MNT_MS_USER;
			else {
				int n = getgroups(0, NULL);
				gid_t *gs;

				if (n <= 0)
					goto nouser;

				gs = malloc(n * sizeof(*gs));
				if (!gs)
					return -1;

				if (getgroups(n, gs) == n) {
					int i;
					for (i = 0; i < n; i++) {
						if (gs[i] == sb.st_gid) {
							flags |= MNT_MS_USER;
							break;
						}
					}
				}
				free(gs);
			}
		}
	}

nouser:

	if (!(flags & (MNT_MS_USER | MNT_MS_USERS))) {
		snprintf(req->errbuf, sizeof(req->errbuf),
			"only root can mount %s on %s",
			req->source, req->target);
		return 1;
	}

	if (flags & MNT_MS_USER) {
		/* append username to options */
		moption *op = mnt_opts_get_option(req->opts, "user");
		char *username = mnt_get_username(getuid());

		if (!username)
			return -1;
		if (op)
			/* set username */
			mnt_option_set_value(op, username, strlen(username));
		else {
			/* append user=<usernama> option */
			char buf[6 + strlen(username)];

			snprintf(buf, sizeof(buf), "user=%s", username);
			mnt_opts_parse(req->opts, buf);
		}
		free(username);
	}

	mnt_opts_remove_option_by_flags(req->opts, map,
			MNT_MS_OWNER | MNT_MS_GROUP);
	return 0;
}


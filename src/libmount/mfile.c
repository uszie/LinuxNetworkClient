/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <blkid/blkid.h>

#include "nls.h"
#include "mountP.h"

/**
 * mnt_new_mfile:
 * @filename: file name or NULL
 *
 * The mfile is a container for mfile entries that usually represents a fstab,
 * mtab or mountinfo file from your system.
 *
 * Note that this function does not parse the file. See also
 * mnt_parse_mfile().
 *
 * Returns newly allocated mfile struct.
 */
mfile *mnt_new_mfile(const char *filename)
{
	mfile *mf;

	mf = calloc(1, sizeof(*mf));
	if (!mf)
		return NULL;

	if (filename)
		mf->filename = strdup(filename);
	INIT_LIST_HEAD(&mf->ents);

	return mf;
}

/**
 * mnt_free_mfile:
 * @mfile: mfile pointer
 *
 * Deallocates mfile struct and all entries.
 */
void mnt_free_mfile(mfile *mf)
{
	if (!mf)
		return;
	free(mf->filename);

	while (!list_empty(&mf->ents)) {
		mentry *ent = list_entry(mf->ents.next, mentry, ents);
		mnt_free_entry(ent);
	}

	free(mf);
}

/**
 * mnt_mfile_get_nents:
 * @mf: pointer to mfile
 *
 * Returns number of valid entries in mfile.
 */
int mnt_mfile_get_nents(mfile *mf)
{
	assert(mf);
	return mf ? mf->nents : 0;
}

/**
 * mnt_mfile_set_pcache:
 * @mf: pointer to mfile
 * @mpc: pointer to mpcache instance
 *
 * Setups a cache for canonicalized paths. The cache is optional and useful for
 * large mfiles if you need to often lookup by paths in the file. The cache
 * could be shared between more mfiles. Be careful when you share the same
 * cache between more threads -- currently the cache does not provide any
 * locking method.
 *
 * See also mnt_new_pcache().
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mfile_set_pcache(mfile *mf, mpcache *mpc)
{
	assert(mf);
	if (!mf)
		return -1;
	mf->pcache = mpc;
	return 0;
}

/**
 * mnt_mfile_get_pcache:
 * @mf: pointer to mfile
 *
 * Returns pointer to mpcache instance or NULL.
 */
mpcache *mnt_mfile_get_pcache(mfile *mf)
{
	assert(mf);
	return mf ? mf->pcache : NULL;
}

/*
 * Compares @path with realpath(@native)
 */
static int mnt_mfile_realpath_cmp(mfile *mf, const char *path, const char *native)
{
	char *real;
	int res;

	assert(mf);
	assert(path);

	if (!native)
		return 1;
	real = mnt_resolve_path(native, mf->pcache);
	if (!real)
		return 1;
	res = strcmp(path, real);
	if (!mf->pcache)
		free(real);
	return res;
}

/**
 * mnt_mfile_get_name:
 * @mf: mfile pointer
 *
 * Returns mfile file name or NULL.
 */
const char *mnt_mfile_get_name(mfile *mf)
{
	assert(mf);
	return mf ? mf->filename : NULL;
}

/**
 * mnt_mfile_add_entry:
 * @mf: mfile pointer
 * @ent: new entry
 *
 * Adds a new entry to mfile.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mfile_add_entry(mfile *mf, mentry *ent)
{
	assert(mf);
	assert(ent);

	if (!mf || !ent)
		return -1;

	list_add_tail(&ent->ents, &mf->ents);

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: add entry: %s %s\n",
		mf->filename, mnt_entry_get_source(ent),
		mnt_entry_get_target(ent)));

	if (ent->flags & MNT_ENTRY_ERR)
		mf->nerrs++;
	else
		mf->nents++;
	return 0;
}

/**
 * mnt_mfile_remove_entry:
 * @mf: mfile pointer
 * @ent: new entry
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_mfile_remove_entry(mfile *mf, mentry *ent)
{
	assert(mf);
	assert(ent);

	if (!mf || !ent)
		return -1;

	list_del(&ent->ents);

	if (ent->flags & MNT_ENTRY_ERR)
		mf->nerrs--;
	else
		mf->nents--;
	return 0;
}

/**
 * mnt_iterate_entries:
 * @itr: iterator
 * @mf: mfile pointer
 * @ent: returns the next mfile entry
 *
 * Returns 0 on success, -1 in case of error or 1 at end of list.
 *
 * Example (list all mountpoints from fstab in backward order):
 *
 *	mentry *ent;
 *	mfile *mf = mnt_new_mfile("/etc/fstab", 0);
 *	miter *itr = mnt_new_miter(MNT_ITER_BACKWARD);
 *
 *	mnt_parse_mfile(mf);
 *
 *	while(mnt_iterate_entries(itr, mf, &ent) == 0) {
 *		const char *dir = mnt_entry_get_target(ent);
 *		printf("mount point: %s\n", dir);
 *	}
 *	mnt_free_mfile(fi);
 */
int mnt_iterate_entries(miter *itr, mfile *mf, mentry **ent)
{
	assert(mf);
	assert(itr);
	assert(ent);

	if (!mf || !itr || !ent)
		return -1;
again:
	if (!itr->head)
		MNT_MITER_INIT(itr, &mf->ents);
	if (itr->p != itr->head) {
		MNT_MITER_ITERATE(itr, *ent, struct _mentry, ents);
		return 0;
	}

	/* ignore broken entries */
	if (*ent && ((*ent)->flags & MNT_ENTRY_ERR))
		goto again;

	return 1;
}

/**
 * mnt_mfile_find_srcpath:
 * @mf: mfile pointer
 * @path: "source path", device or directory (for bind mounts) path;
 *         1st field in fstab/mtab.
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in given mfile. The function always compare @path
 * with "native" (non-canonicalized) srcpaths. The second iteration is
 * comparison with canonicalized srcpaths.
 *
 * The @path is not canonicalized.
 *
 * Returns a mfile entry or NULL.
 */
mentry *mnt_mfile_find_srcpath(mfile *mf, const char *path, int direction)
{
	miter itr;
	mentry *ent = NULL;
	const char *p;

	assert(mf);
	assert(path);

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: lookup srcpath: %s\n", mf->filename, path));

	/* native paths */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		p = mnt_entry_get_srcpath(ent);
		if (p && strcmp(p, path) == 0)
			return ent;
	}

	/* canonicalized paths */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		/* canonicalize srcpath for net/pseudo FS is non-sense */
		if (ent->flags & (MNT_ENTRY_PSEUDOFS | MNT_ENTRY_NETFS))
			continue;
	        p = mnt_entry_get_srcpath(ent);
		if (p && mnt_mfile_realpath_cmp(mf, path, p) == 0)
			return ent;
	}

	/* converts TAGs to device names */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		const char *tag, *val;
		char *dev;
		int cmp;

		p = mnt_entry_get_srcpath(ent);
		if (p)
			continue;
		if (mnt_entry_get_tag(ent, &tag, &val))
			continue;
		dev = mnt_resolve_tag(tag, val, mf->pcache);
		if (!dev)
			continue;
		cmp = strcmp(dev, path);
		if (!mf->pcache)
			free(dev);
		if (!cmp)
			return ent;
	}
	return NULL;
}

/**
 * mnt_mfile_find_target:
 * @mf: mfile pointer
 * @path: mountpoint directory
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in given mfile.
 *
 * Returns a mfile entry or NULL.
 */
mentry *mnt_mfile_find_target(mfile *mf, const char *path, int direction)
{
	miter itr;
	mentry *ent = NULL;

	assert(mf);
	assert(path);

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: lookup target: %s\n", mf->filename, path));

	/* native paths */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0)
		if (ent->target && strcmp(ent->target, path) == 0)
			return ent;

	/* canonicalized paths */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		if (mnt_mfile_realpath_cmp(mf, path, ent->target) == 0)
			return ent;
	}
	return NULL;
}

/**
 * mnt_mfile_find_tag:
 * @mf: mfile pointer
 * @tag: tag name (e.g "LABEL", "UUID", ...)
 * @val: tag value
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in given mfile.
 *
 * Returns a mfile entry or NULL.
 */
mentry *mnt_mfile_find_tag(mfile *mf, const char *tag,
			const char *val, int direction)
{
	miter itr;
	mentry *ent = NULL;
	char *dev;

	assert(mf);
	assert(tag);
	assert(val);

	if (!mf || !tag || !val)
		return NULL;

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: lookup by TAG: %s %s\n", mf->filename, tag, val));

	mnt_reset_iter(&itr, direction);

	/* look up by TAG */
	while(mnt_iterate_entries(&itr, mf, &ent) == 0)
		if (ent->tagname && ent->tagval &&
		    strcmp(ent->tagname, tag) == 0 &&
		    strcmp(ent->tagval, val) == 0)
			return ent;

	ent = NULL;

	/* look up by device name */
	dev = mnt_resolve_tag(tag, val, mf->pcache);
	if (dev) {
		ent = mnt_mfile_find_srcpath(mf, dev, direction);
		if (!mf->pcache)
			free(dev);
	}
	return ent;
}

/**
 * mnt_mfile_find_source:
 * @mf: mfile pointer
 * @source: TAG or path
 *
 * This is high-level API for mnt_mfile_find_{srcpath,tag}. You needn't to
 * care about source format. This function parses @source and lookup an
 * entry in given mfile.
 *
 * Returns a mfile entry or NULL.
 */
mentry *mnt_mfile_find_source(mfile *mf, const char *source, int direction)
{
	mentry *ent = NULL;
	char *path;

	assert(mf);
	assert(source);

	if (!mf || !source)
		return NULL;

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: lookup SOURCE: %s\n", mf->filename, source));

	if (strchr(source, '=')) {
		char *tag, *val;

		if (blkid_parse_tag_string(source, &tag, &val) == 0) {

			ent = mnt_mfile_find_tag(mf, tag, val, direction);

			free(tag);
			free(val);
		}
	} else {
		path = mnt_resolve_path(source, mf->pcache);
		if (path)
			ent = mnt_mfile_find_srcpath(mf, path, direction);
		if (!mf->pcache)
			free(path);
	}
	return ent;
}

/**
 * mnt_mfile_find_pair:
 * @mf: mfile pointer
 * @srcpath: "source path", device or directory (for bind mounts)
 * @target: path to mount point directory
 * @direction: MNT_ITER_{FORWARD,BACKWARD}
 *
 * Try to lookup an entry in given mfile. This function is usually used by
 * umount(8) -- in such a case is better to canonicalize @srcpath and @target,
 * because all stuff in mtab is always canonicalized.
 *
 * Returns a mfile entry or NULL.
 */
mentry *mnt_mfile_find_pair(mfile *mf, const char *srcpath,
			const char *target, int direction)
{
	miter itr;
	mentry *ent = NULL;
	const char *p;

	assert(mf);
	assert(srcpath);
	assert(target);

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: lookup pair: srcpath(%s) target(%s)\n",
		mf->filename, srcpath, target));

	/* "native" paths */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		if (!ent->target || strcmp(ent->target, target))
			continue;
		p = mnt_entry_get_srcpath(ent);
		if (p && strcmp(p, srcpath) == 0)
			return ent;
	}

	/* "canonicalized" paths */
	mnt_reset_iter(&itr, direction);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		int res;
		if (mnt_mfile_realpath_cmp(mf, target, ent->target))
			continue;
		p = mnt_entry_get_srcpath(ent);
		if (!p)
			continue;
		if (ent->flags & (MNT_ENTRY_PSEUDOFS | MNT_ENTRY_NETFS))
			/* canonicalize srcpath for net/pseudo FS is non-sense */
			res = strcmp(srcpath, p);
		else
			res = mnt_mfile_realpath_cmp(mf, srcpath, p);
		if (res == 0)
			return ent;
	}
	return NULL;
}

/**
 * mnt_fprintf_mfile:
 * @f: FILE
 * @fmt: per line printf-like format string (see MNT_MFILE_PRINTFMT)
 * @mf: mfile pointer
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_fprintf_mfile(FILE *f, const char *fmt, mfile *mf)
{
	miter itr;
	mentry *ent;

	assert(f);
	assert(fmt);
	assert(mf);

	if (!f || !fmt || !mf)
		return -1;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);
	while(mnt_iterate_entries(&itr, mf, &ent) == 0) {
		if (mnt_fprintf_entry(f, fmt, ent) == -1)
			return -1;
	}

	return 0;
}

/**
 * mnt_rewrite_mfile:
 * @mf: mfile pointer
 *
 * Writes mfile to disk. Don't forget to lock the file (see mnt_lock()).
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_rewrite_mfile(mfile *mf)
{
	FILE *f = NULL;
	char tmpname[PATH_MAX];
	const char *filename;
	struct stat st;
	int fd;

	assert(mf);
	if (!mf)
		goto error;

	filename = mnt_mfile_get_name(mf);
	if (!filename)
		goto error;

	if (snprintf(tmpname, sizeof(tmpname), "%s.tmp", filename)
						>= sizeof(tmpname))
		goto error;

	f = fopen(tmpname, "w");
	if (!f)
		goto error;

	if (mnt_fprintf_mfile(f, MNT_MFILE_PRINTFMT, mf) != 0)
		goto error;

	fd = fileno(f);

	if (fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) < 0)
		goto error;

	/* Copy uid/gid from the present file before renaming. */
	if (stat(filename, &st) == 0) {
		if (fchown(fd, st.st_uid, st.st_gid) < 0)
			goto error;
	}

	fclose(f);
	f = NULL;

	if (rename(tmpname, filename) < 0)
		goto error;

	return 0;
error:
	if (f)
		fclose(f);
	return -1;
}

#ifdef LIBMOUNT_TEST_PROGRAM
int test_strerr(struct mtest *ts, int argc, char *argv[])
{
	char buf[BUFSIZ];
	mfile *mf;
	int i;

	mf = mnt_new_mfile("-test-");
	if (!mf)
		goto err;

	for (i = 0; i < 10; i++) {
		mentry *ent = mnt_new_entry();
		if (!ent)
			goto err;
		if (i % 2)
			ent->flags |= MNT_ENTRY_ERR;	/* mark entry as broken */
		ent->lineno = i+1;
		mnt_mfile_add_entry(mf, ent);
	}

	printf("\tadded %d valid lines\n", mnt_mfile_get_nents(mf));
	printf("\tadded %d broken lines\n", mnt_mfile_get_nerrs(mf));

	if (!mnt_mfile_get_nerrs(mf))		/* report broken entries */
		goto err;
	mnt_mfile_strerror(mf, buf, sizeof(buf));
	printf("\t%s\n", buf);

	mnt_free_mfile(mf);
	return 0;
err:
	return -1;
}

mfile *create_mfile(const char *file)
{
	mfile *mf;

	if (!file)
		return NULL;
	mf = mnt_new_mfile(file);
	if (!mf)
		goto err;
	if (mnt_parse_mfile(mf) != 0)
		goto err;
	if (mnt_mfile_get_nerrs(mf)) {
		char buf[BUFSIZ];
		mnt_mfile_strerror(mf, buf, sizeof(buf));
		fprintf(stderr, "%s\n", buf);
		goto err;
	}
	return mf;
err:
	mnt_free_mfile(mf);
	return NULL;
}

int test_bind(struct mtest *ts, int argc, char *argv[])
{
	mfile *mf;
	mentry *ent;
	miter *itr;

	mf = create_mfile(argv[1]);
	if (!mf)
		goto err;
	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto err;
	while(mnt_iterate_entries(itr, mf, &ent) == 0) {
		if (!mnt_entry_is_bind(ent))
			continue;
		printf("%s|%s|%s\n",
				mnt_entry_get_srcpath(ent),
				mnt_entry_get_target(ent),
				mnt_entry_get_optstr(ent));
	}
	mnt_free_mfile(mf);
	mnt_free_iter(itr);
	return 0;
err:
	return -1;
}

int test_fprintf(struct mtest *ts, int argc, char *argv[])
{
	mfile *mf;

	mf = create_mfile(argv[1]);
	if (!mf)
		return -1;

	mnt_fprintf_mfile(stdout, MNT_MFILE_PRINTFMT, mf);
	mnt_free_mfile(mf);
	return 0;
}

int test_find(struct mtest *ts, int argc, char *argv[], int dr)
{
	mfile *mf;
	mentry *ent = NULL;
	mpcache *mpc;
	const char *file, *find, *what;

	if (argc != 4) {
		fprintf(stderr, "try --help\n");
		goto err;
	}

	file = argv[1], find = argv[2], what = argv[3];

	mf = create_mfile(file);
	if (!mf)
		goto err;

	/* create a cache for canonicalized paths */
	mpc = mnt_new_pcache();
	if (!mpc)
		goto err;
	mnt_mfile_set_pcache(mf, mpc);

	if (strcasecmp(find, "source") == 0)
		ent = mnt_mfile_find_source(mf, what, dr);
	else if (strcasecmp(find, "target") == 0)
		ent = mnt_mfile_find_target(mf, what, dr);

	if (!ent)
		fprintf(stderr, "%s: not found %s '%s'\n", file, find, what);
	else {
		const char *s = mnt_entry_get_srcpath(ent);
		if (s)
			printf("%s", s);
		else {
			const char *tag, *val;
			mnt_entry_get_tag(ent, &tag, &val);
			printf("%s=%s", tag, val);
		}
		printf("|%s|%s\n", mnt_entry_get_target(ent),
				mnt_entry_get_optstr(ent));
	}
	mnt_free_mfile(mf);
	mnt_free_pcache(mpc);
	return 0;
err:
	return -1;
}

int test_find_bw(struct mtest *ts, int argc, char *argv[])
{
	return test_find(ts, argc, argv, MNT_ITER_BACKWARD);
}

int test_find_fw(struct mtest *ts, int argc, char *argv[])
{
	return test_find(ts, argc, argv, MNT_ITER_FORWARD);
}

int main(int argc, char *argv[])
{
	struct mtest tss[] = {
	{ "--strerror", test_strerr,       "        test mfile error reporting" },
	{ "--ls-binds", test_bind,         "<file>  list bind mounts" },
	{ "--print",    test_fprintf,      "<file>  test mfile printing" },
	{ "--find-forward",  test_find_fw, "<file> <source|target> <string>" },
	{ "--find-backward", test_find_bw, "<file> <source|target> <string>" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}

#endif /* LIBMOUNT_TEST_PROGRAM */

/*
 * Copyright (C) 2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <blkid/blkid.h>	/* TODO: use <blkid.h> only */

#include "nls.h"
#include "mountP.h"

#define MNT_PCACHE_CHUNKSZ	128

/*
 * Canonicalized (resolved) Paths Cache
 */

#define MNT_PCACHE_ISTAG	1
#define MNT_PCACHE_ISPATH	2

/* path cache entry */
struct mpent {
	char	*native;	/* the original path */
	char	*real;		/* canonicalized path */
	int	flag;
};

struct _mpcache {
	struct mpent	*ents;
	size_t		nents;
	size_t		nallocs;
};

/**
 * mnt_new_pcache:
 *
 * Returns new mpcache instance or NULL in case of ENOMEM error.
 */
mpcache *mnt_new_pcache(void)
{
	return calloc(1, sizeof(struct _mpcache));
}

/**
 * mnt_free_pcache:
 * @mpc: pointer to mpcache instance
 *
 * Deallocates mpcache.
 */
void mnt_free_pcache(mpcache *mpc)
{
	int i;

	if (!mpc)
		return;
	for (i = 0; i < mpc->nents; i++) {
		struct mpent *e = &mpc->ents[i];
		if (e->real != e->native)
			free(e->real);
		free(e->native);
	}
	free(mpc->ents);
	free(mpc);
}

/* note that the @native could be tha same pointer as @real */
static int mnt_pcache_add_entry(mpcache *mpc, char *native, char *real, int flag)
{
	struct mpent *e;

	assert(mpc);
	assert(real);
	assert(native);

	if (!mpc || !native || !real)
		return -1;

	if (mpc->nents == mpc->nallocs) {
		size_t sz = mpc->nallocs + MNT_PCACHE_CHUNKSZ;

		e = realloc(mpc->ents, sz * sizeof(struct mpent));
		if (!e)
			return -1;
		mpc->ents = e;
		mpc->nallocs = sz;
	}

	e = &mpc->ents[mpc->nents];
	e->native = native;
	e->real = real;
	e->flag = flag;
	mpc->nents++;
	return 0;
}

/**
 * mnt_pcache_find_path:
 * @mpc: pointer to mpcache instance
 * @path: requested "native" (non-canonicalized) path
 *
 * Returns cached canonicalized path or NULL.
 */
const char *mnt_pcache_find_path(mpcache *mpc, const char *path)
{
	int i;

	assert(mpc);
	assert(path);

	if (!mpc || !path)
		return NULL;

	for (i = 0; i < mpc->nents; i++) {
		struct mpent *e = &mpc->ents[i];
		if (!(e->flag & MNT_PCACHE_ISPATH))
			continue;
		if (strcmp(path, e->native) == 0)
			return e->real;
	}
	return NULL;
}

/**
 * mnt_pcache_find_tag:
 * @mpc: pointer to mpcache instance
 * @token: tag name
 * @value: tag value
 *
 * Returns cached path or NULL.
 */
const char *mnt_pcache_find_tag(mpcache *mpc, const char *token, const char *value)
{
	int i;
	size_t tksz;

	assert(mpc);
	assert(token);
	assert(value);

	if (!mpc || !token || !value)
		return NULL;

	tksz = strlen(token);

	for (i = 0; i < mpc->nents; i++) {
		struct mpent *e = &mpc->ents[i];
		if (!(e->flag & MNT_PCACHE_ISTAG))
			continue;
		if (strcmp(token, e->native) == 0 &&
		    strcmp(value, e->native + tksz + 1))
			return e->real;
	}
	return NULL;
}

/**
 * mnt_resolve_path:
 * @path: "native" path
 * @mpc: cache for results or NULL
 *
 * Returns absolute path or NULL in case of error. The result has to be
 * deallocated by free() if @cache is NULL.
 */
char *mnt_resolve_path(const char *path, mpcache *mpc)
{
	char *p = NULL;
	char *native = NULL;
	char *real = NULL;

	assert(path);

	if (!path)
		return NULL;
	if (mpc)
		p = (char *) mnt_pcache_find_path(mpc, path);

	/* TODO: THIS IS TEMPORARY SOLUTION, we have to replace glibc realpath
	 *       with stuff from lib/canonicalize.c from the master util-linux-ng
	 *       branch.
	 */
	#warning "replace realpath(2) with lib/canonicalize.c stuff!"

	if (!p) {
		char buf[PATH_MAX];

		p = realpath(path, buf);
		if (p && mpc) {
			native = strdup(path);
			real = strcmp(path, p) == 0 ? native : strdup(p);

			if (!native || !real)
				goto error;

			if (mnt_pcache_add_entry(mpc, native, real,
							MNT_PCACHE_ISPATH))
				goto error;
		} else if (p)
			p = strdup(p);
	}

	return p;
error:
	if (real != native)
		free(real);
	free(native);
	return NULL;
}


/**
 * mnt_resolve_tag:
 * @token: tag name
 * @value: tag value
 * @cache: for results or NULL
 *
 * Returns device name or NULL in case of error. The result has to be
 * deallocated by free() if @cache is NULL.
 */
char *mnt_resolve_tag(const char *token, const char *value, mpcache *mpc)
{
	char *p = NULL;

	assert(token);
	assert(value);

	if (!token || !value)
		return NULL;

	if (mpc)
		p = (char *) mnt_pcache_find_tag(mpc, token, value);
	if (!p) {
		/* returns newly allocated string */
		p = blkid_evaluate_tag(token, value, NULL);

		if (p && mpc) {
			/* add into cache -- cache format for TAGs is
			 *	native = "NAME\0VALUE\0"
			 *	real   = "/dev/foo"
			 */
			size_t tksz = strlen(token);
			size_t vlsz = strlen(value);
			char *native = malloc(tksz + vlsz + 2);

			if (!native)
				goto error;

			memcpy(native, token, tksz + 1);	   /* include '\0' */
			memcpy(native + tksz, value, vlsz + 1);
			if (mnt_pcache_add_entry(mpc, native, p, MNT_PCACHE_ISTAG)) {
				free(native);
				goto error;
			}
		}
	}
	return p;
error:
	free(p);
	return NULL;
}

#ifdef LIBMOUNT_TEST_PROGRAM

int test_resolve_path(struct mtest *ts, int argc, char *argv[])
{
	char line[BUFSIZ];
	mpcache *mpc;

	mpc = mnt_new_pcache();
	if (!mpc)
		return EXIT_FAILURE;

	while(fgets(line, sizeof(line), stdin)) {
		size_t sz = strlen(line);
		char *p = NULL;

		if (line[sz - 1] == '\n')
			line[sz - 1] = '\0';

		p = mnt_resolve_path(line, mpc);
		printf("%s : %s\n", line, p);
	}
	mnt_free_pcache(mpc);
	return EXIT_SUCCESS;
}

int test_resolve_tag(struct mtest *ts, int argc, char *argv[])
{
	char line[BUFSIZ];
	mpcache *mpc;

	mpc = mnt_new_pcache();
	if (!mpc)
		return EXIT_FAILURE;

	while(fgets(line, sizeof(line), stdin)) {
		size_t sz = strlen(line);
		char *p = NULL;
		char *v, *t;

		if (line[sz - 1] == '\n')
			line[sz - 1] = '\0';

		if (blkid_parse_tag_string(line, &t, &v))
			fprintf(stderr, "Parse error: %s\n", line);

		p = mnt_resolve_tag(t, v, mpc);
		free(t);
		free(v);
		printf("%s : %s\n", line, p);
	}
	mnt_free_pcache(mpc);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	struct mtest tss[] = {
	{ "--resolve-path", test_resolve_path, "   resolve paths from stdin" },
	{ "--resolve-tag",  test_resolve_tag,  "   evaluate NAME=value tags from stdin" },
	{ NULL }
	};

	return mnt_run_test(tss, argc, argv);
}
#endif

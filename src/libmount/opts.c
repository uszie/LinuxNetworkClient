/*
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * "options container" (opts) -- all parsed options are stored in "mopt"
 * container. You can add/remove/modify options in the container. It's
 * possible to generate "mountflags", "mountdata" or mtab-like options
 * string from the container. See the example below.
 *
 * mount(2) options
 * ----------------
 * The options with MNT_MFLAG will be used in the "mountflags" mount(2)
 * syscall; options with MNT_MDATA will be used in the "mountdata" mount(2)
 * syscall. For more details see mnt_opts_create_mountdata() and
 * mnt_opts_create_mountflags().
 *
 * All unknown mount options (without MNT_MFLAG or MNT_MDATA mask) are marked
 * as "extra options" and they are always added to "mountdata" -- it means
 * these options are always visible for mount(2) syscall and always added to
 * /etc/mtab.
 *
 * All options that are _not_ marked as MNT_NOMTAB (e.g. "auto") or inverted
 * MNT_MFLAGs (e.g. "atime") will be added to /etc/mtab options string.
 *
 * This all is default behaviour, you can modify the list of options and
 * generate arbitrary output of course. The list of options is completely
 * accessable by mnt_opts_iterate().
 *
 * Example (mopt container):
 * -------------------------
 *    - add new options "foo", "nofoo" and "bar=<data>"
 *    - "foo" is a mountflag for mount(2) syscall
 *    - "bar" is a mountdata for mount(2) syscall, this option require data
 *
 *  // IDs
 *  #define MY_MS_FOO	(1 << 1)
 *  #define MY_MS_BAR   (1 << 2)
 *
 *  // define map
 *  mopts_map myoptions[] = {
 *	{ "foo",   MY_MS_FOO, MNT_MFLAG },
 *	{ "nofoo", MY_MS_FOO, MNT_MFLAG | MNT_INVERT },
 *	{ "bar=%s",MY_MS_BAR, MNT_MDATA },
 *	{ NULL }
 *  };
 *
 *  mopts     *opts = mnt_opts_new();
 *  moption   *option;
 *  mopts_map *map;
 *  int       ids;
 *
 *  // enable generic mount options
 *  mnt_opts_add_builtin_map(opts, MNT_LINUX_MAP);
 *  mnt_opts_add_builtin_map(opts, MNT_USERSPACE_MAP);
 *
 *  // add custom options map
 *  mnt_opts_add_map( opts, myoptions );
 *
 *  // parse mount options from command line
 *  mnt_opts_parse( opts, argv[<n>] );
 *
 *  // print all options
 *  while(mnt_opts_iterate( opts, NULL, &option ))
 *     printf("%s\n", mnt_option_get_name(option)));
 *
 *  // check for option by IDs
 *  ids =  mnt_opts_get_ids(opts, myoptions);
 *  if (ids & MY_MS_FOO)
 *      printf("foo is ON\n");
 *
 *  // is this mount read-only?
 *  map = mnt_get_builtin_map(MNT_MAP_LINUX);
 *  ids = mnt_opts_get_ids(opts, map);
 *  if (ids & MS_RDONLY)
 *     printf("read-only mode requested\n");
 *
 *  // print uknown (usually fs-depend) options:
 *  while(mnt_opts_iterate_extra( opts, &option ))
 *     printf("extra options: %s\n", mnt_option_get_name(option)));
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "nls.h"
#include "mountP.h"

/**
 * mnt_opts_add_map:
 * @opts: pointer to mopts instance
 * @map: pointer to the custom map
 *
 * Stores pointer to the custom options map (options description). The map has
 * to be accessible all time when the libmount works with options.
 *
 * Example (add new options "foo" and "bar=<data>"):
 *
 *     #define MY_MS_FOO   (1 << 1)
 *     #define MY_MS_BAR   (1 << 2)
 *
 *     mopts_map myoptions[] = {
 *       { "foo",   MY_MS_FOO, MNT_MFLAG },
 *       { "nofoo", MY_MS_FOO, MNT_MFLAG | MNT_INVERT },
 *       { "bar=%s",MY_MS_BAR, MNT_MDATA },
 *       { NULL }
 *     };
 *
 *     mnt_opts_add_map(opts, myoptions);
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_opts_add_map(mopts *opts, const struct mopt_map *map)
{
	assert(opts);
	assert(map || opts->maps == NULL);

	opts->maps = realloc(opts->maps,
			sizeof(struct mopt_map *) * (opts->nmaps + 1));
	if (!opts->maps)
		return -1;

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: add map[%zd]", opts, opts->nmaps));
	opts->maps[opts->nmaps] = map;
	opts->nmaps++;
	return 0;
}

/**
 * mnt_opts_add_builtin_map:
 * @opts: pointer to mopts instance
 * @map_id: built-in map id (see mnt_get_builtin_map())
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_opts_add_builtin_map(mopts *opts, int id)
{
	const struct mopt_map *m = mnt_get_builtin_map(id);

	assert(opts);
	assert(id);

	return m ? mnt_opts_add_map(opts, m) : -1;
}

/*
 * Append the option to "opts" container.
 */
static void mnt_opts_add_option(mopts *opts, moption *op)
{
	assert(opts);
	assert(op);

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: add option %s",
		opts, mnt_option_get_name(op)));

	list_add_tail(&op->opts, &opts->opts);
}

/**
 * mnt_new_opts:
 *
 * Returns newly allocated and initialized mopts instance. The library
 * uses this object as a container for mount options.
 */
mopts *mnt_new_opts(void)
{
	mopts *opts = calloc(1, sizeof(struct _mopts));
	if (!opts)
		return NULL;
	INIT_LIST_HEAD(&opts->opts);
	return opts;
}

/**
 * mnt_free_opts:
 * @opts: pointer to mopts instance.
 *
 * Deallocates mopts and all stored options.
 */
void mnt_free_opts(mopts *opts)
{
	assert(opts);
	if (!opts)
		return;
	while (!list_empty(&opts->opts)) {
		moption *o = list_entry(opts->opts.next, moption, opts);
		mnt_free_option(o);
	}

	free(opts->maps);
	free(opts);
}

/**
 * mnt_opts_remove_option_by_flags:
 * @opts: pointer to mopts instance
 * @map: pointer to the map with wanted options or NULL for all options
 *
 * Removes options which match with @flags. The set of options could
 * be restricted by @map. For exmaple:
 *
 *	mopts_map *map = mnt_get_builtin_map(MNT_LINUX_MAP);
 *	mnt_opts_remove_option_by_flags(opts, map, MS_NOEXEC);
 *
 * removes "noexec" option from "opts".
 *
 * Note that this function is useles for options with MNT_INVERT mask (e.g.
 * "exec" is inverting MS_NOEXEC flag).
 *
 * See also mnt_option_get_flag() and mnt_opts_remove_option_by_iflags().
 *
 * Returns number of removed options or -1 in case of error.
 */
int mnt_opts_remove_option_by_flags(mopts *opts,
		const struct mopt_map *map, const int flags)
{
	struct list_head *p, *pnext;
	int ct = 0;

	if (!opts)
		return -1;

	list_for_each_safe(p, pnext, &opts->opts) {
		moption *op;
		int fl = 0;

		if (!p)
			break;
		op = list_entry(p, moption, opts);

		if (!map || mnt_option_get_map(op) == map) {
			mnt_option_get_flag(op, &fl);
			if (fl & flags) {
				mnt_free_option(op);
				ct++;
			}
		}
	}
	return ct;
}

/**
 * mnt_opts_remove_option_by_iflags:
 * @opts: pointer to mopts instance
 * @map: pointer to the map with wanted options or NULL for all options
 *
 * Removes options which inverting any id from @flags. The set of options could
 * be restricted by @map. For exmaple:
 *
 *	mopts_map *map = mnt_get_builtin_map(MNT_LINUX_MAP);
 *
 *	mnt_opts_remove_option_by_iflags(opts, map, MS_NOEXEC);
 *
 * removes "exec" option from "opts".
 *
 * Note that this function is useles for options without MNT_INVERT mask (e.g.
 * "noexec").
 *
 * See also mnt_option_get_flag() and mnt_opts_remove_option_by_flags().
 *
 * Returns number of removed options or -1 in case of error.
 */
int mnt_opts_remove_option_by_iflags(mopts *opts,
		const struct mopt_map *map, const int flags)
{
	struct list_head *p, *pnext;
	int ct = 0;

	if (!opts)
		return -1;

	list_for_each_safe(p, pnext, &opts->opts) {
		moption *op;
		int fl = flags;

		if (!p)
			break;
		op = list_entry(p, moption, opts);

		if (!map || mnt_option_get_map(op) == map) {
			int id = mnt_option_get_id(op);

			if (!(id & fl))
				continue;

			mnt_option_get_flag(op, &fl);

			if (!(id & fl)) {
				mnt_free_option(op);
				ct++;
			}
		}
	}
	return ct;
}

/**
 * mnt_opts_remove_option_by_name:
 * @opts: pointer to mopts instance
 * @name: option name
 *
 * Returns 0 on success, 1 if @name not found and -1 in case of error.
 */
int mnt_opts_remove_option_by_name(mopts *opts, const char *name)
{
	struct list_head *p, *pnext;

	if (!opts || !name)
		return -1;

	list_for_each_safe(p, pnext, &opts->opts) {
		moption *op;
		const char *n;

		if (!p)
			break;
		op = list_entry(p, moption, opts);
		n = mnt_option_get_name(op);
		if (n && strcmp(name, n) == 0) {
			mnt_free_option(op);
			return 0;
		}
	}
	return 1;
}

/**
 * mnt_opts_parse:
 * @opts: pointer to mopts instance.
 * @optstr: zero terminated string with mount options (comma separaed list)
 *
 * Parses @optstr according to defined options maps and stores all results to
 * @opts container. The options are accessible by mnt_iterate_options().
 *
 * It's possible to call this function more than once. The new options from
 * @optstr will be appended to the container.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_opts_parse(mopts *opts, const char *optstr)
{
	int rc = 0;
	const char *p = optstr;

	assert(opts);
	assert(optstr);

	if (!opts || !optstr)
		return -1;

	while(p && *p) {
		moption *op = mnt_new_option();
		if (!op)
			return -1;
		rc = mnt_optstr_next_option(&p, op, opts->maps, opts->nmaps);
		if (rc)
			/* TODO: store into 'opts' position where the parsing
			 *       failed. We can use it for more verbose error
			 *       messages.
			 */
			return rc == 1 ? 0 : rc;
		mnt_opts_add_option(opts, op);
	}

	return 0;
}

/**
 * @opts: pointer to mopts instance.
 * @map: options map
 *
 * This function re-parse extra options (unknown option). That's useful when
 * you need to add a new map to the already existing @opts. For example:
 *
 *	mnt_opts_add_map(opts, my_nfs_map);
 *	mnt_opts_reparse_extra(opts, my_nfs_map);
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_opts_reparse_extra(mopts *opts, struct mopt_map const *map)
{
	moption *op;
	miter itr;

	assert(opts);
	assert(map);

	if (!opts || !map)
		return -1;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, NULL, &op) == 0) {
		if (!mnt_option_is_extra(op))
			continue;
		if (mnt_reinit_extra_option(op, map) != 0)
			return -1;
	}
	return 0;
}

/**
 * mnt_iterate_options:
 * @opts: pointer to mopts instance
 * @map: pointer to the map of wanted options or NULL for all options
 * @option: returns pointer to the option object
 *
 * Example:
 *
 *     moption *option;
 *     mopts *opts = mnt_opts_new();
 *
 *     mnt_opts_parse(opts, "noexec,nodev");
 *
 *     while(mnt_iterate_options(itr, opts, NULL, &option ))
 *         printf("%s\n", mnt_option_get_name(option)));
 *
 * Returns 0 on succes, -1 in case of error or 1 at end of list.
 */
int mnt_iterate_options(miter *itr, mopts *opts,
		const struct mopt_map *map, moption **option)
{
	assert(itr);
	assert(opts);
	assert(option);

	if (!itr || !opts || !option)
		return -1;
	if (!itr->head)
		MNT_MITER_INIT(itr, &opts->opts);
	while (itr->p != itr->head) {
		MNT_MITER_ITERATE(itr, *option, struct _moption, opts);
		if (map == NULL || (*option)->map == map)
			return 0;
	}

	return 1;
}

/**
 * mnt_opts_get_option:
 * @opts: pointer to mopts instance
 * @name: options name
 *
 * Returns option or NULL.
 */
moption *mnt_opts_get_option(mopts *opts, const char *name)
{
	moption *op;
	miter itr;

	assert(opts);
	assert(name);

	if (!opts || !name)
		return NULL;
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, NULL, &op) == 0) {
		const char *n = mnt_option_get_name(op);

		if (n && !strcmp(n, name))
			return op;
	}
	return NULL;
}

/**
 * mnt_opts_get_ids:
 * @opts: pointer to mopts instance
 * @map: pointer to the map of wanted options or NULL for all options
 *
 * Note that ID has to be unique in all maps when the @map is NULL.
 *
 * Note also that this function works with ALL options -- see also
 * mnt_opts_create_mountflags() that returns MNT_MFLAG options
 * (mount(2) flags) only.
 *
 * Return IDs from all options.
 */
int mnt_opts_get_ids(mopts *opts, const struct mopt_map *map)
{
	int flags = 0;
	miter itr;
	moption *op;

	assert(opts);
	if (!opts)
		return 0;
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, map, &op) == 0)
		mnt_option_get_flag(op, &flags);

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: generated IDs 0x%08x", opts, flags));
	return flags;
}

/**
 * mnt_opts_create_mountflags:
 * @opts: pointer to mopts instance
 *
 * The mountflags are IDs from all MNT_MFLAG options. See "struct mopt_map".
 * For more details about mountflags see mount(2) syscall.
 *
 * Returns mount flags or 0.
 */
int mnt_opts_create_mountflags(mopts *opts)
{
	int flags = 0;
	miter itr;
	moption *op;

	assert(opts);
	if (!opts)
		return 0;
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, NULL, &op) == 0) {
		if (!(op->mask & MNT_MFLAG))
			continue;
		mnt_option_get_flag(op, &flags);
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: generated mountflags 0x%08x", opts, flags));
	return flags;
}

/**
 * mnt_opts_create_mountdata:
 * @opts: pointer to mopts instance
 *
 * For more details about mountdata see mount(2) syscall.
 *
 * Returns newly allocated string with mount options.
 */
char *mnt_opts_create_mountdata(mopts *opts)
{
	miter itr;
	moption *op;
	char *optstr = NULL;

	assert(opts);
	if (!opts)
		return NULL;
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, NULL, &op) == 0) {
		if (!(op->mask & MNT_MDATA) && !mnt_option_is_extra(op))
			continue;
		optstr = mnt_optstr_append_option(optstr, op);
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: generated mountdata: %s", opts, optstr));
	return optstr;
}

/**
 * mnt_opts_create_mtab_optstr:
 * @opts: pointer to mopts instance
 *
 * Returns newly allocated string with mount options for mtab.
 */
char *mnt_opts_create_mtab_optstr(mopts *opts)
{
	miter itr;
	moption *op;
	char *optstr = NULL;

	assert(opts);
	if (!opts)
		return NULL;

	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, NULL, &op) == 0) {
		if (op->mask & MNT_NOMTAB)
			continue;
		optstr = mnt_optstr_append_option(optstr, op);
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: generated mtab options: %s", opts, optstr));
	return optstr;
}

/**
 * mnt_opts_create_userspace_optstr:
 * @opts: pointer to mopts instance
 *
 * Returns newly allocated string with mount options that are
 * userspace specific (e.g. uhelper=).
 */
char *mnt_opts_create_userspace_optstr(mopts *opts)
{
	miter itr;
	moption *op;
	char *optstr = NULL;

	assert(opts);
	if (!opts)
		return NULL;
	mnt_reset_iter(&itr, MNT_ITER_FORWARD);

	while(mnt_iterate_options(&itr, opts, NULL, &op) == 0) {
		if (mnt_option_is_extra(op))
			continue;
		if (op->mask & (MNT_MDATA | MNT_MFLAG | MNT_NOMTAB))
			continue;
		optstr = mnt_optstr_append_option(optstr, op);
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: opts %p: generated userspace-only options: %s",
		opts, optstr));
	return optstr;
}

#ifdef LIBMOUNT_TEST_PROGRAM
mopts *mk_opts(const char *optstr)
{
	mopts *opts = mnt_new_opts();
	if (!opts)
		goto err;

	mnt_opts_add_builtin_map(opts, MNT_LINUX_MAP);
	mnt_opts_add_builtin_map(opts, MNT_USERSPACE_MAP);

	if (mnt_opts_parse(opts, optstr) != 0) {
		fprintf(stderr, "\tfailed to parse: %s\n", optstr);
		goto err;
	}
	return opts;
err:
	mnt_free_opts(opts);
	return NULL;
}

int test_parse(struct mtest *ts, int argc, char *argv[])
{
	mopts *opts = NULL;
	moption *op;
	miter *itr = NULL;
	int rc = -1;

	if (argc < 1)
		goto done;
	opts = mk_opts(argv[1]);
	if (!opts)
		goto done;
	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto done;

	while(mnt_iterate_options(itr, opts, NULL, &op) == 0)
		mnt_option_print_debug(op, stdout, NULL);
	rc = 0;
done:
	mnt_free_opts(opts);
	mnt_free_iter(itr);
	return rc;
}

int test_flags(struct mtest *ts, int argc, char *argv[])
{
	mopts *opts = NULL;
	int rc = -1;
	int flags;
	const struct mopt_map *map;

	if (argc < 1)
		goto done;
	opts = mk_opts(argv[1]);
	if (!opts)
		goto done;

	flags = mnt_opts_create_mountflags(opts);
	printf("\tmount(2) flags:        0x%08x\n", flags);

	map = mnt_get_builtin_map(MNT_LINUX_MAP);
	flags = mnt_opts_get_ids(opts, map);
	printf("\tMNT_MAP_LINUX IDs:     0x%08x  (map %p)\n", flags, map);

	map = mnt_get_builtin_map(MNT_USERSPACE_MAP);
	flags = mnt_opts_get_ids(opts, map);
	printf("\tMNT_USERSPACE_MAP IDs: 0x%08x  (map %p)\n", flags, map);

	rc = 0;
done:
	mnt_free_opts(opts);
	return rc;
}

int test_data(struct mtest *ts, int argc, char *argv[])
{
	mopts *opts = NULL;
	char *optstr;
	int rc = -1;

	if (argc < 1)
		goto done;
	opts = mk_opts(argv[1]);
	if (!opts)
		goto done;

	optstr = mnt_opts_create_mountdata(opts);
	printf("\tmount(2) data: '%s'\n", optstr);
	free(optstr);
	rc = 0;
done:
	mnt_free_opts(opts);
	return rc;
}

int test_mtabstr(struct mtest *ts, int argc, char *argv[])
{
	mopts *opts = NULL;
	char *optstr;
	int rc = -1;

	if (argc < 1)
		goto done;
	opts = mk_opts(argv[1]);
	if (!opts)
		goto done;

	optstr = mnt_opts_create_mtab_optstr(opts);
	printf("\tmtab options: '%s'\n", optstr);
	free(optstr);
	rc = 0;
done:
	mnt_free_opts(opts);
	return rc;
}

int test_reparse(struct mtest *ts, int argc, char *argv[])
{
	const struct mopt_map *map;
	mopts *opts = NULL;
	moption *op;
	miter *itr;
	char *optstr;
	int rc = -1;

	if (argc < 1)
		goto done;
	optstr = argv[1];
	opts = mnt_new_opts();
	if (!opts)
		goto done;

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto done;

	/* parse kernel options only */
	mnt_opts_add_builtin_map(opts, MNT_LINUX_MAP);

	if (mnt_opts_parse(opts, optstr) != 0) {
		fprintf(stderr, "\tfailed to parse: %s\n", optstr);
		goto done;
	}

	fprintf(stdout, "------ parse\n");
	while(mnt_iterate_options(itr, opts, NULL, &op) == 0)
		mnt_option_print_debug(op, stdout, NULL);

	/* parse userspace only */
	map = mnt_get_builtin_map(MNT_USERSPACE_MAP);

	mnt_opts_add_map(opts, map);
	if (mnt_opts_reparse_extra(opts, map) != 0) {
		fprintf(stderr, "\tfailed to re-parse: %s\n", optstr);
		goto done;
	}

	fprintf(stdout, "------ re-parse\n");
	mnt_reset_iter(itr, MNT_ITER_FORWARD);
	while(mnt_iterate_options(itr, opts, NULL, &op) == 0)
		mnt_option_print_debug(op, stdout, NULL);

	rc = 0;
done:
	mnt_free_opts(opts);
	return rc;
}

int main(int argc, char *argv[])
{
	struct mtest tss[] = {
		{ "--parse",    test_parse, "<optstr>  parse mount options string" },
		{ "--ls-data",	test_data,  "<optstr>  parse and generate mountdata" },
		{ "--ls-flags", test_flags, "<optstr>  parse and generate mountflags" },
		{ "--ls-mtabstr",test_mtabstr,"<optstr>  parse and generate mtab options" },
		{ "--reparse",  test_reparse, "<optstr>  test extra options reparsing" },
		{ NULL }
	};
	return  mnt_run_test(tss, argc, argv);
}
#endif /* LIBMOUNT_TEST_PROGRAM */


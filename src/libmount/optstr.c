/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * "option string" (optstr) -- all options are stored in comma delimited
 *  string. mnt_optstr_next_option() is a low-level parser that returns
 *  one "moption". The parser does not store previously parsed options.
 *
 *  while(mnt_optstr_next_option(str, &op, maps, nmaps) == 0) {
 *      ...
 *  }
 *
 *  See also opts.c and options.c.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "nls.h"
#include "mountP.h"

static int mnt_parse_one_option(moption *op, const char *option, size_t len,
				struct mopt_map const **maps, int nmaps)
{
	char *sep;
	size_t namelen;

	assert(op);
	assert(option);
	assert(len > 0);

	sep = strnchr(option, len, '=');
	namelen = sep ? sep - option : len;

	if (sep && namelen + 1 == len)		/* "name=" without value */
		goto error;
	if (__mnt_init_option(op, option, namelen, maps, nmaps) != 0)
		goto error;
	if (sep) {
		if (mnt_option_set_value(op, sep + 1, len - namelen - 1) != 0)
			goto error;
	} else if (mnt_option_require_value(op))
		goto error;

	DBG(DEBUG_OPTIONS, mnt_option_print_debug(
			op, stderr, "libmount: parsed option"));
	return 0;
error:
	{
		DBG(DEBUG_OPTIONS, fprintf(stderr,
				"libmount: failed to parse option: \""));
		DBG(DEBUG_OPTIONS, fwrite(option, 1, len, stderr));
		DBG(DEBUG_OPTIONS, fputs("\"\n", stderr));
	}
	mnt_reset_option(op);
	return -1;
}

/**
 * mnt_optstr_next_option
 * @optstr: zero terminated string with mount options (comma separated list)
 * @op: pointer to moption instance
 * @maps: array with option maps
 * @nmaps: number of @maps
 *
 * Parses the next @op from @optstr.
 *
 * Example:
 *    const char *optstr = argv[<n>];
 *    const char *p = optstr;
 *    moption *op = mnt_new_option();
 *
 *    while(mnt_optstr_next_option(&p, op, maps, nmaps) == 0) {
 *       printf("%s\n", mnt_option_get_name(op));
 *    }
 *    mnt_free_option(op);
 *
 * Returns 0 on success, -1 in case of error or 1 at end of optstr.
 */
int mnt_optstr_next_option(const char **optstr, moption *op,
				struct mopt_map const **maps, int nmaps)
{
	int open_quote = 0, rc = -1;
	const char *begin = NULL, *end = NULL, *p;

	assert(optstr);
	if (!optstr)
		return -1;
	p = *optstr;
	if (*p == '\0')
		return 1;

	mnt_reset_option(op);

	for (; p && *p; p++) {
		if (!begin)
			begin = p;		/* begin of the option item */
		if (*p == '"')
			open_quote ^= 1;	/* reverse the status */
		if (open_quote)
			continue;		/* still in quoted block */
		if (*p == ',')
			end = p;		/* terminate the option item */
		if (*(p + 1) == '\0')
			end = p + 1;		/* end of optstr */
		if (!begin || !end)
			continue;
		if (end <= begin)
			return -1;

		rc = mnt_parse_one_option(op, begin, end - begin, maps, nmaps);
		*optstr = p + 1;
		return rc;
	}
	return 1;
}

/*
 * Merges @vfs and @fs options strings into a new string
 * This function skips the generic part of @fs options.
 * For example (see "rw"):
 *
 *    mnt_merge_optstr("rw,noexec", "rw,journal=update")
 *
 * returns --> "rw,noexec,journal=update"
 *
 * We need this function for /proc/self/mountinfo parsing.
 */
char *mnt_merge_optstr(const char *vfs, const char *fs)
{
	const char *p1 = vfs, *p2 = fs;
	char *res;
	size_t sz;

	if (!vfs && !fs)
		return NULL;
	if (!vfs || !fs)
		return strdup(fs ? fs : vfs);
	if (!strcmp(vfs, fs))
		return strdup(vfs);		/* e.g. "aaa" and "aaa" */

	/* skip the same FS options */
	while (*p1 && *p2 && *++p1 == *++p2);

	if (*p1 == ',')
		p1++;
	if (*p2 == ',')
		p2++;
	if (!*p1 && !*p2)			/* e.g. "aaa,bbb" and "aaa,bbb," */
		return strdup(vfs);
	if (!*p1 || !*p2)			/* e.g. "aaa" and "aaa,bbb" */
		return strdup(*p1 ? vfs : fs);

	p1 = vfs;
	sz = strlen(p1) + strlen(p2) + 2;	/* 2= separator + '\0' */

	res = malloc(sz);
	if (!res)
		return NULL;

	snprintf(res, sz, "%s,%s", p1, p2);
	return res;
}

/**
 * mnt_optstr_append_option:
 * @optstr: option string or NULL
 * @op: moption instance
 *
 * Returns reallocated (or newly allocated) @optstr with name=value from @op.
 */
char *mnt_optstr_append_option(char *optstr, moption *op)
{
	const char *name;
	char *p;
	size_t sz, vsz, osz, nsz;

	assert(op);
	if (!op)
		return NULL;

	name = mnt_option_get_name(op);
	if (!name)
		return NULL;
	osz = optstr ? strlen(optstr) : 0;
	nsz = strlen(name);
	vsz = mnt_option_get_valsiz(op);

	sz = osz + nsz + 1;		/* 1= '\0' */
	if (osz)
		sz++;			/* ',' options separator */
	if (vsz)
		sz += vsz + 1;		/* 1= '=' */

	optstr = realloc(optstr, sz);
	if (!optstr)
		return NULL;
	p = optstr;

	if (osz) {
		p += osz;
		*p++ = ',';
	}

	memcpy(p, name, nsz);
	p += nsz;

	if (vsz) {
		/* TODO: SELinux context options requires quotes */
		*p++ = '=';
		mnt_option_snprintf_value(op, p, vsz + 1);
	} else
		*p = '\0';

	return optstr;
}

/**
 * mnt_optstr_find_option:
 * @optstr: string with comma separated list of options
 * @name: requested option name
 * @value: returns pointer to the begin of the value (e.g. name=VALUE) or NULL
 * @valsz: returns size of options value or 0
 *
 * Lookup for the option in non-parsed list of options.
 *
 * Returns 0 on success, 1 when not found the @name or -1 in case of error.
 */
int mnt_optstr_find_option(const char *optstr, const char *name,
				const char **value, size_t *valsz)
{
	size_t namelen;
	int open_quote = 0;
	const char *begin = NULL, *end = NULL, *p;

	assert(optstr);
	assert(name);

	if (!optstr || !name)
		return -1;

	namelen = strlen(name);
	if (value)
		*value = NULL;
	if (valsz)
		*valsz = 0;

	for (p = optstr; p && *p; p++) {
		if (!begin)
			begin = p;		/* begin of the option item */
		if (*p == '"')
			open_quote ^= 1;	/* reverse the status */
		if (open_quote)
			continue;		/* still in quoted block */
		if (*p == ',')
			end = p;		/* terminate the option item */
		if (*(p + 1) == '\0')
			end = p + 1;		/* end of optstr */
		if (!begin || !end)
			continue;
		if (end <= begin)
			goto error;

		if (!strncmp(name, begin, namelen)) {
			const char *nameend = begin + namelen;

			if (*nameend == ',' || *nameend == '\0')	/* option without value */
			       return 0;
			if (*nameend == '=') {	/* options with value */
				if (value)
					*value = begin + namelen + 1;
				if (valsz)
					*valsz = end - begin - namelen - 1;
				return 0;
			}
		}
		begin = end = NULL;
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
			"libmount: not found \"%s\" in \"%s\"", name, optstr));
	return 1;
error:
	DBG(DEBUG_OPTIONS, fprintf(stderr,
			"libmount: parse error: \"%s\"", optstr));
	return -1;
}


#ifdef LIBMOUNT_TEST_PROGRAM

int test_find(struct mtest *ts, int argc, char *argv[])
{
	const char *optstr;
	const char *name;
	const char *val = NULL;
	size_t sz = 0;
	int rc;

	if (argc < 2)
		goto done;
	optstr = argv[1];
	name = argv[2];

	rc = mnt_optstr_find_option(optstr, name, &val, &sz);
	if (rc == 0) {
		printf("found; name: %s", name);
		if (sz) {
			printf(", argument: size=%zd data=", sz);
			if (fwrite(val, 1, sz, stdout) != sz)
				goto done;
		}
		printf("\n");
		return 0;
	} else if (rc == 1)
		printf("%s: not found\n", name);
	else
		printf("parse error: %s\n", optstr);
done:
	return -1;
}

int test_parse(struct mtest *ts, int argc, char *argv[])
{
	const char *optstr, *p;
	moption *op = NULL;
	int rc = -1;
	struct mopt_map const *maps[2];
        int nmaps;

	if (argc < 1)
		goto done;
	p = optstr = argv[1];

	op = mnt_new_option();
	if (!op)
		goto done;

	/* define options maps */
	maps[0] = mnt_get_builtin_map(MNT_LINUX_MAP);	   /* generic kernel options */
	maps[1] = mnt_get_builtin_map(MNT_USERSPACE_MAP);  /* mount(8) options */
	nmaps = 2;

	do {
		/* parse */
		rc = mnt_optstr_next_option(&p, op, maps, nmaps);
		if (rc == -1) {
			printf("parse error\n");
			goto done;
		}
		if (rc != 0)
			break;	/* end of string */
		mnt_option_print_debug(op, stdout, NULL);
	} while(1);

	rc = 0;
done:
	mnt_free_option(op);
	return rc;
}

int main(int argc, char *argv[])
{
	struct mtest tss[] = {
		{ "--find",  test_find, "<optstr> <name>  search name in optstr" },
		{ "--parse", test_parse,"<optstr>         list all option in optstr" },
		{ NULL }
	};
	return  mnt_run_test(tss, argc, argv);
}
#endif /* LIBMOUNT_TEST_PROGRAM */

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

#include "nls.h"
#include "mountP.h"

static inline char *skip_spaces(char *s)
{
	assert(s);

	while (*s == ' ' || *s == '\t')
		s++;
	return s;
}

static inline char *skip_nonspaces(char *s)
{
	assert(s);

	while (*s && !(*s == ' ' || *s == '\t'))
		s++;
	return s;
}

#define isoctal(a) (((a) & ~7) == '0')

/* returns malloced pointer - no more strdup required */
static void unmangle(char *s, char *buf, size_t len)
{
	size_t sz = 0;
	assert(s);

	while(*s && sz < len - 1) {
		if (*s == '\\' && sz + 4 < len - 1 && isoctal(s[1]) &&
		    isoctal(s[2]) && isoctal(s[3])) {

			*buf++ = 64*(s[1] & 7) + 8*(s[2] & 7) + (s[3] & 7);
			s += 4;
			sz += 4;
		} else {
			*buf++ = *s++;
			sz++;
		}
	}
	*buf = '\0';
}

static size_t next_word_size(char *s, char **start, char **end)
{
	char *e;

	assert(s);

	s = skip_spaces(s);
	if (!*s)
		return 0;
	e = skip_nonspaces(s);

	if (start)
		*start = s;
	if (end)
		*end = e;

	return e - s;
}

static char *next_word(char **s)
{
	size_t sz;
	char *res, *end;

	assert(s);

	sz = next_word_size(*s, s, &end) + 1;
	if (sz == 1)
		return NULL;

	res = malloc(sz);
	if (!res)
		return NULL;

	unmangle(*s, res, sz);
	*s = end + 1;
	return res;
}

static int next_word_skip(char **s)
{
	*s = skip_spaces(*s);
	if (!**s)
		return 1;
	*s = skip_nonspaces(*s);
	return 0;
}

static int next_number(char **s, int *num)
{
	char *end = NULL;

	assert(num);
	assert(s);

	*s = skip_spaces(*s);
	if (!**s)
		return -1;
	*num = strtol(*s, &end, 10);
	if (end == NULL || *s == end)
	       return -1;

	*s = end;

	/* valid end of number is space or terminator */
	if (*end == ' ' || *end == '\t' || *end == '\0')
		return 0;
	return -1;
}

/*
 * Parses one line from {fs,m}tab
 */
static int mnt_parse_tab_line(mentry *ent, char *s)
{
	/* SOURCE */
	if (__mnt_entry_set_source(ent, next_word(&s)) != 0)
		return 1;

	/* TARGET */
	ent->target = next_word(&s);
	if (!ent->target)
		return 1;

	/* TYPE */
	if (__mnt_entry_set_fstype(ent, next_word(&s)) != 0)
		return 1;

	/* OPTS */
	ent->optstr = next_word(&s);
	if (!ent->optstr)
		return 1;
	/* default */
	ent->passno = ent->freq = 0;

	/* FREQ (optional) */
	if (next_number(&s, &ent->freq) != 0) {
		if (*s)
			return 1;

	/* PASSNO (optional) */
	} else if (next_number(&s, &ent->passno) != 0 && *s)
		return 1;

	return 0;
}

/*
 * Parses one line from mountinfo file
 */
static int mnt_parse_mountinfo_line(mentry *ent, char *s)
{
	/* ID */
	if (next_number(&s, &ent->id) != 0)
		return 1;

	/* PARENT */
	if (next_number(&s, &ent->parent) != 0)
		return 1;

	/* <maj>:<min> (ignore) */
	if (next_word_skip(&s) != 0)
		return 1;

	/* MOUNTROOT */
	ent->mntroot = next_word(&s);
	if (!ent->mntroot)
		return 1;

	/* TARGET (mountpoit) */
	ent->target = next_word(&s);
	if (!ent->target)
		return 1;

	/* OPTIONS (fs-independent) */
	ent->optvfs = next_word(&s);
	if (!ent->optvfs)
		return 1;

	/* optional fields (ignore) */
	do {
		s = skip_spaces(s);
		if (s && *s == '-' &&
		    (*(s + 1) == ' ' || *(s + 1) == '\t')) {
			s++;
			break;
		}
		if (s && next_word_skip(&s) != 0)
			return 1;
	} while (s);

	/* FSTYPE */
	if (__mnt_entry_set_fstype(ent, next_word(&s)) != 0)
		return 1;

	/* SOURCE or "none" */
	if (__mnt_entry_set_source(ent, next_word(&s)) != 0)
		return 1;

	/* OPTIONS (fs-dependent) */
	ent->optfs = next_word(&s);
	if (!ent->optfs)
		return 1;

	return 0;
}

/*
 * Returns {m,fs}tab or mountinfo file format (MNT_FMT_*)
 *
 * The "mountinfo" format is always: "<number> <number> ... "
 */
static int mnt_detect_fmt(char *line)
{
	int num;

	/* ID */
	if (next_number(&line, &num) != 0)
		return MNT_FMT_TAB;

	/* PARENT */
	if (next_number(&line, &num) != 0)
		return MNT_FMT_TAB;

	return MNT_FMT_MOUNTINFO;
}

/*
 * Read and parse the next line from {fs,m}tab or mountinfo
 */
static int mnt_mfile_parse_next(mfile *mf, FILE *f, mentry *ent)
{
	char buf[BUFSIZ];
	char *s;

	assert(mf);
	assert(f);
	assert(ent);

	/* read the next non-blank non-comment line */
	do {
		if (fgets (buf, sizeof(buf), f) == NULL)
			return -1;
		mf->nlines++;
		s = index (buf, '\n');
		if (!s) {
			/* Missing final newline?  Otherwise extremely */
			/* long line - assume file was corrupted */
			if (feof(f)) {
				DBG(DEBUG_MFILE, fprintf(stderr,
					"libmount: WARNING: no final newline at the end of %s\n",
					mf->filename));
				s = index (buf, '\0');
			} else {
				DBG(DEBUG_MFILE, fprintf(stderr,
					"libmount: %s: %d: missing newline at line\n",
					mf->filename, mf->nlines));
				goto err;
			}
		}
		*s = '\0';
		if (--s >= buf && *s == '\r')
			*s = '\0';
		s = skip_spaces(buf);
	} while (*s == '\0' || *s == '#');

	DBG(DEBUG_MFILE, fprintf(stderr, "libmount: %s:%d: %s\n",
		mf->filename, mf->nlines, s));

	if (!mf->fmt)
		mf->fmt = mnt_detect_fmt(s);

	if (mf->fmt == MNT_FMT_TAB) {
		/* parse /etc/{fs,m}tab */
		if (mnt_parse_tab_line(ent, s) != 0)
			goto err;
	} else if (mf->fmt == MNT_FMT_MOUNTINFO) {
		/* parse /proc/self/mountinfo */
		if (mnt_parse_mountinfo_line(ent, s) != 0)
			goto err;
	}

	/* merge optfs and optvfs into optstr (necessary for "mountinfo") */
	if (!ent->optstr && (ent->optvfs || ent->optfs)) {
		ent->optstr = mnt_merge_optstr(ent->optvfs, ent->optfs);
		if (!ent->optstr)
			goto err;
	}

	ent->lineno = mf->nlines;

	DBG(DEBUG_MFILE, fprintf(stderr,
		"libmount: %s: %d: SOURCE:%s, MNTPOINT:%s, TYPE:%s, "
				  "OPTS:%s, FREQ:%d, PASSNO:%d\n",
		mf->filename, ent->lineno,
		ent->source, ent->target, ent->fstype,
		ent->optstr, ent->freq, ent->passno));

	return 0;
err:
	/* we don't report parse errors to caller; caller has to check
	 * errors by mnt_mfile_get_nerrs() or internaly by MNT_ENTRY_ERR flag
	 */
	ent->lineno = mf->nlines;
	ent->flags |= MNT_ENTRY_ERR;
	return 0;
}

/**
 * mnt_parse_mfile:
 * @mf: mfile pointer
 *
 * Parses whole mfile (e.g. /etc/fstab).
 *
 * Returns 0 on success and -1 in case of error. The parse errors is possible
 * to detect by mnt_mfile_get_nerrs() and error message is possible to create by
 * mnt_mfile_strerror().
 *
 * Example:
 *
 *	mfile *mf = mnt_new_mfile("/etc/fstab");
 *	int rc;
 *
 *	rc = mnt_parse_mfile(mf);
 *	if (rc) {
 *		if (mnt_mfile_get_nerrs(mf)) {             / * parse error * /
 *			mnt_mfile_strerror(mf, buf, sizeof(buf));
 *			fprintf(stderr, "%s: %s\n", progname, buf);
 *		} else
 *			perror(mnt_mfile_get_name(mf));  / * system error * /
 *	} else
 *		mnt_fprintf_mfile(mf, stdout, NULL);
 *
 *	mnt_free_mfile(mf);
 */
int mnt_parse_mfile(mfile *mf)
{
	FILE *f;

	assert(mf);
	assert(mf->filename);

	if (!mf->filename)
		return -1;

	f = fopen(mf->filename, "r");
	if (!f)
		return -1;

	while (!feof(f)) {
		int rc;
		mentry *ent = mnt_new_entry();
		if (!ent)
			goto error;

		rc = mnt_mfile_parse_next(mf, f, ent);
		if (!rc)
			rc = mnt_mfile_add_entry(mf, ent);
		else if (feof(f)) {
			mnt_free_entry(ent);
			break;
		}
		if (rc) {
			mnt_free_entry(ent);
			goto error;
		}
	}

	fclose(f);
	return 0;
error:
	fclose(f);
	return -1;
}

/**
 * mnt_new_mfile_parse:
 * @filename: /etc/{m,fs}tab or /proc/self/mountinfo path
 *
 * Same as mnt_new_mfile() + mnt_parse_mfile(). Note that this function does
 * not provide details (by mnt_mfile_strerror()) about failed parsing -- so you
 * should not to use this function for user-writeable files like /etc/fstab.
 *
 * Returns newly allocated mfile on success and NULL in case of error.
 */
mfile *mnt_new_mfile_parse(const char *filename)
{
	mfile *mf;

	assert(filename);

	if (!filename)
		return NULL;
	mf = mnt_new_mfile(filename);
	if (mf && mnt_parse_mfile(mf) != 0) {
		mnt_free_mfile(mf);
		mf = NULL;
	}
	return mf;
}

/**
 * mnt_mfile_get_nerrs:
 * @mf: pointer to mfile
 *
 * Returns number of broken (parse error) entries in the mfile.
 */
int mnt_mfile_get_nerrs(mfile *mf)
{
	assert(mf);
	return mf->nerrs;
}

/**
 * mnt_mfile_strerror:
 * @mf: pointer to mfile
 * @buf: buffer to return error message
 * @buflen: lenght of the buf
 *
 * Returns error message for mfile parse errors. For example:
 *
 *	"/etc/fstab: parse error at line(s): 1, 2 and 3."
 */
char *mnt_mfile_strerror(mfile *mf, char *buf, size_t buflen)
{
	struct list_head *p;
	int last = -1;
	char *b = buf;
	char *end = buf + buflen - 1;

	assert(mf);
	assert(buf);
	assert(buflen);

	if (!mf || !mf->nerrs || !buf || buflen <=0)
		return NULL;

	if (mf->filename) {
		snprintf(b, end - b, "%s: ", mf->filename);
		b += strnlen(b, end - b);
	}

	if (mf->nerrs > 1)
		strncpy(b, _("parse error at lines: "), end - b);
	else
		strncpy(b, _("parse error at line: "), end - b);
	b += strnlen(b, end - b);
	*b = '\0';

	list_for_each(p, &mf->ents) {
		mentry *ent = list_entry(p, mentry, ents);
		if (b == end)
			goto done;
		if (ent->flags & MNT_ENTRY_ERR) {
			if (last != -1) {
				snprintf(b, end - b, "%d, ", last);
				b += strnlen(b, end - b);
			}
			last = ent->lineno;
		}
	}

	if (mf->nerrs == 1)
		snprintf(b, end - b, "%d.", last);
	else
		snprintf(b - 1, end - b, _(" and %d."), last);
done:
	return buf;
}

#ifdef LIBMOUNT_TEST_PROGRAM
int test_mfile(struct mtest *ts, int argc, char *argv[])
{
	mfile *mf;
	mentry *ent;
	miter *itr;

	if (argc != 2)
		goto err;

	mf = mnt_new_mfile(argv[1]);
	if (!mf)
		goto err;
	if (mnt_parse_mfile(mf) != 0)
		goto err;
	if (mnt_mfile_get_nerrs(mf)) {
		char buf[BUFSIZ];

		mnt_mfile_strerror(mf, buf, sizeof(buf));
		printf("\t%s\n", buf);
		goto err;
	}

	itr = mnt_new_iter(MNT_ITER_FORWARD);
	if (!itr)
		goto err;
	while(mnt_iterate_entries(itr, mf, &ent) == 0) {
		const char *tg, *vl;

		if (mnt_entry_get_tag(ent, &tg, &vl) == 0)
			printf("%s=%s", tg, vl);
		else
			printf("%s", mnt_entry_get_srcpath(ent));

		printf("|%s|%s|%s|%d|%d|\n",
				mnt_entry_get_target(ent),
				mnt_entry_get_fstype(ent),
				mnt_entry_get_optstr(ent),
				mnt_entry_get_freq(ent),
				mnt_entry_get_passno(ent));
	}
	mnt_free_mfile(mf);
	mnt_free_iter(itr);

	return 0;
err:
	return -1;
}

int main(int argc, char *argv[])
{
	struct mtest tss[] = {
	{ "--mfile",    test_mfile, "<file>   parse the {fs,m}tab or mountinfo file" },
	{ NULL }
	};
	return mnt_run_test(tss, argc, argv);
}

#endif /* LIBMOUNT_TEST_PROGRAM */

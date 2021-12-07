/*
 * mountP.h - private library header file
 *
 * Copyright (C) 2008 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 */

#ifndef _LIBMOUNT_PRIVATE_H
#define _LIBMOUNT_PRIVATE_H

/* features */
#define CONFIG_LIBMOUNT_DEBUG
#define CONFIG_LIBMOUNT_ASSERT

#include <sys/types.h>

#ifdef CONFIG_LIBMOUNT_ASSERT
#include <assert.h>
#endif

#ifdef __GNUC__
#define _INLINE_ static __inline__
#else                         /* For Watcom C */
#define _INLINE_ static inline
#endif

#include "mount.h"
#include "list.h"

#define MNT_CRDOM_NOMEDIUM_RETRIES    5

/* TODO: move to some top-level util-linux include file */
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* utils.c */
extern char *mnt_getenv_safe(const char *arg);

#ifndef HAVE_STRNLEN
extern size_t strnlen(const char *s, size_t maxlen);
#endif
#ifndef HAVE_STRNDUP
extern char *strndup(const char *s, size_t n);
#endif
#ifndef HAVE_STRNCHR
char *strnchr(const char *s, size_t maxlen, int c);
#endif

/* debug stuff -- for more details see conf.c */
#if defined(LIBMOUNT_TEST_PROGRAM) && !defined(CONFIG_LIBMOUNT_DEBUG)
#define CONFIG_LIBMOUNT_DEBUG
#endif

#define DEBUG_INIT	(1 << 1)
#define DEBUG_SETTING	(1 << 2)
#define DEBUG_MFILE	(1 << 3)
#define DEBUG_OPTIONS	(1 << 4)
#define DEBUG_LOCKS	(1 << 5)
#define DEBUG_REQUEST	(1 << 6)

#define DEBUG_ALL	0xFFFF

#ifdef CONFIG_LIBMOUNT_DEBUG
#include <stdio.h>
extern int libmount_debug_mask;
extern void mnt_init_debug(int mask);
extern int mnt_option_print_debug(moption *op, FILE *file, const char *premesg);
#define DBG(m,x)	if ((m) & libmount_debug_mask) x;
#else
#define DBG(m,x)
#define mnt_init_debug(x)
#endif

/*
 * mentry flags
 */
#define MNT_ENTRY_ERR		(1 << 1) /* broken entry */
#define MNT_ENTRY_PSEUDOFS	(1 << 2) /* pseudo filesystem */
#define MNT_ENTRY_NETFS		(1 << 3) /* network filesystem */

/*
 * mfile entry
 *
 * This struct represents one entry in mtab/fstab file.
 */
struct _mentry {
	struct list_head ents;

	int		id;		/* mountinfo[1]: ID */
	int		parent;		/* moutninfo[2]: parent */

	char		*source;	/* fstab[1]: mountinfo[10]:
                                         * source dev, file, dir or TAG */
	char		*tagname;	/* fstab[1]: tag name - "LABEL", "UUID", ..*/
	char		*tagval;	/*           tag value */

	char		*mntroot;	/* mountinfo[4]: root of the mount within the FS */
	char		*target;	/* mountinfo[5], fstab[2]: mountpoint */
	char		*fstype;	/* mountinfo[9], fstab[3]: filesystem type */

	char		*optstr;	/* mountinfo[6,11], fstab[4]: option string */
	char		*optvfs;	/* mountinfo[6]: fs-independent (VFS) options */
	char		*optfs;		/* mountinfo[11]: fs-depend options */

	int		freq;		/* fstab[5]:  dump frequency in days */
	int		passno;		/* fstab[6]: pass number on parallel fsck */

	int		flags;		/* MNT_ENTRY_* flags */
	int		lineno;		/* line number in the parental mfile */
};

extern int __mnt_entry_set_source(mentry *ent, char *source);
extern int __mnt_entry_set_fstype(mentry *ent, char *fstype);

/*
 * File format
 */
enum {
       MNT_FMT_TAB = 1,                /* /etc/{fs,m}tab */
       MNT_FMT_MOUNTINFO               /* /proc/#/mountinfo */
};


/*
 * mtab/fstab file
 */
struct _mfile {
	char		*filename;	/* file name or NULL */
	int		fmt;		/* MNT_FMT_* file format */

	int		nlines;		/* number of lines in the file (include commentrys) */
	int		nents;		/* number of valid entries */
	int		nerrs;		/* number of broken entries (parse errors) */

	mpcache		*pcache;	/* canonicalized paths cache */

	struct list_head	ents;	/* list of entries (mentry) */
};

/*
 * Generic iterator
 */
struct _miter {
        struct list_head        *p;		/* current position */
        struct list_head        *head;		/* start position */
	int			direction;	/* MNT_MITER_{FOR,BACK}WARD */
};

#define IS_ITER_FORWARD(_i)	((_i)->direction == MNT_ITER_FORWARD)
#define IS_ITER_BACKWARD(_i)	((_i)->direction == MNT_ITER_BACKWARD)

#define MNT_MITER_INIT(itr, list) \
	do { \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(list)->next : (list)->prev; \
		(itr)->head = (list); \
	} while(0)

#define MNT_MITER_ITERATE(itr, res, restype, member) \
	do { \
		res = list_entry((itr)->p, restype, member); \
		(itr)->p = IS_ITER_FORWARD(itr) ? \
				(itr)->p->next : (itr)->p->prev; \
	} while(0)


/* private option masks */
#define MNT_HASVAL	(1 << 20)	/* option has value (e.g. name=<value>) */

/*
 * mount option value
 */
struct _moption {
	char			*name;	/* option name (allcocated when mapent is NULL) */

	char			*string; /* option argument value */
	int			number;	 /* option argument value */

	int			mask;	/* MNT_{INVMASK,MDATA,MFLAG,NOMTAB,NOSYS}
					 * modifiable flags (initial value comes from map->mask)
					 */
	const struct mopt_map	*mapent;/* the option description (msp entry) */
	const struct mopt_map	*map;   /* head of the map */

	struct list_head	opts;	/* list of options */
};

/*
 * top-level container for all options
 */
struct _mopts {
	struct mopt_map const	**maps;	/* array with option maps */
	size_t			nmaps;	/* number of maps */

	struct list_head	opts;	/* list of options */
};

extern char *mnt_merge_optstr(const char *vfs, const char *fs);
extern int __mnt_init_option(moption *op, const char *name, size_t namelen,
                struct mopt_map const **maps, int nmaps);
extern int mnt_reinit_extra_option(moption *op, struct mopt_map const *map);

#ifdef LIBMOUNT_TEST_PROGRAM
struct mtest {
	const char	*name;
	int		(*body)(struct mtest *ts, int argc, char *argv[]);
	const char	*usage;
};

/* utils.c */
extern int mnt_run_test(struct mtest *tests, int argc, char *argv[]);
#endif

#endif /* _LIBMOUNT_PRIVATE_H */


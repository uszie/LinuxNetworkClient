/*
 * mount.h  - libmount API
 *
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#ifndef _LIBMOUNT_MOUNT_H
#define _LIBMOUNT_MOUNT_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _mentry		mentry;
typedef struct _mfile		mfile;
typedef struct _miter		miter;
typedef struct _mopts		mopts;
typedef struct _moption		moption;
typedef struct _mpcache		mpcache;
typedef struct _mlock		mlock;
typedef struct _mrequest	mrequest;

/* request.c */

/* task.c */
enum {
	MNT_TASK_MOUNT,
	MNT_TASK_UMOUNT,
	MNT_TASK_SWAPON,
	MNT_TASK_SWAPOFF
};

/* mtab.c */
extern int mnt_mtab_remove_line(const char *target);
extern int mnt_mtab_append_line(const char *source, const char *target,
                         const char *fstype, const char *options);
extern int mnt_mtab_modify_options(const char *target, const char *options);
extern int mnt_mtab_modify_target(const char *old_target, const char *new_target);

/* lock.c */
extern mlock *mnt_new_lock(const char *lockfile, pid_t id);
extern void mnt_free_lock(mlock *ml);
extern const char *mnt_lock_get_lockfile(mlock *ml);
extern const char *mnt_lock_get_linkfile(mlock *ml);

extern void mnt_unlock(mlock *ml);
extern int mnt_lock(mlock *ml);

/* version.c */
extern int mnt_parse_version_string(const char *ver_string);
extern int mnt_get_library_version(const char **ver_string);

/* utils.c */
extern int mnt_fstype_is_netfs(const char *type);
extern int mnt_fstype_is_pseudofs(const char *type);
extern char *mnt_strconcat3(char *s, const char *t, const char *u);
extern int mnt_open_device(const char *devname, int flags);
extern char *mnt_get_username(const uid_t uid);

/* mfile.c */
extern mfile *mnt_new_mfile(const char *filename);
extern void mnt_free_mfile(mfile *mf);
extern int mnt_mfile_get_nents(mfile *mf);
extern int mnt_mfile_set_pcache(mfile *mf, mpcache *mpc);
extern mpcache *mnt_mfile_get_pcache(mfile *mf);
extern const char *mnt_mfile_get_name(mfile *mf);
extern int mnt_mfile_add_entry(mfile *mf, mentry *ent);
extern int mnt_mfile_remove_entry(mfile *mf, mentry *ent);
extern int mnt_iterate_entries(miter *itr, mfile *mf, mentry **ent);
extern mentry *mnt_mfile_find_srcpath(mfile *mf, const char *path, int direction);
extern mentry *mnt_mfile_find_target(mfile *mf, const char *path, int direction);
extern mentry *mnt_mfile_find_source(mfile *mf, const char *path, int direction);
extern mentry *mnt_mfile_find_tag(mfile *mf, const char *tag,
                        const char *val, int direction);
extern mentry *mnt_mfile_find_pair(mfile *mf, const char *srcpath,
                        const char *target, int direction);
extern int mnt_fprintf_mfile(FILE *f, const char *fmt, mfile *mf);
extern int mnt_rewrite_mfile(mfile *mf);

/* mfile-parse.c */
extern mfile *mnt_new_mfile_parse(const char *filename);
extern int mnt_parse_mfile(mfile *mf);
extern char *mnt_mfile_strerror(mfile *mf, char *buf, size_t buflen);
extern int mnt_mfile_get_nerrs(mfile *mf);

/* pcache.c */
extern mpcache *mnt_new_pcache(void);
extern void mnt_free_pcache(mpcache *mpc);
extern const char *mnt_pcache_find_path(mpcache *mpc, const char *path);
extern const char *mnt_pcache_find_tag(mpcache *mpc,
			const char *token, const char *value);
extern char *mnt_resolve_path(const char *path, mpcache *mpc);
extern char *mnt_resolve_tag(const char *token, const char *value, mpcache *mpc);

/* entry.c */
extern mentry *mnt_new_entry(void);
extern void mnt_free_entry(mentry *ent);
extern const char *mnt_entry_get_source(mentry *ent);
extern int mnt_entry_set_source(mentry *ent, const char *source);
extern const char *mnt_entry_get_srcpath(mentry *ent);
extern int mnt_entry_get_tag(mentry *ent, const char **name, const char **value);
extern const char *mnt_entry_get_target(mentry *ent);
extern int mnt_entry_set_target(mentry *ent, const char *target);
extern const char *mnt_entry_get_fstype(mentry *ent);
extern int mnt_entry_set_fstype(mentry *ent, const char *fstype);
extern const char *mnt_entry_get_optstr(mentry *ent);
extern int mnt_entry_set_optstr(mentry *ent, const char *optstr);
extern const char *mnt_entry_get_optfs(mentry *ent);
extern const char *mnt_entry_get_optvfs(mentry *ent);
extern int mnt_entry_get_freq(mentry *ent);
extern int mnt_entry_set_freq(mentry *ent, int freq);
extern int mnt_entry_get_passno(mentry *ent);
extern int mnt_entry_set_passno(mentry *ent, int passno);
extern int mnt_entry_is_bind(mentry *ent);
extern int mnt_entry_get_option(mentry *ent, const char *name,
			const char **value, size_t *valsz);

/* mtab/fstab line */
#define MNT_MFILE_PRINTFMT	"%s %s %s %s %d %d\n"

extern int mnt_fprintf_line(
                        FILE *f,
                        const char *fmt,
                        const char *source,
                        const char *target,
                        const char *fstype,
                        const char *options,
                        int freq,
                        int passno);

extern int mnt_fprintf_entry(FILE *f, const char *fmt, mentry *ent);

/* iter.c */
enum {

	MNT_ITER_FORWARD = 0,
	MNT_ITER_BACKWARD
};
extern miter *mnt_new_iter(int direction);
extern void mnt_free_iter(miter *mi);
extern void mnt_reset_iter(miter *mi, int direction);

extern int mnt_iterate_entries(miter *mi, mfile *mf, mentry **ent);

/* options.c */
extern const struct mopt_map *mnt_get_builtin_map(int id);
extern moption *mnt_new_option(void);
extern void mnt_reset_option(moption *op);
extern void mnt_free_option(moption *op);
extern int mnt_option_get_type(moption *op);
extern int mnt_init_option(moption *op, const char *name,
                struct mopt_map const **maps, int nmaps);
extern int mnt_option_set_value(moption *op, char *rawdata, size_t len);
extern int mnt_option_has_value(moption *option);
extern int mnt_option_require_value(moption *op);
extern int mnt_option_is_inverted(moption *op);
extern int mnt_option_get_number(moption *option, int *number);
extern int mnt_option_get_string(moption *option, const char **str);
extern char *mnt_option_dup_string(moption *option, char **str);
extern const char *mnt_option_get_name(moption *op);
extern int mnt_option_get_mask(moption *op);
extern int mnt_option_get_valsiz(moption *op);
extern int mnt_option_snprintf_value(moption *op, char *str, size_t size);
extern int mnt_option_get_id(moption *op);
extern int mnt_option_get_flag(moption *op, int *flags);
extern int mnt_option_is_extra(moption *op);
extern const struct mopt_map *mnt_option_get_map(moption *op);
extern const struct mopt_map *mnt_option_get_mapent(moption *op);

/* optstr.c */
int mnt_optstr_next_option(const char **optstr, moption *op,
                struct mopt_map const **maps, int nmaps);
int mnt_optstr_find_option(const char *optstr, const char *name,
                                const char **value, size_t *valsz);
char *mnt_optstr_append_option(char *optstr, moption *op);

/* opts.c */
int mnt_opts_add_map(mopts *opts, const struct mopt_map *map);
int mnt_opts_add_builtin_map(mopts *opts, int map_id);
mopts *mnt_new_opts(void);
void mnt_free_opts(mopts *opts);
int mnt_opts_remove_option_by_flags(mopts *opts,
		const struct mopt_map *map, const int flags);
int mnt_opts_remove_option_by_iflags(mopts *opts,
		const struct mopt_map *map, const int flags);
int mnt_opts_remove_option_by_name(mopts *opts, const char *name);
int mnt_opts_parse(mopts *opts, const char *optstr);
int mnt_iterate_options(miter *itr, mopts *opts,
                const struct mopt_map *map, moption **option);
moption *mnt_opts_get_option(mopts *opts, const char *name);
int mnt_opts_get_ids(mopts *opts, const struct mopt_map *map);
int mnt_opts_create_mountflags(mopts *opts);
char *mnt_opts_create_mountdata(mopts *opts);
char *mnt_opts_create_mtab_optstr(mopts *opts);
char *mnt_opts_create_userspace_optstr(mopts *opts);


/*
 * mount options map masks
 */
#define MNT_MFLAG	(1 << 1) /* use the mask as mount(2) flag */
#define MNT_MDATA	(1 << 2) /* use the option as mount(2) data */
#define MNT_INVERT	(1 << 3) /* invert the mountflag */
#define MNT_NOMTAB	(1 << 4) /* skip in the mtab option string */

/*
 * mount option description
 *
 * The libmount supports mount options with values in %<type>:
 *	%s, %d, %u, %o, %x
 */
struct mopt_map {
	const char	*name;	 /* option name[=%<type>] (e.g. "loop[=%s]") */
	int		id;	 /* option ID or MS_* flags (e.g MS_RDONLY) */
	int		mask;	 /* MNT_{MFLAG,MDATA,INVMASK,...} mask */
};

/*
 * Built-in options maps
 */
enum {
	MNT_LINUX_MAP = 1,
	MNT_USERSPACE_MAP
};

/*
 * mount(8) userspace options masks (MNT_MAP_USERSPACE map)
 */
#define MNT_MS_DFLTS	(1 << 1)
#define MNT_MS_NOAUTO	(1 << 2)
#define MNT_MS_USER	(1 << 3)
#define MNT_MS_USERS	(1 << 4)
#define MNT_MS_OWNER	(1 << 5)
#define MNT_MS_GROUP	(1 << 6)
#define MNT_MS_NETDEV	(1 << 7)
#define MNT_MS_COMMENT  (1 << 8)
#define MNT_MS_LOOP     (1 << 9)
#define MNT_MS_NOFAIL   (1 << 10)

/*
 * mount(2) MS_* masks (MNT_MAP_LINUX map)
 */
#ifndef MS_RDONLY
#define MS_RDONLY	 1	/* Mount read-only */
#endif
#ifndef MS_NOSUID
#define MS_NOSUID	 2	/* Ignore suid and sgid bits */
#endif
#ifndef MS_NODEV
#define MS_NODEV	 4	/* Disallow access to device special files */
#endif
#ifndef MS_NOEXEC
#define MS_NOEXEC	 8	/* Disallow program execution */
#endif
#ifndef MS_SYNCHRONOUS
#define MS_SYNCHRONOUS	16	/* Writes are synced at once */
#endif
#ifndef MS_REMOUNT
#define MS_REMOUNT	32	/* Alter flags of a mounted FS */
#endif
#ifndef MS_MANDLOCK
#define MS_MANDLOCK	64	/* Allow mandatory locks on an FS */
#endif
#ifndef MS_DIRSYNC
#define MS_DIRSYNC	128	/* Directory modifications are synchronous */
#endif
#ifndef MS_NOATIME
#define MS_NOATIME	0x400	/* 1024: Do not update access times. */
#endif
#ifndef MS_NODIRATIME
#define MS_NODIRATIME   0x800	/* 2048: Don't update directory access times */
#endif
#ifndef MS_BIND
#define	MS_BIND		0x1000	/* 4096: Mount existing tree also elsewhere */
#endif
#ifndef MS_MOVE
#define MS_MOVE		0x2000	/* 8192: Atomically move tree */
#endif
#ifndef MS_REC
#define MS_REC		0x4000	/* 16384: Recursive loopback */
#endif
#ifndef MS_VERBOSE
#define MS_VERBOSE	0x8000	/* 32768 */
#endif
#ifndef MS_RELATIME
#define MS_RELATIME	0x200000 /* 200000: Update access times relative
                                  to mtime/ctime */
#endif
#ifndef MS_UNBINDABLE
#define MS_UNBINDABLE	(1<<17)	/* 131072 unbindable*/
#endif
#ifndef MS_PRIVATE
#define MS_PRIVATE	(1<<18)	/* 262144 Private*/
#endif
#ifndef MS_SLAVE
#define MS_SLAVE	(1<<19)	/* 524288 Slave*/
#endif
#ifndef MS_SHARED
#define MS_SHARED	(1<<20)	/* 1048576 Shared*/
#endif
#ifndef MS_I_VERSION
#define MS_I_VERSION	(1<<23)	/* update inode I_version field */
#endif
#ifndef MS_STRICTATIME
#define MS_STRICTATIME	(1<<24) /* strict atime semantics */
#endif

/*
 * Magic mount flag number. Had to be or-ed to the flag values.
 */
#ifndef MS_MGC_VAL
#define MS_MGC_VAL 0xC0ED0000	/* magic flag number to indicate "new" flags */
#endif
#ifndef MS_MGC_MSK
#define MS_MGC_MSK 0xffff0000	/* magic flag number mask */
#endif


/* Shared-subtree options */
#define MS_PROPAGATION  (MS_SHARED|MS_SLAVE|MS_UNBINDABLE|MS_PRIVATE)

/* Options that we make ordinary users have by default.  */
#define MS_SECURE	(MS_NOEXEC|MS_NOSUID|MS_NODEV)

/* Options that we make owner-mounted devices have by default */
#define MS_OWNERSECURE	(MS_NOSUID|MS_NODEV)


#ifdef __cplusplus
}
#endif

#endif /* _LIBMOUNT_MOUNT_H_ */

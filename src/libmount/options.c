/*
 * Copyright (C) 2008-2009 Karel Zak <kzak@redhat.com>
 *
 * This file may be redistributed under the terms of the
 * GNU Lesser General Public License.
 *
 * The options.c contains basic routines for work with options maps
 * and a single option (type moption).
 *
 * Option-maps
 * -----------
 * The mount(2) linux syscall uses two arguments for mount options:
 *
 *    1) mountflags (see MS_* macros in linux/fs.h)
 *    2) mountdata  (usully a comma separated string of options)
 *
 * The libmount uses options-map(s) to describe mount options. The number of
 * maps is unlimited. The libmount options parser could be easily extended
 * (e.g. by mnt_opts_add_map()) to work with new options.
 *
 * The option description (map entry) includes:
 *
 *    - option name and argument type (e.g. "loop[=%s]")
 *    - option ID (in the map unique identifier or a mountflags, e.g MS_RDONLY)
 *    - mask (MNT_INVERT, MNT_MDATA, MNT_MFLAG, MNT_NOMTAB)
 *
 * The option argument type is defined by:
 *
 *     "=<type>"          -- required argument
 *     "[=<type>]"        -- optional argument
 *
 * where the <type> is:
 *
 *     %s                 -- string
 *     %d                 -- number (int)
 *     %u                 -- unsigned number
 *     %o                 -- number in octal format
 *     %x                 -- number in hexadecimal format
 *     {item0,item1,...}  -- enum, converted to 0..N number
 *
 * The libmount defines two basic built-in options maps:
 *
 *    - MNT_LINUX_MAP     -- fs-independent kernel mount options (usually MS_* flags)
 *    - MNT_USERSPACE_MAP -- userspace specific mount options (e.g. "user", "loop")
 *
 * Options parser
 * --------------
 * The libmount support two way how work with mount options.
 *
 * 1/ "option string" (optstr) -- all options are stored in comma delimited
 *    string. mnt_optstr_next_option() is a low-level parser that returns
 *    one "moption". The parser does not store previously parsed options.
 *
 *    while(mnt_optstr_next_option(str, &op, maps, nmaps) == 0) {
 *        ...
 *    }
 *
 *    See also optstr.c
 *
 * 2/ "options container" (opts) -- all parsed options are stored in "mopt"
 *    container. You can add/remove/modify options in the container. It's
 *    possible to generate "mountflags", "mountdata" or mtab-like options
 *    string from the container. See the example below.
 *
 *    See also opts.c
 *
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "nls.h"
#include "mountP.h"

/*
 * fs-independent mount flags (built-in MNT_LINUX_MAP)
 */
static const struct mopt_map linux_flags_map[] =
{
   { "ro",       MS_RDONLY, MNT_MFLAG },               /* read-only */
   { "rw",       MS_RDONLY, MNT_MFLAG | MNT_INVERT },  /* read-write */
   { "exec",     MS_NOEXEC, MNT_MFLAG | MNT_INVERT },  /* permit execution of binaries */
   { "noexec",   MS_NOEXEC, MNT_MFLAG },               /* don't execute binaries */
   { "suid",     MS_NOSUID, MNT_MFLAG | MNT_INVERT },  /* honor suid executables */
   { "nosuid",   MS_NOSUID, MNT_MFLAG },               /* don't honor suid executables */
   { "dev",      MS_NODEV,  MNT_MFLAG | MNT_INVERT },  /* interpret device files  */
   { "nodev",    MS_NODEV,  MNT_MFLAG },               /* don't interpret devices */

   { "sync",     MS_SYNCHRONOUS, MNT_MFLAG },          /* synchronous I/O */
   { "async",    MS_SYNCHRONOUS, MNT_MFLAG | MNT_INVERT }, /* asynchronous I/O */

   { "dirsync",  MS_DIRSYNC, MNT_MFLAG },              /* synchronous directory modifications */
   { "remount",  MS_REMOUNT, MNT_MFLAG },              /* Alter flags of mounted FS */
   { "bind",     MS_BIND,    MNT_MFLAG },              /* Remount part of tree elsewhere */
   { "rbind",    MS_BIND|MS_REC, MNT_MFLAG },          /* Idem, plus mounted subtrees */
#ifdef MS_NOSUB
   { "sub",      MS_NOSUB,  MNT_MFLAG | MNT_INVERT },  /* allow submounts */
   { "nosub",    MS_NOSUB,  MNT_MFLAG },               /* don't allow submounts */
#endif
#ifdef MS_SILENT
   { "quiet",	 MS_SILENT, MNT_MFLAG },               /* be quiet  */
   { "loud",     MS_SILENT, MNT_MFLAG | MNT_INVERT },  /* print out messages. */
#endif
#ifdef MS_MANDLOCK
   { "mand",     MS_MANDLOCK, MNT_MFLAG },             /* Allow mandatory locks on this FS */
   { "nomand",   MS_MANDLOCK, MNT_MFLAG | MNT_INVERT },/* Forbid mandatory locks on this FS */
#endif
#ifdef MS_NOATIME
   { "atime",    MS_NOATIME, MNT_MFLAG | MNT_INVERT }, /* Update access time */
   { "noatime",	 MS_NOATIME, MNT_MFLAG },              /* Do not update access time */
#endif
#ifdef MS_I_VERSION
   { "iversion", MS_I_VERSION,   MNT_MFLAG },          /* Update inode I_version time */
   { "noiversion", MS_I_VERSION, MNT_MFLAG | MNT_INVERT}, /* Don't update inode I_version time */
#endif
#ifdef MS_NODIRATIME
   { "diratime", MS_NODIRATIME,   MNT_MFLAG | MNT_INVERT }, /* Update dir access times */
   { "nodiratime", MS_NODIRATIME, MNT_MFLAG },         /* Do not update dir access times */
#endif
#ifdef MS_RELATIME
   { "relatime", MS_RELATIME,   MNT_MFLAG },           /* Update access times relative to mtime/ctime */
   { "norelatime", MS_RELATIME, MNT_MFLAG | MNT_INVERT }, /* Update access time without regard to mtime/ctime */
#endif
#ifdef MS_STRICTATIME
   { "strictatime", MS_STRICTATIME, MNT_MFLAG },       /* Strict atime semantics */
   { "nostrictatime", MS_STRICTATIME, MNT_MFLAG | MNT_INVERT }, /* kernel default atime */
#endif
   { NULL, 0, 0 }
};

/*
 * userspace mount option (built-in MNT_USERSPACE_MAP)
 */
static const struct mopt_map userspace_opts_map[] =
{
   { "defaults", MNT_MS_DFLTS, MNT_NOMTAB },               /* default options */

   { "auto",    MNT_MS_NOAUTO, MNT_INVERT | MNT_NOMTAB },  /* Can be mounted using -a */
   { "noauto",  MNT_MS_NOAUTO, MNT_NOMTAB },               /* Can  only be mounted explicitly */

   { "user[=%s]", MNT_MS_USER },                           /* Allow ordinary user to mount (mtab) */
   { "nouser",  MNT_MS_USER, MNT_INVERT | MNT_NOMTAB },    /* Forbid ordinary user to mount */

   { "users",   MNT_MS_USERS, MNT_NOMTAB },                /* Allow ordinary users to mount */
   { "nousers", MNT_MS_USERS, MNT_INVERT | MNT_NOMTAB },   /* Forbid ordinary users to mount */

   { "owner",   MNT_MS_OWNER, MNT_NOMTAB },                /* Let the owner of the device mount */
   { "noowner", MNT_MS_OWNER, MNT_INVERT | MNT_NOMTAB },   /* Device owner has no special privs */

   { "group",   MNT_MS_GROUP, MNT_NOMTAB },                /* Let the group of the device mount */
   { "nogroup", MNT_MS_GROUP, MNT_INVERT | MNT_NOMTAB },   /* Device group has no special privs */

   { "_netdev", MNT_MS_NETDEV },                           /* Device requires network */

   { "comment=%s", MNT_MS_COMMENT, MNT_NOMTAB },           /* fstab comment only */

   { "loop[=%s]", MNT_MS_LOOP },                           /* use the loop device */

   { "nofail",  MNT_MS_NOFAIL, MNT_NOMTAB },               /* Do not fail if ENOENT on dev */

   { NULL, 0, 0 }
};

/**
 * mnt_get_builtin_map:
 * @id: map id -- MNT_LINUX_MAP or MNT_USERSPACE_MAP
 *
 * MNT_LINUX_MAP - Linux kernel fs-independent mount options
 *                 (usually MS_* flags, see linux/fs.h)
 *
 * MNT_USERSPACE_MAP - userpace mount(8) specific mount options
 *                     (e.g user=, _netdev, ...)
 *
 * Returns internal (static) libmount map.
 */
const struct mopt_map *mnt_get_builtin_map(int id)
{
	assert(id);

	if (id == MNT_LINUX_MAP)
		return linux_flags_map;
	else if (id == MNT_USERSPACE_MAP)
		return userspace_opts_map;
	return NULL;
}

/**
 * mnt_new_option:
 *
 * Returns a new options.
 */
moption *mnt_new_option(void)
{
	moption *op;

	op = calloc(1, sizeof(*op));
	if (!op)
		return NULL;
	INIT_LIST_HEAD(&op->opts);
	return op;
}

void mnt_reset_option(moption *op)
{
	assert(op);
	if (op->mapent == NULL || op->mapent->name != op->name)
		free(op->name);
	free(op->string);
	if (!list_empty(&op->opts))
		list_del(&op->opts);
	memset(op, 0, sizeof(*op));
	INIT_LIST_HEAD(&op->opts);
}

/**
 * mnt_free_option
 * @op: moption instance
 *
 * Deallocates the option.
 */
void mnt_free_option(moption *op)
{
	if (!op)
		return;
	mnt_reset_option(op);
	free(op);
}

/*
 * Look up for the @name in @maps and returns relevant map entry or NULL
 */
static const struct mopt_map *
mnt_optmaps_get_map(struct mopt_map const **maps,
		int nmaps,
		const char *name,
		size_t namelen,
		const struct mopt_map **mapent)
{
	int i;

	assert(maps);
	assert(nmaps);
	assert(name);
	assert(namelen);
	assert(mapent);

	*mapent = NULL;

	for (i = 0; i < nmaps; i++) {
		const struct mopt_map *map = maps[i];
		const struct mopt_map *ent;
		const char *p;

		for (ent = map; ent && ent->name; ent++) {
			if (strncmp(ent->name, name, namelen))
				continue;
			p = ent->name + namelen;
			if (*p == '\0' || *p == '=' || *p == '[') {
				*mapent = ent;
				return map;
			}
		}
	}
	return NULL;
}

/*
 * converts @rawdata to number according to enum definition in
 * the option @mapent.
 */
static int mnt_mapent_enum_to_number(const struct mopt_map *mapent,
		char *rawdata, size_t len)
{
	const char *p, *end, *begin;
	int n = -1;

	if (!rawdata || !*rawdata || !mapent || !len)
		return -1;

	p = strrchr(mapent->name, '=');
	if (!p || *(p + 1) == '{')
		return -1;	/* value unexpected or not "enum" */
	p += 2;
	if (!*p || *(p + 1) == '}')
		return -1;	/* hmm... option <type> is "={" or "={}" */

	/* we cannot use strstr(), @rawdata is not terminated */
	for (; p && *p; p++) {
		if (!begin)
			begin = p;		/* begin of the item */
		if (*p == ',')
			end = p;		/* terminate the item */
		if (*(p + 1) == '}')
			end = p + 1;		/* end of enum definition */
		if (!begin || !end)
			continue;
		if (end <= begin)
			return -1;
		n++;
		if (len == end - begin && strncasecmp(begin, rawdata, len) == 0)
			return n;
		p = end;
	}
	return -1;
}

static int mnt_mapent_number_to_enum(const struct mopt_map *mapent, int number,
				const char **elem, size_t *len)
{
	const char *p, *end, *begin;
	int n = -1;

	assert(mapent);
	assert(elem);
	assert(len);

	if (!mapent && !elem && !len)
		return -1;

	p = strrchr(mapent->name, '=');
	if (!p || *(p + 1) == '{')
		return -1;	/* value unexpected or not "enum" */
	p += 2;
	if (!*p || *(p + 1) == '}')
		return -1;	/* hmm... option <type> is "={" or "={}" */

	/* we cannot use strstr(), @rawdata is not terminated */
	for (; p && *p; p++) {
		if (!begin)
			begin = p;		/* begin of the item */
		if (*p == ',')
			end = p;		/* terminate the item */
		if (*(p + 1) == '}')
			end = p + 1;		/* end of enum definition */
		if (!begin || !end)
			continue;
		if (end <= begin)
			return -1;
		n++;
		if (n == number) {
			*elem = begin;
			*len = end - begin;
			return 0;
		}
		p = end;
	}
	return -1;
}

static int mnt_mapent_get_type(const struct mopt_map *mapent)
{
	char *type;

	assert(mapent);
	assert(mapent->name);

	type = strrchr(mapent->name, '=');
	if (!type)
		return -1;			/* value is unexpected */
	if (type == mapent->name)
		return -1;			/* wrong format of type definition */
	type++;
	if (*type != '%')
		return -1;			/* wrong format of type definition */
	type++;
	return *type ? *type : -1;
}

static int mnt_mapent_require_value(const struct mopt_map *mapent)
{
	char *type;

	assert(mapent);
	assert(mapent->name);

	type = strchr(mapent->name, '=');
	if (!type)
		return 0;			/* value is unexpected */
	if (type == mapent->name)
		return 0;			/* wrong format of type definition */
	if (*(type - 1) == '[')
		return 0;			/* optional */
	return 1;
}


/**
 * mnt_option_get_type:
 * @op: moption instance
 *
 * Notes:
 *  - 's' (string) is default for all extra (unknown) options.
 *  - '{' is "enum", converted to unsigned number
 *  - 'd', 'u', 'o', 'x' and '{' are accessible by mnt_option_get_number()
 *  - 's' is accessible by mnt_option_get_string()
 *
 * Returns 'd', 'u', 'o', 'x', 's', '{' or -1 on case on error.
 */
int mnt_option_get_type(moption *op)
{
	assert(op);
	if (!op)
		return -1;
	return op->mapent ? mnt_mapent_get_type(op->mapent) : 's';
}

int __mnt_init_option(moption *op, const char *name, size_t namelen,
		struct mopt_map const **maps, int nmaps)
{
	const struct mopt_map *mapent = NULL, *map = NULL;

	assert(op);
	assert(name);

	if (!op || !name)
		return -1;

	if (nmaps && maps)
		map = mnt_optmaps_get_map(maps, nmaps, name, namelen, &mapent);

	if (mapent == NULL || mnt_mapent_get_type(mapent) != -1)
		/* we allocate the name for uknown options of for options with
		 * "=%<type>" argument. This is not perfect... */
		op->name = strndup(name, namelen);
	else
		op->name = (char *) mapent->name;

	op->mapent = mapent;
	op->map = map;
	op->mask = mapent ? mapent->mask : 0;

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: option %s: initialized\n", op->name));

	return 0;
}

/* reinit option for the given map (see mnt_opts_reparse_extra()) */
int mnt_reinit_extra_option(moption *op, struct mopt_map const *map)
{
	const struct mopt_map *mapent = NULL;
	char *oldval;
	int rc = 0, type;

	assert(op);
	assert(op->name);
	assert(mnt_option_is_extra(op));

	if (!op || !map)
		return -1;

	map = mnt_optmaps_get_map(&map, 1, op->name, strlen(op->name), &mapent);
	if (!map)
		/* the option does not belong to the map -- ignore */
		return 0;

	if (mnt_mapent_get_type(mapent) == -1) {
		/* we can use the name from map */
		free(op->name);
		op->name = (char *) mapent->name;
	}

	op->mapent = mapent;
	op->map = map;
	op->mask = mapent ? mapent->mask : 0;

	/* extra options always use "string" as type */
	oldval = op->string;

	/* the new type */
	type = mnt_option_get_type(op);

	if (type == -1 && oldval)
		return -1;		/* value is unexpected */
	if (mnt_option_require_value(op) && !oldval)
		return -1;		/* value is required */

	if (oldval) {
		/* convert "oldval" string to the new type */
		if (type != 's') {
			op->string = NULL;
			rc = mnt_option_set_value(op, oldval, strlen(oldval));
			free(oldval);
		} else
			op->mask |= MNT_HASVAL;
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: option %s: reinitialized\n", op->name));
	return rc;
}

/**
 * mnt_init_option:
 * @op: moption instance
 * @name: option name
 * @maps: array of options maps (or NULL)
 * @nmaps: number of members in @maps array or 0
 *
 * Sets option name and associate the option with an option map.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_init_option(moption *op, const char *name,
		struct mopt_map const **maps, int nmaps)
{
	if (!name)
		return -1;
	return __mnt_init_option(op, name, strlen(name), maps, nmaps);
}


/**
 * mnt_option_set_value:
 * @op: moption instance
 * @rawdata: pointer to string with raw option data or NULL
 * @len: lenght of rawdata or 0.
 *
 * Converts @rawdata to %s, %d, %u, %o or %x format according to
 * options map. The default is %s for unknown options.
 *
 * The function unset (zeroize) the option value if the @rawdata pointer is NULL.
 *
 * Returns 0 on success or -1 in case of error.
 */
int mnt_option_set_value(moption *op, char *rawdata, size_t len)
{
	int type;
	char *end = NULL;
	int n;

	assert(op);
	if (!op)
		return -1;

	free(op->string);
	op->number = 0;
	op->string = NULL;
	op->mask &= ~MNT_HASVAL;

	if (!rawdata)
		return 0;	/* zeroize only */
	type = mnt_option_get_type(op);
	if (type == -1)
		return -1;			/* value is unexpected */

	switch(type) {
	case 's':
		op->string = strndup(rawdata, len);
		if (!op->string)
			return -1;
		break;
	case '{':
		n = mnt_mapent_enum_to_number(op->mapent, rawdata, len);
		if (n == -1)
			return -1;
		op->number = n;
		break;
	case 'd':
		n = strtol(rawdata, &end, 10);
		goto num;
	case 'u':
		n = strtoul(rawdata, &end, 10);
		goto num;
	case 'o':
		n = strtoul(rawdata, &end, 8);
		goto num;
	case 'x':
		n = strtoul(rawdata, &end, 16);
	num:
		if (!end || end != rawdata + len)
			return -1;
		op->number = n;
		break;
	default:
		return -1;
	}

	DBG(DEBUG_OPTIONS, fprintf(stderr,
		"libmount: option %s: set argument value\n", op->name));

	op->mask |= MNT_HASVAL;
	return 0;
}

/**
 * mnt_option_has_value:
 * @option: pointer to moption instance
 *
 * Returns 1 if the option has actually set an argument value, or 0.
 */
int mnt_option_has_value(moption *op)
{
	return op && (op->mask & MNT_HASVAL) ? 1 : 0;
}

/**
 * mnt_option_require_value:
 * @op: pointer to moption instance
 *
 * Returns 1 if the option requires an argument (option=<arg>).
 */
int mnt_option_require_value(moption *op)
{
	return op && op->mapent ? mnt_mapent_require_value(op->mapent) : 0;
}

/**
 * mnt_option_is_inverted:
 * @op: pointer to moption instance
 *
 * Returns 1 if the option has MNT_INVERT mask or 0.
 */
int mnt_option_is_inverted(moption *op)
{
	return (op && (op->mask & MNT_INVERT));
}

/**
 * mnt_option_get_number:
 * @op: pointer to moption instance
 * @number: resulting number
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_option_get_number(moption *op, int *number)
{
	assert(op);
	assert(number);

	if (number && mnt_option_has_value(op)) {
		*number = op->number;
		return 0;
	}
	return -1;
}

/**
 * mnt_option_get_string:
 * @op: pointer to moption instance
 * @str: resulting string
 *
 * Returns 0 on succes, -1 in case of error.
 */
int mnt_option_get_string(moption *op, const char **str)
{
	assert(op);
	assert(str);

	if (str && mnt_option_has_value(op)) {
		*str = op->string;
		return 0;
	}
	return -1;
}

/**
 * mnt_option_get_valsiz:
 * @op: pointer to moption instance
 *
 * Returns length of string that is necessary to print option value or -1 in
 * case of error.
 */
int mnt_option_get_valsiz(moption *op)
{
	assert(op);

	if (!op)
		return -1;
	if (!mnt_option_has_value(op))
		return 0;
	if (op->string)
		return strlen(op->string);
	else {
		/* so primitive, so useful... */
		char buf[256];
		return mnt_option_snprintf_value(op, buf, sizeof(buf));
	}
}

/**
 * mnt_snprintf_option:
 * @op: pointer to moption instance
 * @str: resulting string
 * @size: size of string
 *
 * Returns number of printed characters or negative number in case of error.
 */
int mnt_option_snprintf_value(moption *op, char *str, size_t size)
{
	int type;
	int rc = 0;

	assert(op);
	assert(str);

	if (!op || !str || !size)
		return -1;
	if (!mnt_option_has_value(op))
		return -1;
	type = mnt_option_get_type(op);
	if (type == -1)
		return -1;

	if (type == 's') {
		const char *data;

		if (mnt_option_get_string(op, &data) != 0)
			return -1;
		/* TODO: SELinux contexts require quoted options
		 *       (e.g. context="foo") */
		rc = snprintf(str, size, "%s", data);
	} else if (type == '{') {
		int n;
		size_t len;
		const char *data;

		if (mnt_option_get_number(op, &n) != 0)
			return -1;
		if (mnt_mapent_number_to_enum(op->mapent, n, &data, &len) != 0)
			return -1;
		if (len > size)
			return len;
		memcpy(str, data, len);
		*(str + len) = '\0';
	} else {
		int n;
		char fmt[16];

		if (mnt_option_get_number(op, &n) != 0)
			return -1;

		snprintf(fmt, sizeof(fmt), "%%%c", type);
		rc = snprintf(str, size, fmt, n);
	}
	return rc;
}

/**
 * mnt_option_dup_string:
 * @op: pointer to moption instance
 *
 * Returns duplicate a option value.
 */
char *mnt_option_dup_string(moption *op, char **str)
{
	assert(op);

	if (str && mnt_option_has_value(op) && op->string)
		return strdup(op->string);
	return NULL;
}

/**
 * mnt_option_get_name:
 * @op: pointer to moption instance
 *
 * Returns option name or NULL in case of error.
 */
const char *mnt_option_get_name(moption *op)
{
	assert(op);
	return op ? op->name : NULL;
}

/**
 * mnt_option_get_mask:
 * @op: pointer to moption instance
 *
 * The initial value of the option mask is a copy from map->mask.
 * Note that the mask is NOT a mountflag/ID.
 *
 * Returns option mask or 0.
 */
int mnt_option_get_mask(moption *op)
{
	assert(op);
	return op ? op->mask : 0;
}

/**
 * mnt_option_get_id:
 * @op: pointer to moption instance
 *
 * Note that the ID is also mountflag for all options with MNT_MFLAG mask.
 *
 * WARNING: the ID is usually shared between "option" (e.g. exec) and
 * "nooption" (e.g. noexec) -- you have to carefully check for MNT_INVERT in
 * the option mask. Use mnt_option_get_flag().
 *
 * Returns option ID/mountflag or 0 for extra options (options with undefined
 * options map).
 */
int mnt_option_get_id(moption *op)
{
	assert(op);
	return op && op->mapent ? op->mapent->id : 0;
}

/**
 * mnt_option_get_flag:
 * @op: pointer to moption instance
 * @flags: resulting flags
 *
 * Adds option ID to @flags or removes option ID from @flags when the option
 * is inverted option (e.g. "norelatime")
 *
 * Example:
 *	int flags = 0;
 *
 *	while(mnt_iterate_options(&itr, opts, map, &op) == 0)
 *		mnt_option_get_flag(op, &flags);
 *
 *	if (flags & MS_RELATIME)
 *		printf("relatime not set\n");
 *
 * Returns 0 on success, -1 in case of error.
 */
int mnt_option_get_flag(moption *op, int *flags)
{
	int id;

	assert(op);
	if (!op || !flags)
		return -1;

	id = mnt_option_get_id(op);
	if (op->mask & MNT_INVERT)
		*flags &= ~id;
	else
		*flags |= id;
	return 0;
}

/**
 * mnt_option_is_extra:
 * @op: pointer to moption instance
 *
 * The "extra option" is unknown option (undefined in any option map)
 *
 * Return 1 or 0.
 */
int mnt_option_is_extra(moption *op)
{
	assert(op);
	return op && op->mapent ? 0 : 1;
}

/**
 * mnt_option_get_map:
 * @op: pointer to moption instance
 *
 * Returns pointer to the head of the map that is associated with the option or
 * NULL (for "extra options").
 */
const struct mopt_map *mnt_option_get_map(moption *op)
{
	assert(op);
	return op ? op->map : NULL;
}

/**
 * mnt_option_get_map_entry:
 * @op: pointer to moption instance
 *
 * Returns pointer to the map entry that describes the option or NULL (for
 * "extra options").
 */
const struct mopt_map *mnt_option_get_mapent(moption *op)
{
	assert(op);
	return op ? op->mapent : NULL;
}

#ifdef CONFIG_LIBMOUNT_DEBUG
/*
 * Prints details about the option.
 */
int mnt_option_print_debug(moption *op, FILE *file, const char *premesg)
{
	const struct mopt_map *map;
	int x, num;
	const char *str;

	if (!op)
		return -1;

	fprintf(file, "%s %s:\n", premesg ? premesg : "option",
			mnt_option_get_name(op));

	fprintf(file, "\tID=0x%x\n", mnt_option_get_id(op));
	fprintf(file, "\tMASK=%d\n", mnt_option_get_mask(op));

	map = mnt_option_get_map(op);
	fprintf(file, "\tMAP=%p\n", map ? map : NULL);

	map = mnt_option_get_mapent(op);
	fprintf(file, "\tMAPENT=%s\n", map ? map->name : NULL);

	x = mnt_option_has_value(op);
	fprintf(file, "\tHAS_VALUE=%s\n", x ? "yes" : "not");

	x = mnt_option_get_type(op);
	if (x != -1)
		fprintf(file, "\tTYPE=%c\n", x);
	else
		fprintf(file, "\tTYPE=<none>\n");

	x = mnt_option_get_string(op, &str);
	fprintf(file, "\tSTRING=%s\n", x == 0 ? str : NULL);

	x = mnt_option_get_number(op, &num);
	if (x == 0)
		fprintf(file, "\tNUMBER=%d\n", num);
	else
		fprintf(file, "\tNUMBER=<none>\n");

	fprintf(file, "\n");
	return 0;
}
#endif

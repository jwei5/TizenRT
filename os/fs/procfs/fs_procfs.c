/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * fs/procfs/fs_procfs.c
 *
 *   Copyright (C) 2013-2014 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <sys/types.h>
#include <sys/statfs.h>
#include <sys/stat.h>

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/arch.h>
#include <tinyara/sched.h>
#include <tinyara/kmalloc.h>
#include <tinyara/fs/fs.h>
#include <tinyara/fs/procfs.h>
#include <tinyara/fs/dirent.h>
#include <tinyara/regex.h>

#include <arch/irq.h>

#if !defined(CONFIG_DISABLE_MOUNTPOINT) && defined(CONFIG_FS_PROCFS)

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PROCFS_NATTRS  2

/****************************************************************************
 * External Definitons
 ****************************************************************************/

extern const struct procfs_operations proc_operations;
extern const struct procfs_operations cpuload_operations;
extern const struct procfs_operations uptime_operations;
extern const struct procfs_operations version_operations;
#if defined(CONFIG_LOG_DUMP)
extern const struct procfs_operations logsave_operations;
#endif

/* This is not good.  These are implemented in drivers/mtd.  Having to
 * deal with them here is not a good coupling.
 */

extern const struct procfs_operations mtd_procfsoperations;
extern const struct procfs_operations part_procfsoperations;
extern const struct procfs_operations smartfs_procfsoperations;
extern const struct procfs_operations power_procfsoperations;
extern const struct procfs_operations cm_operations;
extern const struct procfs_operations irqs_operations;
extern const struct procfs_operations ereport_operations;

/* And even worse, this one is specific to the STM32.  The solution to
 * this nasty couple would be to replace this hard-coded, ROM-able
 * operations table with a RAM-base registration table.
 */

/****************************************************************************
 * Private Types
 ****************************************************************************/
/* Table of all known / pre-registered procfs handlers / participants. */

static const struct procfs_entry_s g_procfsentries[] = {
#ifndef CONFIG_FS_PROCFS_EXCLUDE_PROCESS
	{"[0-9]*/**", &proc_operations},
	{"[0-9]*", &proc_operations},
#endif

#if defined(CONFIG_SCHED_CPULOAD) && !defined(CONFIG_FS_PROCFS_EXCLUDE_CPULOAD)
	{"cpuload", &cpuload_operations},
#endif

#if defined(CONFIG_LOG_DUMP)
	{"logsave", &logsave_operations},
#endif

#if defined(CONFIG_FS_SMARTFS) && !defined(CONFIG_FS_PROCFS_EXCLUDE_SMARTFS)
	{"fs/smartfs**", &smartfs_procfsoperations},
#endif

#if defined(CONFIG_DEBUG_IRQ_INFO) && !defined(CONFIG_FS_PROCFS_EXCLUDE_IRQS)
	{"irqs", &irqs_operations},
#endif

#if defined(CONFIG_MTD) && !defined(CONFIG_FS_PROCFS_EXCLUDE_MTD)
	{"mtd", &mtd_procfsoperations},
#endif

#if defined(CONFIG_MTD_PARTITION) && !defined(CONFIG_FS_PROCFS_EXCLUDE_PARTITIONS)
	{"partitions", &part_procfsoperations},
#endif

#if defined(CONFIG_PM) && !defined(CONFIG_FS_PROCFS_EXCLUDE_POWER)
	{"power**", &power_procfsoperations},
	{"power/*", &power_procfsoperations},
#endif

#if !defined(CONFIG_FS_PROCFS_EXCLUDE_UPTIME)
	{"uptime", &uptime_operations},
#endif

#if !defined(CONFIG_FS_PROCFS_EXCLUDE_VERSION)
	{"version", &version_operations},
#endif

#if defined(CONFIG_CM) && !defined(CONFIG_FS_PROCFS_EXCLUDE_CONNECTIVITY)
	{"connectivity**", &cm_operations},
#endif

#if !defined(CONFIG_FS_PROCFS_EXCLUDE_EREPORT)
	{"ereport**", &ereport_operations},
	{"ereport/*", &ereport_operations},
#endif

	{NULL, NULL}
};

static const uint8_t g_procfsentrycount = sizeof(g_procfsentries) / sizeof(struct procfs_entry_s) - 1;

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/
/* Helpers */

static void procfs_enum(FAR struct tcb_s *tcb, FAR void *arg);

/* File system methods */

static int procfs_open(FAR struct file *filep, FAR const char *relpath, int oflags, mode_t mode);
static int procfs_close(FAR struct file *filep);
static ssize_t procfs_read(FAR struct file *filep, FAR char *buffer, size_t buflen);
static ssize_t procfs_write(FAR struct file *filep, FAR const char *buffer, size_t buflen);
static int procfs_ioctl(FAR struct file *filep, int cmd, unsigned long arg);

static int procfs_dup(FAR const struct file *oldp, FAR struct file *newp);

static int procfs_opendir(FAR struct inode *mountpt, const char *relpath, FAR struct fs_dirent_s *dir);
static int procfs_closedir(FAR struct inode *mountpt, FAR struct fs_dirent_s *dir);
static int procfs_readdir(FAR struct inode *mountpt, FAR struct fs_dirent_s *dir);
static int procfs_rewinddir(FAR struct inode *mountpt, FAR struct fs_dirent_s *dir);

static int procfs_bind(FAR struct inode *blkdriver, FAR const void *data, FAR void **handle);
static int procfs_unbind(FAR void *handle, FAR struct inode **blkdriver);
static int procfs_statfs(FAR struct inode *mountpt, FAR struct statfs *buf);

static int procfs_stat(FAR struct inode *mountpt, FAR const char *relpath, FAR struct stat *buf);

/****************************************************************************
 * Private Variables
 ****************************************************************************/

/****************************************************************************
 * Public Variables
 ****************************************************************************/

/* See fs_mount.c -- this structure is explicitly externed there.
 * We use the old-fashioned kind of initializers so that this will compile
 * with any compiler.
 */

const struct mountpt_operations procfs_operations = {
	procfs_open,				/* open */
	procfs_close,				/* close */
	procfs_read,				/* read */
	procfs_write,				/* write */
	NULL,						/* seek */
	procfs_ioctl,				/* ioctl */

	NULL,						/* sync */
	procfs_dup,					/* dup */
	NULL,						/* fstat */
	NULL,						/* truncate */

	procfs_opendir,				/* opendir */
	procfs_closedir,			/* closedir */
	procfs_readdir,				/* readdir */
	procfs_rewinddir,			/* rewinddir */

	procfs_bind,				/* bind */
	procfs_unbind,				/* unbind */
	procfs_statfs,				/* statfs */

	NULL,						/* unlink */
	NULL,						/* mkdir */
	NULL,						/* rmdir */
	NULL,						/* rename */
	procfs_stat					/* stat */
};

/* Level 0 contains the directory of active tasks in addition to other
 * statically registered entries with custom handlers.  This strcture
 * contains a snapshot of the active tasks when the directory is first
 * opened.
 */

struct procfs_level0_s {
	struct procfs_dir_priv_s base;	/* Base struct for ProcFS dir */

	/* Our private data */

	uint8_t lastlen;			/* length of last reported static dir */
	pid_t pid[CONFIG_MAX_TASKS];	/* Snapshot of all active task IDs */
	FAR const char *lastread;	/* Pointer to last static dir read */
};

/* Level 1 is an internal virtual directory (such as /proc/fs) which
 * will contain one or more additional static entries based on the
 * configuration.
 */

struct procfs_level1_s {
	struct procfs_dir_priv_s base;	/* Base struct for ProcFS dir */

	/* Our private data */

	uint8_t lastlen;			/* length of last reported static dir */
	uint8_t subdirlen;			/* Length of the subdir search */
	uint16_t firstindex;		/* Index of 1st entry matching this subdir */
	FAR const char *lastread;	/* Pointer to last static dir read */
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

#ifndef CONFIG_FS_PROCFS_EXCLUDE_PROCESS

/****************************************************************************
 * Name: procfs_enum
 ****************************************************************************/

static void procfs_enum(FAR struct tcb_s *tcb, FAR void *arg)
{
	FAR struct procfs_level0_s *dir = (FAR struct procfs_level0_s *)arg;
	int index;

	DEBUGASSERT(dir);

	/* Add the PID to the list */

	index = dir->base.nentries;
	DEBUGASSERT(index < CONFIG_MAX_TASKS);

	dir->pid[index] = tcb->pid;
	dir->base.nentries = index + 1;
}
#endif

/****************************************************************************
 * Name: procfs_open
 ****************************************************************************/

static int procfs_open(FAR struct file *filep, FAR const char *relpath, int oflags, mode_t mode)
{
	int x, ret = -ENOENT;

	fvdbg("Open '%s'\n", relpath);

	/* Perform the stat based on the procfs_entry operations */

	for (x = 0; x < g_procfsentrycount; x++) {
		/* Test if the path matches this entry's specification */

		if (match(g_procfsentries[x].pathpattern, relpath)) {
			/* Match found!  Stat using this procfs entry */

			DEBUGASSERT(g_procfsentries[x].ops && g_procfsentries[x].ops->open);

			ret = g_procfsentries[x].ops->open(filep, relpath, oflags, mode);

			if (ret == OK) {
				DEBUGASSERT(filep->f_priv);

				((struct procfs_file_s *)filep->f_priv)->procfsentry = &g_procfsentries[x];
			}
		}
	}

	return ret;
}

/****************************************************************************
 * Name: procfs_close
 ****************************************************************************/

static int procfs_close(FAR struct file *filep)
{
	FAR struct procfs_file_s *attr;

	/* Recover our private data from the struct file instance */

	attr = (FAR struct procfs_file_s *)filep->f_priv;
	DEBUGASSERT(attr);

	/* Release the file attributes structure */

	kmm_free(attr);
	filep->f_priv = NULL;
	return OK;
}

/****************************************************************************
 * Name: procfs_read
 ****************************************************************************/

static ssize_t procfs_read(FAR struct file *filep, FAR char *buffer, size_t buflen)
{
	FAR struct procfs_file_s *handler;
	ssize_t ret = 0;

	fvdbg("buffer=%p buflen=%d\n", buffer, (int)buflen);

	/* Recover our private data from the struct file instance */

	handler = (FAR struct procfs_file_s *)filep->f_priv;
	DEBUGASSERT(handler);

	/* Call the handler's read routine */

	ret = handler->procfsentry->ops->read(filep, buffer, buflen);

	return ret;
}

/****************************************************************************
 * Name: procfs_write
 ****************************************************************************/

static ssize_t procfs_write(FAR struct file *filep, FAR const char *buffer, size_t buflen)
{
	FAR struct procfs_file_s *handler;
	ssize_t ret = 0;

	fvdbg("buffer=%p buflen=%d\n", buffer, (int)buflen);

	/* Recover our private data from the struct file instance */

	handler = (FAR struct procfs_file_s *)filep->f_priv;
	DEBUGASSERT(handler);

	/* Call the handler's read routine */

	if (handler->procfsentry->ops->write) {
		ret = handler->procfsentry->ops->write(filep, buffer, buflen);
	}

	return ret;
}

/****************************************************************************
 * Name: procfs_ioctl
 ****************************************************************************/

static int procfs_ioctl(FAR struct file *filep, int cmd, unsigned long arg)
{
	fvdbg("cmd: %d arg: %08lx\n", cmd, arg);

	/* No IOCTL commands supported */

	return -ENOTTY;
}

/****************************************************************************
 * Name: procfs_dup
 *
 * Description:
 *   Duplicate open file data in the new file structure.
 *
 ****************************************************************************/

static int procfs_dup(FAR const struct file *oldp, FAR struct file *newp)
{
	FAR struct procfs_file_s *oldattr;

	fvdbg("Dup %p->%p\n", oldp, newp);

	/* Recover our private data from the old struct file instance */

	oldattr = (FAR struct procfs_file_s *)oldp->f_priv;
	DEBUGASSERT(oldattr);

	/* Allow lower-level handler do the dup to get it's extra data */

	return oldattr->procfsentry->ops->dup(oldp, newp);
}

/****************************************************************************
 * Name: procfs_opendir
 *
 * Description:
 *   Open a directory for read access
 *
 ****************************************************************************/

static int procfs_opendir(FAR struct inode *mountpt, FAR const char *relpath, FAR struct fs_dirent_s *dir)
{
	FAR struct procfs_level0_s *level0;
	FAR struct procfs_dir_priv_s *dirpriv;
	FAR void *priv = NULL;
#ifndef CONFIG_FS_PROCFS_EXCLUDE_PROCESS
	irqstate_t flags;
#endif

	fvdbg("relpath: \"%s\"\n", relpath ? relpath : "NULL");
	DEBUGASSERT(mountpt && relpath && dir && !dir->u.procfs);

	/* The relative must be either:
	 *
	 * ""      - The top level directory of task/thread IDs
	 * "<pid>" - The sub-directory of task/thread attributes
	 */

	if (!relpath || relpath[0] == '\0') {
		/* The path refers to the top level directory.  Allocate the level0
		 * dirent structure.
		 */

		level0 = (FAR struct procfs_level0_s *)
				 kmm_zalloc(sizeof(struct procfs_level0_s));

		if (!level0) {
			fdbg("ERROR: Failed to allocate the level0 directory structure\n");
			return -ENOMEM;
		}

		/* Take a snapshot of all currently active tasks.  Any new tasks
		 * added between the opendir() and closedir() call will not be
		 * visible.
		 *
		 * NOTE that interrupts must be disabled throughout the traversal.
		 */

#ifndef CONFIG_FS_PROCFS_EXCLUDE_PROCESS
		flags = enter_critical_section();
		sched_foreach(procfs_enum, level0);
		leave_critical_section(flags);
#else
		level0->base.index = 0;
		level0->base.nentries = 0;
#endif

		/* Initialze lastread entries */

		level0->lastread = "";
		level0->lastlen = 0;
		level0->base.procfsentry = NULL;

		priv = (FAR void *)level0;
	} else {
		int x, ret;
		int len = strlen(relpath);

		/* Search the static array of procfs_entries */

		for (x = 0; x < g_procfsentrycount; x++) {
			/* Test if the path matches this entry's specification */

			if (match(g_procfsentries[x].pathpattern, relpath)) {
				/* Match found!  Call the handler's opendir routine.  If successful,
				 * this opendir routine will create an entry derived from struct
				 * procfs_dir_priv_s as dir->u.procfs.
				 */

				DEBUGASSERT(g_procfsentries[x].ops && g_procfsentries[x].ops->opendir);
				ret = g_procfsentries[x].ops->opendir(relpath, dir);

				if (ret == OK) {
					DEBUGASSERT(dir->u.procfs);

					/* Set the procfs_entry handler */

					dirpriv = (FAR struct procfs_dir_priv_s *)dir->u.procfs;
					dirpriv->procfsentry = &g_procfsentries[x];
				}

				return ret;
			}

			/* Test for a sub-string match (e.g. "ls /proc/fs") */

			else if (strncmp(g_procfsentries[x].pathpattern, relpath, len) == 0) {
				FAR struct procfs_level1_s *level1;

				/* Doing an intermediate directory search */

				/* The path refers to the top level directory.  Allocate the level0
				 * dirent structure.
				 */

				level1 = (FAR struct procfs_level1_s *)
						 kmm_zalloc(sizeof(struct procfs_level1_s));

				if (!level1) {
					fdbg("ERROR: Failed to allocate the level0 directory structure\n");
					return -ENOMEM;
				}

				level1->base.level = 1;
				level1->base.index = x;
				level1->firstindex = x;
				level1->subdirlen = len;
				level1->lastread = "";
				level1->lastlen = 0;
				level1->base.procfsentry = NULL;

				priv = (FAR void *)level1;
				break;
			}
		}
	}

	dir->u.procfs = priv;
	return OK;
}

/****************************************************************************
 * Name: procfs_closedir
 *
 * Description: Close the directory listing
 *
 ****************************************************************************/

static int procfs_closedir(FAR struct inode *mountpt, FAR struct fs_dirent_s *dir)
{
	FAR struct procfs_dir_priv_s *priv;

	DEBUGASSERT(mountpt && dir && dir->u.procfs);
	priv = dir->u.procfs;

	if (priv) {
		kmm_free(priv);
	}

	dir->u.procfs = NULL;
	return OK;
}

/****************************************************************************
 * Name: procfs_readdir
 *
 * Description: Read the next directory entry
 *
 ****************************************************************************/

static int procfs_readdir(struct inode *mountpt, struct fs_dirent_s *dir)
{
	FAR struct procfs_dir_priv_s *priv;
	FAR struct procfs_level0_s *level0;
	FAR const char *name = NULL;
	unsigned int index;
#ifndef CONFIG_FS_PROCFS_EXCLUDE_PROCESS
	FAR struct tcb_s *tcb;
	irqstate_t flags;
	pid_t pid;
#endif
	int ret = -ENOENT;

	DEBUGASSERT(mountpt && dir && dir->u.procfs);
	priv = dir->u.procfs;

	/* Are we reading the 1st directory level with dynamic PID and static
	 * entries?
	 */

	if (priv->level == 0) {
		level0 = (FAR struct procfs_level0_s *)priv;

		/* Have we reached the end of the PID information */

		index = priv->index;
		if (index >= priv->nentries) {
			/* We must report the next static entry ... no more PID entries.
			 * skip any entries with wildcards in the first segment of the
			 * directory name.
			 */

			while (index < priv->nentries + g_procfsentrycount) {
				name = g_procfsentries[index - priv->nentries].pathpattern;
				while (*name != '/' && *name != '\0') {
					if (*name == '*' || *name == '[' || *name == '?') {
						/* Wildcard found.  Skip this entry */

						index++;
						name = NULL;
						break;
					}

					name++;
				}

				/* Test if we skipped this entry */

				if (name != NULL) {
					/* This entry is okay to report. Test if it has a duplicate
					 * first level name as the one we just reported.  This could
					 * happen in the event of procfs_entry_s such as:
					 *
					 *    fs/smartfs
					 *    fs/nfs
					 *    fs/nxffs
					 */

					name = g_procfsentries[index - priv->nentries].pathpattern;
					if (!level0->lastlen || (strncmp(name, level0->lastread, level0->lastlen) != 0)) {
						/* Not a duplicate, return the first segment of this
						 * entry
						 */

						break;
					} else {
						/* Skip this entry ... duplicate 1st level name found */

						index++;
					}
				}
			}

			/* Test if we are at the end of the directory */

			if (index >= priv->nentries + g_procfsentrycount) {
				/* We signal the end of the directory by returning the special
				 * error -ENOENT
				 */

				fvdbg("Entry %d: End of directory\n", index);
				ret = -ENOENT;
			} else {
				/* Report the next static entry */

				level0->lastlen = strcspn(name, "/");
				level0->lastread = name;
				strncpy(dir->fd_dir.d_name, name, level0->lastlen);
				dir->fd_dir.d_name[level0->lastlen] = '\0';

				if (name[level0->lastlen] == '/') {
					dir->fd_dir.d_type = DTYPE_DIRECTORY;
				} else {
					dir->fd_dir.d_type = DTYPE_FILE;
				}

				/* Advance to next entry for the next read */

				priv->index = index;
				ret = OK;
			}
		}
#ifndef CONFIG_FS_PROCFS_EXCLUDE_PROCESS
		else {
			/* Verify that the pid still refers to an active task/thread */

			pid = level0->pid[index];

			flags = enter_critical_section();
			tcb = sched_gettcb(pid);
			leave_critical_section(flags);

			if (!tcb) {
				fdbg("ERROR: PID %d is no longer valid\n", (int)pid);
				return -ENOENT;
			}

			/* Save the filename=pid and file type=directory */

			dir->fd_dir.d_type = DTYPE_DIRECTORY;
			snprintf(dir->fd_dir.d_name, NAME_MAX + 1, "%d", (int)pid);

			/* Set up the next directory entry offset.  NOTE that we could use the
			 * standard f_pos instead of our own private index.
			 */

			level0->base.index = index + 1;
			ret = OK;
		}
#endif							/* CONFIG_FS_PROCFS_EXCLUDE_PROCESS */
	}

	/* Are we reading an intermediate subdirectory? */

	else if (priv->level > 0 && priv->procfsentry == NULL) {
		FAR struct procfs_level1_s *level1;

		level1 = (FAR struct procfs_level1_s *)priv;

		/* Test if this entry matches.  We assume all entries of the same
		 * subdirectory are listed in order in the procfs_entry array.
		 */

		if (strncmp(g_procfsentries[level1->base.index].pathpattern, g_procfsentries[level1->firstindex].pathpattern, level1->subdirlen) == 0) {
			/* This entry matches.  Report the subdir entry */

			name = &g_procfsentries[level1->base.index].pathpattern[level1->subdirlen + 1];
			level1->lastlen = strcspn(name, "/");
			level1->lastread = name;
			strncpy(dir->fd_dir.d_name, name, level1->lastlen);

			/* Some of the search entries contain '**' wildcards.  When we
			 * report the entry name, we must remove this wildcard search
			 * specifier.
			 */

			while (dir->fd_dir.d_name[level1->lastlen - 1] == '*') {
				level1->lastlen--;
			}

			dir->fd_dir.d_name[level1->lastlen] = '\0';

			if (name[level1->lastlen] == '/') {
				dir->fd_dir.d_type = DTYPE_DIRECTORY;
			} else {
				dir->fd_dir.d_type = DTYPE_FILE;
			}

			level1->base.index++;
			ret = OK;
		} else {
			/* No more entries in the subdirectory */

			ret = -ENOENT;
		}
	} else {
		/* We are performing a directory search of one of the subdirectories
		 * and we must let the handler perform the read.
		 */

		DEBUGASSERT(priv->procfsentry && priv->procfsentry->ops->readdir);
		ret = priv->procfsentry->ops->readdir(dir);
	}

	return ret;
}

/****************************************************************************
 * Name: procfs_rewindir
 *
 * Description: Reset directory read to the first entry
 *
 ****************************************************************************/

static int procfs_rewinddir(struct inode *mountpt, struct fs_dirent_s *dir)
{
	FAR struct procfs_dir_priv_s *priv;

	DEBUGASSERT(mountpt && dir && dir->u.procfs);
	priv = dir->u.procfs;

	if (priv->level > 0 && priv->procfsentry == NULL) {
		priv->index = ((struct procfs_level1_s *)priv)->firstindex;
	} else {
		priv->index = 0;
	}

	return OK;
}

/****************************************************************************
 * Name: procfs_bind
 *
 * Description: This implements a portion of the mount operation. This
 *  function allocates and initializes the mountpoint private data and
 *  binds the blockdriver inode to the filesystem private data.  The final
 *  binding of the private data (containing the blockdriver) to the
 *  mountpoint is performed by mount().
 *
 ****************************************************************************/

static int procfs_bind(FAR struct inode *blkdriver, const void *data, void **handle)
{
	return OK;
}

/****************************************************************************
 * Name: procfs_unbind
 *
 * Description: This implements the filesystem portion of the umount
 *   operation.
 *
 ****************************************************************************/

static int procfs_unbind(void *handle, FAR struct inode **blkdriver)
{
	return OK;
}

/****************************************************************************
 * Name: procfs_statfs
 *
 * Description: Return filesystem statistics
 *
 ****************************************************************************/

static int procfs_statfs(struct inode *mountpt, struct statfs *buf)
{
	/* Fill in the statfs info */

	memset(buf, 0, sizeof(struct statfs));
	buf->f_type = PROCFS_MAGIC;
	buf->f_bsize = 0;
	buf->f_blocks = 0;
	buf->f_bfree = 0;
	buf->f_bavail = 0;
	buf->f_namelen = NAME_MAX;
	return OK;
}

/****************************************************************************
 * Name: procfs_stat
 *
 * Description: Return information about a file or directory
 *
 ****************************************************************************/

static int procfs_stat(struct inode *mountpt, const char *relpath, struct stat *buf)
{
	int ret = -ENOSYS;

	/* Three path forms are accepted:
	 *
	 * ""      - The relative path refers to the top level directory
	 * "<pid>" - If <pid> refers to a currently active task/thread, then it
	 *   is a directory
	 * "<pid>/<attr>" - If <attr> is a recognized attribute then, then it
	 *   is a file.
	 */

	if (!relpath || relpath[0] == '\0') {
		/* The path refers to the top level directory */
		/* It's a read-only directory */

		buf->st_mode = S_IFDIR | S_IROTH | S_IRGRP | S_IRUSR;
		ret = OK;
	} else {
		int x;
		int len = strlen(relpath);

		/* Perform the stat based on the procfs_entry operations */

		for (x = 0; x < g_procfsentrycount; x++) {
			/* Test if the path matches this entry's specification */

			if (match(g_procfsentries[x].pathpattern, relpath)) {
				/* Match found!  Stat using this procfs entry */

				DEBUGASSERT(g_procfsentries[x].ops && g_procfsentries[x].ops->stat);

				return g_procfsentries[x].ops->stat(relpath, buf);
			}

			/* Test for an internal subdirectory stat */

			else if (strncmp(g_procfsentries[x].pathpattern, relpath, len) == 0) {
				/* It's an internal subdirectory */

				buf->st_mode = S_IFDIR | S_IROTH | S_IRGRP | S_IRUSR;
				ret = OK;
				break;
			}
		}
	}

	/* File/directory size, access block size */

	buf->st_size = 0;
	buf->st_blksize = 0;
	buf->st_blocks = 0;
	return ret;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

#endif							/* !CONFIG_DISABLE_MOUNTPOINT && CONFIG_FS_PROCFS */

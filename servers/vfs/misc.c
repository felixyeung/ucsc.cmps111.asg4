/* This file contains a collection of miscellaneous procedures.  Some of them
 * perform simple system calls.  Some others do a little part of system calls
 * that are mostly performed by the Memory Manager.
 *
 * The entry points into this file are
 *   do_dup:	  perform the DUP system call
 *   do_fcntl:	  perform the FCNTL system call
 *   do_sync:	  perform the SYNC system call
 *   do_fsync:	  perform the FSYNC system call
 *   pm_reboot:	  sync disks and prepare for shutdown
 *   pm_fork:	  adjust the tables after PM has performed a FORK system call
 *   do_exec:	  handle files with FD_CLOEXEC on after PM has done an EXEC
 *   do_exit:	  a process has exited; note that in the tables
 *   do_set:	  set uid or gid for some process
 *   do_revive:	  revive a process that was waiting for something (e.g. TTY)
 *   do_svrctl:	  file system control
 *   do_getsysinfo:	request copy of FS data structure
 *   pm_dumpcore: create a core dump
 */

#include "fs.h"
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <minix/callnr.h>
#include <minix/safecopies.h>
#include <minix/endpoint.h>
#include <minix/com.h>
#include <minix/sysinfo.h>
#include <minix/u64.h>
#include <sys/ptrace.h>
#include <sys/svrctl.h>
#include "file.h"
#include "fproc.h"
#include "scratchpad.h"
#include "dmap.h"
#include <minix/vfsif.h>
#include "vnode.h"
#include "vmnt.h"
#include "param.h"

#define CORE_NAME	"core"
#define CORE_MODE	0777	/* mode to use on core image files */

#if ENABLE_SYSCALL_STATS
PUBLIC unsigned long calls_stats[NCALLS];
#endif

FORWARD _PROTOTYPE( void free_proc, (struct fproc *freed, int flags)	);
/*
FORWARD _PROTOTYPE( int dumpcore, (int proc_e, struct mem_map *seg_ptr)	);
FORWARD _PROTOTYPE( int write_bytes, (struct inode *rip, off_t off,
				      char *buf, size_t bytes)		);
FORWARD _PROTOTYPE( int write_seg, (struct inode *rip, off_t off, int proc_e,
			int seg, off_t seg_off, phys_bytes seg_bytes)	);
*/

/*===========================================================================*
 *				do_getsysinfo				     *
 *===========================================================================*/
PUBLIC int do_getsysinfo()
{
  vir_bytes src_addr, dst_addr;
  size_t len;

  /* Only su may call do_getsysinfo. This call may leak information (and is not
   * stable enough to be part of the API/ABI). In the future, requests from
   * non-system processes should be denied.
   */

  if (!super_user) return(EPERM);

  switch(m_in.SI_WHAT) {
    case SI_PROC_TAB:
	src_addr = (vir_bytes) fproc;
	len = sizeof(struct fproc) * NR_PROCS;
	break;
    case SI_DMAP_TAB:
	src_addr = (vir_bytes) dmap;
	len = sizeof(struct dmap) * NR_DEVICES;
	break;
#if ENABLE_SYSCALL_STATS
    case SI_CALL_STATS:
	src_addr = (vir_bytes) calls_stats;
	len = sizeof(calls_stats);
	break;
#endif
    default:
	return(EINVAL);
  }

  if (len != m_in.SI_SIZE)
	return(EINVAL);

  dst_addr = (vir_bytes) m_in.SI_WHERE;
  return sys_datacopy(SELF, src_addr, who_e, dst_addr, len);
}

/*===========================================================================*
 *				do_dup					     *
 *===========================================================================*/
PUBLIC int do_dup()
{
/* Perform the dup(fd) or dup2(fd,fd2) system call. These system calls are
 * obsolete.  In fact, it is not even possible to invoke them using the
 * current library because the library routines call fcntl().  They are
 * provided to permit old binary programs to continue to run.
 */

  register int rfd;
  register struct filp *f;
  int r = OK;

  /* Is the file descriptor valid? */
  rfd = m_in.fd & ~DUP_MASK;		/* kill off dup2 bit, if on */
  if ((f = get_filp(rfd, VNODE_READ)) == NULL) return(err_code);

  /* Distinguish between dup and dup2. */
  if (m_in.fd == rfd) {			/* bit not on */
	/* dup(fd) */
	r = get_fd(0, 0, &m_in.fd2, NULL);
  } else {
	/* dup2(old_fd, new_fd) */
	if (m_in.fd2 < 0 || m_in.fd2 >= OPEN_MAX) {
		r = EBADF;
	} else if (rfd == m_in.fd2) {	/* ignore the call: dup2(x, x) */
		r = m_in.fd2;
	} else {
		/* All is fine, close new_fd if necessary */
		m_in.fd = m_in.fd2;	/* prepare to close fd2 */
		unlock_filp(f);		/* or it might deadlock on do_close */
		(void) do_close();	/* cannot fail */
		f = get_filp(rfd, VNODE_READ); /* lock old_fd again */
	}
  }

  if (r == OK) {
	/* Success. Set up new file descriptors. */
	f->filp_count++;
	fp->fp_filp[m_in.fd2] = f;
	FD_SET(m_in.fd2, &fp->fp_filp_inuse);
	r = m_in.fd2;
  }

  unlock_filp(f);
  return(r);
}

/*===========================================================================*
 *				do_fcntl				     *
 *===========================================================================*/
PUBLIC int do_fcntl()
{
/* Perform the fcntl(fd, request, ...) system call. */

  register struct filp *f;
  int new_fd, fl, r = OK;
  tll_access_t locktype;

  scratch(fp).file.fd_nr = m_in.fd;
  scratch(fp).io.io_buffer = m_in.buffer;
  scratch(fp).io.io_nbytes = m_in.nbytes;	/* a.k.a. m_in.request */

  /* Is the file descriptor valid? */
  locktype = (m_in.request == F_FREESP) ? VNODE_WRITE : VNODE_READ;
  if ((f = get_filp(scratch(fp).file.fd_nr, locktype)) == NULL)
	return(err_code);

  switch (m_in.request) {
    case F_DUPFD:
	/* This replaces the old dup() system call. */
	if (m_in.addr < 0 || m_in.addr >= OPEN_MAX) r = EINVAL;
	else if ((r = get_fd(m_in.addr, 0, &new_fd, NULL)) == OK) {
		f->filp_count++;
		fp->fp_filp[new_fd] = f;
		FD_SET(new_fd, &fp->fp_filp_inuse);
		r = new_fd;
	}
	break;

    case F_GETFD:
	/* Get close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	r = 0;
	if (FD_ISSET(scratch(fp).file.fd_nr, &fp->fp_cloexec_set))
		r = FD_CLOEXEC;
	break;

    case F_SETFD:
	/* Set close-on-exec flag (FD_CLOEXEC in POSIX Table 6-2). */
	if(m_in.addr & FD_CLOEXEC)
		FD_SET(scratch(fp).file.fd_nr, &fp->fp_cloexec_set);
	else
		FD_CLR(scratch(fp).file.fd_nr, &fp->fp_cloexec_set);
	break;

    case F_GETFL:
	/* Get file status flags (O_NONBLOCK and O_APPEND). */
	fl = f->filp_flags & (O_NONBLOCK | O_APPEND | O_ACCMODE);
	r = fl;
	break;

    case F_SETFL:
	/* Set file status flags (O_NONBLOCK and O_APPEND). */
	fl = O_NONBLOCK | O_APPEND | O_REOPEN;
	f->filp_flags = (f->filp_flags & ~fl) | (m_in.addr & fl);
	break;

    case F_GETLK:
    case F_SETLK:
    case F_SETLKW:
	/* Set or clear a file lock. */
	r = lock_op(f, m_in.request);
	break;

    case F_FREESP:
     {
	/* Free a section of a file */
	off_t start, end;
	struct flock flock_arg;
	signed long offset;

	/* Check if it's a regular file. */
	if ((f->filp_vno->v_mode & I_TYPE) != I_REGULAR) r = EINVAL;
	else if (!(f->filp_mode & W_BIT)) r = EBADF;
	else
		/* Copy flock data from userspace. */
		r = sys_datacopy(who_e, (vir_bytes) m_in.name1, SELF,
				 (vir_bytes) &flock_arg,
				 (phys_bytes) sizeof(flock_arg));

	if (r != OK) break;

	/* Convert starting offset to signed. */
	offset = (signed long) flock_arg.l_start;

	/* Figure out starting position base. */
	switch(flock_arg.l_whence) {
	  case SEEK_SET: start = 0; break;
	  case SEEK_CUR:
		if (ex64hi(f->filp_pos) != 0)
			panic("do_fcntl: position in file too high");
		start = ex64lo(f->filp_pos);
		break;
	  case SEEK_END: start = f->filp_vno->v_size; break;
	  default: r = EINVAL;
	}
	if (r != OK) break;

	/* Check for overflow or underflow. */
	if (offset > 0 && start + offset < start) r = EINVAL;
	else if (offset < 0 && start + offset > start) r = EINVAL;
	else {
		start += offset;
		if (start < 0) r = EINVAL;
	}
	if (r != OK) break;

	if (flock_arg.l_len != 0) {
		if (start >= f->filp_vno->v_size) r = EINVAL;
		else if ((end = start + flock_arg.l_len) <= start) r = EINVAL;
		else if (end > f->filp_vno->v_size) end = f->filp_vno->v_size;
	} else {
                end = 0;
	}
	if (r != OK) break;

	r = req_ftrunc(f->filp_vno->v_fs_e, f->filp_vno->v_inode_nr,start,end);

	if (r == OK && flock_arg.l_len == 0)
		f->filp_vno->v_size = start;

	break;
     }

    default:
	r = EINVAL;
  }

  unlock_filp(f);
  return(r);
}

/*===========================================================================*
 *				do_sync					     *
 *===========================================================================*/
PUBLIC int do_sync()
{
  struct vmnt *vmp;
  int r = OK;

  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	if (vmp->m_dev != NO_DEV && vmp->m_fs_e != NONE &&
		 vmp->m_root_node != NULL) {
		if ((r = lock_vmnt(vmp, VMNT_EXCL)) != OK)
			break;
		req_sync(vmp->m_fs_e);
		unlock_vmnt(vmp);
	}
  }

  return(r);
}

/*===========================================================================*
 *				do_fsync				     *
 *===========================================================================*/
PUBLIC int do_fsync()
{
/* Perform the fsync() system call. */
  struct filp *rfilp;
  struct vmnt *vmp;
  dev_t dev;
  int r = OK;

  if ((rfilp = get_filp(m_in.m1_i1, VNODE_READ)) == NULL) return(err_code);
  dev = rfilp->filp_vno->v_dev;
  for (vmp = &vmnt[0]; vmp < &vmnt[NR_MNTS]; ++vmp) {
	if (vmp->m_dev != NO_DEV && vmp->m_dev == dev &&
		vmp->m_fs_e != NONE && vmp->m_root_node != NULL) {

		if ((r = lock_vmnt(vmp, VMNT_EXCL)) != OK)
			break;
		req_sync(vmp->m_fs_e);
		unlock_vmnt(vmp);
	}
  }

  unlock_filp(rfilp);

  return(r);
}

/*===========================================================================*
 *				pm_reboot				     *
 *===========================================================================*/
PUBLIC void pm_reboot()
{
  /* Perform the VFS side of the reboot call. */
  int i;
  struct fproc *rfp;

  do_sync();

  /* Do exit processing for all leftover processes and servers,
   * but don't actually exit them (if they were really gone, PM
   * will tell us about it).
   */
  for (i = 0; i < NR_PROCS; i++) {
	rfp = &fproc[i];
	if (rfp->fp_endpoint == NONE) continue;

	/* Don't just free the proc right away, but let it finish what it was
	 * doing first */
	lock_proc(rfp, 0);
	free_proc(rfp, 0);
	unlock_proc(rfp);
  }

  do_sync();
  unmount_all();
}

/*===========================================================================*
 *				pm_fork					     *
 *===========================================================================*/
PUBLIC void pm_fork(pproc, cproc, cpid)
int pproc;	/* Parent process */
int cproc;	/* Child process */
int cpid;	/* Child process id */
{
/* Perform those aspects of the fork() system call that relate to files.
 * In particular, let the child inherit its parent's file descriptors.
 * The parent and child parameters tell who forked off whom. The file
 * system uses the same slot numbers as the kernel.  Only PM makes this call.
 */

  register struct fproc *cp, *pp;
  int i, parentno, childno;
  mutex_t c_fp_lock;

  /* Check up-to-dateness of fproc. */
  okendpt(pproc, &parentno);

  /* PM gives child endpoint, which implies process slot information.
   * Don't call isokendpt, because that will verify if the endpoint
   * number is correct in fproc, which it won't be.
   */
  childno = _ENDPOINT_P(cproc);
  if (childno < 0 || childno >= NR_PROCS)
	panic("VFS: bogus child for forking: %d", m_in.child_endpt);
  if (fproc[childno].fp_pid != PID_FREE)
	panic("VFS: forking on top of in-use child: %d", childno);

  /* Copy the parent's fproc struct to the child. */
  /* However, the mutex variables belong to a slot and must stay the same. */
  c_fp_lock = fproc[childno].fp_lock;
  fproc[childno] = fproc[parentno];
  fproc[childno].fp_lock = c_fp_lock;

  /* Increase the counters in the 'filp' table. */
  cp = &fproc[childno];
  pp = &fproc[parentno];

  for (i = 0; i < OPEN_MAX; i++)
	if (cp->fp_filp[i] != NULL) cp->fp_filp[i]->filp_count++;

  /* Fill in new process and endpoint id. */
  cp->fp_pid = cpid;
  cp->fp_endpoint = cproc;

  /* A forking process never has an outstanding grant, as it isn't blocking on
   * I/O. */
  if(GRANT_VALID(pp->fp_grant)) {
	panic("VFS: fork: pp (endpoint %d) has grant %d\n", pp->fp_endpoint,
	       pp->fp_grant);
  }
  if(GRANT_VALID(cp->fp_grant)) {
	panic("VFS: fork: cp (endpoint %d) has grant %d\n", cp->fp_endpoint,
	       cp->fp_grant);
  }

  /* A child is not a process leader, not being revived, etc. */
  cp->fp_flags = FP_NOFLAGS;

  /* Record the fact that both root and working dir have another user. */
  if (cp->fp_rd) dup_vnode(cp->fp_rd);
  if (cp->fp_wd) dup_vnode(cp->fp_wd);
}

/*===========================================================================*
 *				free_proc				     *
 *===========================================================================*/
PRIVATE void free_proc(struct fproc *exiter, int flags)
{
  int i;
  register struct fproc *rfp;
  register struct filp *rfilp;
  register struct vnode *vp;
  dev_t dev;

  if (exiter->fp_endpoint == NONE)
	panic("free_proc: already free");

  if (fp_is_blocked(exiter))
	unpause(exiter->fp_endpoint);

  /* Loop on file descriptors, closing any that are open. */
  for (i = 0; i < OPEN_MAX; i++) {
	(void) close_fd(exiter, i);
  }

  /* Release root and working directories. */
  if (exiter->fp_rd) { put_vnode(exiter->fp_rd); exiter->fp_rd = NULL; }
  if (exiter->fp_wd) { put_vnode(exiter->fp_wd); exiter->fp_wd = NULL; }

  /* The rest of these actions is only done when processes actually exit. */
  if (!(flags & FP_EXITING)) return;

  exiter->fp_flags |= FP_EXITING;

  /* Check if any process is SUSPENDed on this driver.
   * If a driver exits, unmap its entries in the dmap table.
   * (unmapping has to be done after the first step, because the
   * dmap table is used in the first step.)
   */
  unsuspend_by_endpt(exiter->fp_endpoint);
  dmap_unmap_by_endpt(exiter->fp_endpoint);

  worker_stop_by_endpt(exiter->fp_endpoint); /* Unblock waiting threads */
  vmnt_unmap_by_endpt(exiter->fp_endpoint); /* Invalidate open files if this
					     * was an active FS */

  /* Invalidate endpoint number for error and sanity checks. */
  exiter->fp_endpoint = NONE;

  /* If a session leader exits and it has a controlling tty, then revoke
   * access to its controlling tty from all other processes using it.
   */
  if ((exiter->fp_flags & FP_SESLDR) && exiter->fp_tty != 0) {
      dev = exiter->fp_tty;
      for (rfp = &fproc[0]; rfp < &fproc[NR_PROCS]; rfp++) {
	  if(rfp->fp_pid == PID_FREE) continue;
          if (rfp->fp_tty == dev) rfp->fp_tty = 0;

          for (i = 0; i < OPEN_MAX; i++) {
		if ((rfilp = rfp->fp_filp[i]) == NULL) continue;
		if (rfilp->filp_mode == FILP_CLOSED) continue;
		vp = rfilp->filp_vno;
		if ((vp->v_mode & I_TYPE) != I_CHAR_SPECIAL) continue;
		if ((dev_t) vp->v_sdev != dev) continue;
		lock_filp(rfilp, VNODE_READ);
		(void) dev_close(dev, rfilp-filp); /* Ignore any errors, even
						    * SUSPEND. */

		rfilp->filp_mode = FILP_CLOSED;
		unlock_filp(rfilp);
          }
      }
  }

  /* Exit done. Mark slot as free. */
  exiter->fp_pid = PID_FREE;
  if (exiter->fp_flags & FP_PENDING)
	pending--;	/* No longer pending job, not going to do it */
  exiter->fp_flags = FP_NOFLAGS;
}

/*===========================================================================*
 *				pm_exit					     *
 *===========================================================================*/
PUBLIC void pm_exit(proc)
int proc;
{
/* Perform the file system portion of the exit(status) system call. */
  int exitee_p;

  /* Nevertheless, pretend that the call came from the user. */
  okendpt(proc, &exitee_p);
  fp = &fproc[exitee_p];
  free_proc(fp, FP_EXITING);
}

/*===========================================================================*
 *				pm_setgid				     *
 *===========================================================================*/
PUBLIC void pm_setgid(proc_e, egid, rgid)
int proc_e;
int egid;
int rgid;
{
  register struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effgid =  egid;
  tfp->fp_realgid = rgid;
}


/*===========================================================================*
 *				pm_setgroups				     *
 *===========================================================================*/
PUBLIC void pm_setgroups(proc_e, ngroups, groups)
int proc_e;
int ngroups;
gid_t *groups;
{
  struct fproc *rfp;
  int slot;

  okendpt(proc_e, &slot);
  rfp = &fproc[slot];
  if (ngroups * sizeof(gid_t) > sizeof(rfp->fp_sgroups))
	panic("VFS: pm_setgroups: too much data to copy");
  if (sys_datacopy(who_e, (vir_bytes) groups, SELF, (vir_bytes) rfp->fp_sgroups,
		   ngroups * sizeof(gid_t)) == OK) {
	rfp->fp_ngroups = ngroups;
  } else
	panic("VFS: pm_setgroups: datacopy failed");
}


/*===========================================================================*
 *				pm_setuid				     *
 *===========================================================================*/
PUBLIC void pm_setuid(proc_e, euid, ruid)
int proc_e;
int euid;
int ruid;
{
  struct fproc *tfp;
  int slot;

  okendpt(proc_e, &slot);
  tfp = &fproc[slot];

  tfp->fp_effuid =  euid;
  tfp->fp_realuid = ruid;
}

/*===========================================================================*
 *				do_svrctl				     *
 *===========================================================================*/
PUBLIC int do_svrctl()
{
  switch (m_in.svrctl_req) {
  /* No control request implemented yet. */
    default:
	return(EINVAL);
  }
}

/*===========================================================================*
 *				pm_dumpcore				     *
 *===========================================================================*/
PUBLIC int pm_dumpcore(endpoint_t proc_e, int csig, vir_bytes exe_name)
{
  int slot, r, core_fd;
  struct filp *f;
  char core_path[PATH_MAX];
  char proc_name[PROC_NAME_LEN];

  okendpt(proc_e, &slot);
  fp = &fproc[slot];

  /* open core file */
  snprintf(core_path, PATH_MAX, "%s.%d", CORE_NAME, fp->fp_pid);
  core_fd = common_open(core_path, O_WRONLY | O_CREAT | O_TRUNC, CORE_MODE);
  if (core_fd < 0) return(core_fd);

  /* get process' name */
  r = sys_datacopy(PM_PROC_NR, exe_name, VFS_PROC_NR, (vir_bytes) proc_name,
			PROC_NAME_LEN);
  if (r != OK) return(r);
  proc_name[PROC_NAME_LEN - 1] = '\0';

  if ((f = get_filp(core_fd, VNODE_WRITE)) == NULL) return(EBADF);
  write_elf_core_file(f, csig, proc_name);
  unlock_filp(f);
  (void) close_fd(fp, core_fd);	/* ignore failure, we're exiting anyway */

  free_proc(fp, FP_EXITING);
  return(OK);
}

/*===========================================================================*
 *				 ds_event				     *
 *===========================================================================*/
PUBLIC void ds_event(void)
{
  char key[DS_MAX_KEYLEN];
  char *blkdrv_prefix = "drv.blk.";
  char *chrdrv_prefix = "drv.chr.";
  u32_t value;
  int type, r, is_blk;
  endpoint_t owner_endpoint;

  /* Get the event and the owner from DS. */
  while ((r = ds_check(key, &type, &owner_endpoint)) == OK) {
	/* Only check for block and character driver up events. */
	if (!strncmp(key, blkdrv_prefix, strlen(blkdrv_prefix))) {
		is_blk = TRUE;
	} else if (!strncmp(key, chrdrv_prefix, strlen(chrdrv_prefix))) {
		is_blk = FALSE;
	} else {
		continue;
	}

	if ((r = ds_retrieve_u32(key, &value)) != OK) {
		printf("VFS: ds_event: ds_retrieve_u32 failed\n");
		return;
	}
	if (value != DS_DRIVER_UP) continue;

	/* Perform up. */
	dmap_endpt_up(owner_endpoint, is_blk);
  }

  if (r != ENOENT) printf("VFS: ds_event: ds_check failed: %d\n", r);
}

# Andy Nguyen (anhnguye@ucsc.edu)
# Shang Hua Wu	(swu14@ucsc.edu)
# Alex Rovnyansky (arovnyan@ucsc.edu)

#	$NetBSD: Makefile,v 1.133 2010/07/31 21:47:53 joerg Exp $
#	@(#)Makefile	8.2 (Berkeley) 1/4/94

# Doing a make includes builds /usr/include

NOOBJ=		# defined

# Missing: mp.h

.if defined(__MINIX)
# Avoid installing: kvm.h lwp.h sa.h (latter not installed anyway)
INCS=	a.out.h aio.h ar.h assert.h atomic.h \
	bitstring.h bm.h cdbr.h cdbw.h complex.h cpio.h ctype.h \
	db.h dirent.h disktab.h dlfcn.h err.h errno.h fenv.h fmtmsg.h fnmatch.h \
	fstab.h fts.h ftw.h getopt.h glob.h grp.h ifaddrs.h iconv.h \
	inttypes.h iso646.h langinfo.h libgen.h \
	limits.h link.h link_aout.h link_elf.h locale.h \
	login_cap.h malloc.h math.h md2.h \
	memory.h mntopts.h monetary.h mpool.h mqueue.h \
	ndbm.h netconfig.h netdb.h netgroup.h nlist.h nl_types.h nsswitch.h \
	paths.h pwd.h randomid.h ranlib.h re_comp.h regex.h regexp.h \
	resolv.h res_update.h rmt.h sched.h search.h semaphore.h setjmp.h \
	sgtty.h signal.h stab.h stdbool.h stddef.h stdio.h stdlib.h string.h \
	strings.h stringlist.h struct.h sysexits.h tar.h time.h \
	ttyent.h tzfile.h ucontext.h ulimit.h unistd.h util.h utime.h utmp.h \
	utmpx.h uuid.h varargs.h vis.h wchar.h wctype.h wordexp.h

INCS += lockcond.h setkey.h

INCS +=	ufs/chfs/chfs.h ufs/chfs/chfs_args.h ufs/chfs/chfs_inode.h \
	ufs/chfs/chfs_pool.h ufs/chfs/debug.h ufs/chfs/ebh.h \
	ufs/chfs/ebh_media.h ufs/chfs/ebh_misc.h ufs/chfs/media.h \
	ufs/ext2fs/ext2fs.h ufs/ext2fs/ext2fs_dinode.h \
	ufs/ext2fs/ext2fs_dir.h ufs/ext2fs/ext2fs_extern.h \
	ufs/ffs/ffs_extern.h ufs/ffs/fs.h ufs/lfs/lfs.h \
	ufs/lfs/lfs_extern.h ufs/mfs/mfs_extern.h ufs/mfs/mfsnode.h \
	ufs/ufs/dinode.h ufs/ufs/dir.h ufs/ufs/dirhash.h \
	ufs/ufs/extattr.h ufs/ufs/inode.h ufs/ufs/quota.h \
	ufs/ufs/quota1.h ufs/ufs/quota2.h ufs/ufs/ufs_bswap.h \
	ufs/ufs/ufs_extern.h ufs/ufs/ufs_quota.h ufs/ufs/ufs_wapbl.h \
	ufs/ufs/ufsmount.h \

.else
INCS=	a.out.h aio.h ar.h assert.h atomic.h \
	bitstring.h bm.h cdbr.h cdbw.h complex.h cpio.h ctype.h \
	db.h dirent.h disktab.h dlfcn.h err.h errno.h fenv.h fmtmsg.h fnmatch.h \
	fstab.h fts.h ftw.h getopt.h glob.h grp.h ifaddrs.h iconv.h \
	inttypes.h iso646.h kvm.h langinfo.h libgen.h \
	limits.h link.h link_aout.h link_elf.h locale.h \
	login_cap.h lwp.h malloc.h math.h md2.h \
	memory.h mntopts.h monetary.h mpool.h mqueue.h \
	ndbm.h netconfig.h netdb.h netgroup.h nlist.h nl_types.h nsswitch.h \
	paths.h pwd.h randomid.h ranlib.h re_comp.h regex.h regexp.h \
	resolv.h res_update.h rmt.h sched.h search.h semaphore.h setjmp.h \
	sgtty.h signal.h stab.h stdbool.h stddef.h stdio.h stdlib.h string.h \
	strings.h stringlist.h struct.h sysexits.h tar.h time.h \
	ttyent.h tzfile.h ucontext.h ulimit.h unistd.h util.h utime.h utmp.h \
	utmpx.h uuid.h varargs.h vis.h wchar.h wctype.h wordexp.h
.endif
INCS+=	arpa/ftp.h arpa/inet.h arpa/nameser.h arpa/nameser_compat.h \
	arpa/telnet.h arpa/tftp.h
INCS+=	protocols/dumprestore.h protocols/routed.h protocols/rwhod.h \
	protocols/talkd.h protocols/timed.h

INCS+=	hesiod.h

INCS+=	rpcsvc/yp_prot.h rpcsvc/ypclnt.h

INCS+=  ssp/ssp.h ssp/stdio.h ssp/string.h ssp/strings.h ssp/unistd.h

.if (${MACHINE_ARCH} != "vax")
INCS+=	ieeefp.h
.endif

.include <bsd.own.mk>


INCSDIR=	/usr/include

.if defined(__MINIX)
# RPC is not compiled in the libc. This include also needs
# rpcgen, which can be compiled if needed.
.else
SUBDIR=		rpc
.endif
.if defined(__MINIX)
SUBDIR+=       prop
.else
SUBDIR+=	../common/include/prop
.endif

.if defined(__MINIX)
.include "Makefile.minix.inc"
SUBDIR+=	arch/${MACHINE}
SUBDIR+= 	arch sys minix
SUBDIR+= 	net netinet netinet6
.endif

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

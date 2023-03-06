/*
 * Copyright (C) 2013-2021 Canonical, Ltd.
 * Copyright (C) 2021-2023 Colin Ian King
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */
#include "stress-ng.h"
#include "core-arch.h"

#if defined(HAVE_ASM_PRCTL_H)
#include <asm/prctl.h>
#endif

#if defined(HAVE_LINUX_SECCOMP_H)
#include <linux/seccomp.h>
#endif

#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif

#if defined(HAVE_SYS_CAPABILITY_H)
#include <sys/capability.h>
#endif

static const stress_help_t help[] = {
	{ NULL,	"procfs N",	"start N workers reading portions of /proc" },
	{ NULL,	"procfs-ops N",	"stop procfs workers after N bogo read operations" },
	{ NULL,	NULL,		NULL }
};

/*
 *  scheduling scopes, added in to Linux on commit
 *  61bc346ce64a3864ac55f5d18bdc1572cda4fb18
 *  "uapi/linux/prctl: provide macro definitions for the PR_SCHED_CORE type argument"
 */
#if !defined(PR_SCHED_CORE_SCOPE_THREAD)
#define PR_SCHED_CORE_SCOPE_THREAD		(0)
#endif
#if !defined(PR_SCHED_CORE_SCOPE_THREAD_GROUP)
#define PR_SCHED_CORE_SCOPE_THREAD_GROUP	(1)
#endif
#if !defined(PR_SCHED_CORE_SCOPE_PROCESS_GROUP)
#define PR_SCHED_CORE_SCOPE_PROCESS_GROUP	(2)
#endif

#if defined(HAVE_SYS_PRCTL_H) &&		\
    defined(HAVE_PRCTL) &&			\
    (defined(PR_SET_PDEATHSIG) ||		/* 1 */ \
     defined(PR_GET_PDEATHSIG) ||		/* 2 */	\
     defined(PR_GET_DUMPABLE) ||		/* 3 */ \
     defined(PR_SET_DUMPABLE) ||		/* 4 */ \
     defined(PR_GET_UNALIGN)) ||		/* 5 */ \
     defined(PR_SET_UNALIGN) ||			/* 6 */ \
     defined(PR_GET_KEEPCAPS) ||		/* 7 */ \
     defined(PR_SET_KEEPCAPS) ||		/* 8 */ \
     defined(PR_GET_FPEMU) ||			/* 9 */ \
     defined(PR_SET_FPEMU) ||			/* 10 */ \
     defined(PR_GET_FPEXC) ||			/* 11 */ \
     defined(PR_SET_FPEXC) ||			/* 12 */ \
     defined(PR_GET_TIMING) ||			/* 13 */ \
     defined(PR_SET_TIMING) ||			/* 14 */ \
     defined(PR_SET_NAME) ||			/* 15 */ \
     defined(PR_GET_NAME) ||			/* 16 */ \
     defined(PR_GET_ENDIAN) ||			/* 19 */ \
     defined(PR_SET_ENDIAN) ||			/* 20 */ \
     defined(PR_GET_SECCOMP) ||			/* 21 */ \
     defined(PR_SET_SECCOMP) ||			/* 22 */ \
     defined(PR_CAPBSET_READ) ||		/* 23 */ \
     defined(PR_CAPBSET_DROP) ||		/* 24 */ \
     defined(PR_GET_TSC) ||			/* 25 */ \
     defined(PR_SET_TSC) ||			/* 26 */ \
     defined(PR_GET_SECUREBITS) ||		/* 27 */ \
     defined(PR_SET_SECUREBITS) ||		/* 28 */ \
     defined(PR_GET_TIMERSLACK) ||		/* 29 */ \
     defined(PR_SET_TIMERSLACK) ||		/* 30 */ \
     defined(PR_TASK_PERF_EVENTS_DISABLE) ||	/* 31 */ \
     defined(PR_TASK_PERF_EVENTS_ENABLE) ||	/* 32 */ \
     defined(PR_MCE_KILL) ||			/* 33 */ \
     defined(PR_MCE_KILL_GET) ||		/* 34 */ \
     defined(PR_SET_MM) ||			/* 35 */ \
     defined(PR_SET_CHILD_SUBREAPER) ||		/* 36 */ \
     defined(PR_GET_CHILD_SUBREAPER) ||		/* 37 */ \
     defined(PR_SET_NO_NEW_PRIVS) ||		/* 38 */ \
     defined(PR_GET_NO_NEW_PRIVS) ||		/* 39 */ \
     defined(PR_GET_TID_ADDRESS) ||		/* 40 */ \
     defined(PR_SET_THP_DISABLE) ||		/* 41 */ \
     defined(PR_GET_THP_DISABLE) ||		/* 42 */ \
     defined(PR_MPX_ENABLE_MANAGEMENT) ||	/* 43 */ \
     defined(PR_MPX_DISABLE_MANAGEMENT) ||	/* 44 */ \
     defined(PR_SET_FP_MODE) ||			/* 45 */ \
     defined(PR_GET_FP_MODE) ||			/* 46 */ \
     defined(PR_CAP_AMBIENT) ||			/* 47 */ \
     defined(PR_SVE_SET_VL) ||			/* 50 */ \
     defined(PR_SVE_GET_VL) ||			/* 51 */ \
     defined(PR_GET_SPECULATION_CTRL) || 	/* 52 */ \
     defined(PR_SET_SPECULATION_CTRL) || 	/* 53 */ \
     defined(PR_PAC_RESET_KEYS) ||		/* 54 */ \
     defined(PR_SET_TAGGED_ADDR_CTRL) ||	/* 55 */ \
     defined(PR_GET_TAGGED_ADDR_CTRL) ||	/* 56 */ \
     defined(PR_SET_IO_FLUSHER) ||		/* 57 */ \
     defined(PR_GET_IO_FLUSHER) ||		/* 58 */ \
     defined(PR_SET_SYSCALL_USER_DISPATCH) ||	/* 59 */ \
     defined(PR_PAC_SET_ENABLED_KEYS) ||	/* 60 */ \
     defined(PR_PAC_GET_ENABLED_KEYS) ||	/* 61 */ \
     defined(PR_SCHED_CORE) ||			/* 62 */ \
     defined(PR_SET_PTRACER)			/* 0x59616d61 */

#if defined(PR_SET_MM) &&		\
    defined(PR_SET_MM_AUXV)

/*
 *  getauxv_addr()
 *	find the address of the auxv vector. This
 *	is located at the end of the environment.
 *	Returns NULL if not found.
 */
static inline void *getauxv_addr(void)
{
	char **env = environ;

	while (env && *env++)
		;

	return (void *)env;
}
#endif

/*
 *  stress_arch_prctl()
 *	exercise arch_prctl(), currently just
 *	x86-64 for now.
 */
static inline void stress_arch_prctl(void)
{
#if defined(HAVE_ASM_PRCTL_H) &&		\
    defined(HAVE_SYS_PRCTL_H) &&		\
    defined(ARCH_GET_CPUID) &&			\
    defined(STRESS_ARCH_X86_64)
	{
		int ret;

		/* GET_CPUID setting, 2nd arg is ignored */
		ret = shim_arch_prctl(ARCH_GET_CPUID, 0);
#if defined(ARCH_SET_CPUID)
		if (ret >= 0)
			VOID_RET(int, shim_arch_prctl(ARCH_SET_CPUID, (unsigned long)ret));
#endif
	}
#endif

#if defined(HAVE_ASM_PRCTL_H) &&		\
    defined(HAVE_SYS_PRCTL_H) &&		\
    defined(ARCH_GET_FS) &&			\
    defined(STRESS_ARCH_X86_64)
	{
		int ret;
		unsigned long fs;

		ret = shim_arch_prctl(ARCH_GET_FS, (unsigned long)&fs);
#if defined(ARCH_SET_FS)
		if (ret == 0)
			ret = shim_arch_prctl(ARCH_SET_FS, fs);
#endif
		(void)ret;
	}
#endif

#if defined(HAVE_ASM_PRCTL_H) &&		\
    defined(HAVE_SYS_PRCTL_H) &&		\
    defined(ARCH_GET_GS) &&			\
    defined(STRESS_ARCH_X86_64)
	{
		int ret;
		unsigned long gs;

		ret = shim_arch_prctl(ARCH_GET_GS, (unsigned long)&gs);
#if defined(ARCH_SET_GS)
		if (ret == 0)
			ret = shim_arch_prctl(ARCH_SET_GS, gs);
#endif
		(void)ret;
	}
#endif

#if defined(HAVE_ASM_PRCTL_H) &&		\
    defined(HAVE_SYS_PRCTL_H) &&		\
    defined(ARCH_GET_XCOMP_SUPP) &&		\
    defined(STRESS_ARCH_X86_64)
	{
		uint64_t features;

		VOID_RET(int, shim_arch_prctl(ARCH_GET_XCOMP_SUPP, (unsigned long)&features));
	}
#endif
#if defined(HAVE_ASM_PRCTL_H) &&		\
    defined(HAVE_SYS_PRCTL_H) &&		\
    defined(ARCH_GET_XCOMP_PERM) &&		\
    defined(STRESS_ARCH_X86_64)
	{
		uint64_t features;

		VOID_RET(int, shim_arch_prctl(ARCH_GET_XCOMP_PERM, (unsigned long)&features));
	}
#endif
#if defined(HAVE_ASM_PRCTL_H) &&		\
    defined(HAVE_SYS_PRCTL_H) &&		\
    defined(ARCH_REQ_XCOMP_PERM) &&		\
    defined(STRESS_ARCH_X86_64)
	{
		unsigned long idx;
		int ret;

		for (idx = 0; idx < 255; idx++) {
			errno = 0;
			ret = shim_arch_prctl(ARCH_REQ_XCOMP_PERM, idx);
			if ((ret < 0) && (errno == -EINVAL))
				break;
		}
	}
#endif
}

#if defined(PR_SET_SYSCALL_USER_DISPATCH) &&	\
    defined(PR_SYS_DISPATCH_ON)	&&		\
    defined(PR_SYS_DISPATCH_OFF) &&		\
    defined(__NR_kill) &&			\
    defined(STRESS_ARCH_X86)

typedef struct {
	int  sig;	/* signal number */
	int  syscall;	/* syscall number */
	int  code;	/* code, should be 2 */
	bool handled;	/* set to true if we handle SIGSYS */
	char selector;	/* prctl mode selector byte */
} stress_prctl_sigsys_info_t;

static stress_prctl_sigsys_info_t prctl_sigsys_info;

#define PRCTL_SYSCALL_OFF()	\
	do { prctl_sigsys_info.selector = SYSCALL_DISPATCH_FILTER_ALLOW; } while (0)
#define PRCTL_SYSCALL_ON()	\
	do { prctl_sigsys_info.selector = SYSCALL_DISPATCH_FILTER_BLOCK; } while (0)

static void stress_prctl_sigsys_handler(int sig, siginfo_t *info, void *ucontext)
{
	(void)ucontext;

	/*
	 *  Turn syscall emulation off otherwise we end up
	 *  producing another SIGSYS when we do a signal
	 *  return
	 */
	PRCTL_SYSCALL_OFF();

	/* Save state */
	prctl_sigsys_info.sig = sig;
	prctl_sigsys_info.syscall = info->si_syscall;
	prctl_sigsys_info.code = info->si_code;
	prctl_sigsys_info.handled = true;
}

/*
 *  stress_prctl_syscall_user_dispatch()
 * 	exercise syscall emulation by trapping a kill() system call
 */
static int stress_prctl_syscall_user_dispatch(const stress_args_t *args)
{
	int ret, rc = EXIT_FAILURE;
	struct sigaction action, oldaction;
	const pid_t pid = getpid();

	(void)memset(&action, 0, sizeof(action));
	(void)sigemptyset(&action.sa_mask);
	action.sa_sigaction = stress_prctl_sigsys_handler;
	action.sa_flags = SA_SIGINFO;

	ret = sigaction(SIGSYS, &action, &oldaction);
	if (ret < 0)
		return 0;

	(void)memset(&prctl_sigsys_info, 0, sizeof(prctl_sigsys_info));

	/* syscall emulation off */
	PRCTL_SYSCALL_OFF();

	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH,
		PR_SYS_DISPATCH_ON, 0, 0, &prctl_sigsys_info.selector);
	if (ret < 0) {
		/* EINVAL will occur on kernels where this is not supported */
		if ((errno == EINVAL) || (errno == ENOSYS))
			return 0;
		pr_fail("%s: prctl PR_SET_SYSCALL_USER_DISPATCH enable failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		goto err;
	}

	/* syscall emulation on */
	PRCTL_SYSCALL_ON();
	(void)kill(pid, 0);

	/* Turn off */
	ret = prctl(PR_SET_SYSCALL_USER_DISPATCH,
		PR_SYS_DISPATCH_OFF, 0, 0, 0);
	if (ret < 0) {
		pr_fail("%s: prctl PR_SET_SYSCALL_USER_DISPATCH disable failed, errno=%d (%s)\n",
			args->name, errno, strerror(errno));
		goto err;
	}

	if (!prctl_sigsys_info.handled) {
		pr_fail("%s: prctl PR_SET_SYSCALL_USER_DISPATCH syscall emulation failed\n",
			args->name);
		goto err;
	}

	if (prctl_sigsys_info.syscall != __NR_kill) {
		pr_fail("%s: prctl PR_SET_SYSCALL_USER_DISPATCH syscall expected syscall 0x%x, got 0x%x instead\n",
			args->name, __NR_kill, prctl_sigsys_info.syscall);
		goto err;
	}

	rc = EXIT_SUCCESS;
err:
	VOID_RET(int, sigaction(SIGSYS, &oldaction, NULL));

	return rc;
}
#else
static int stress_prctl_syscall_user_dispatch(const stress_args_t *args)
{
	(void)args;

	return 0;
}
#endif

static int stress_prctl_child(
	const stress_args_t *args,
	const pid_t mypid,
	void *page_anon,
	size_t page_anon_size)
{
	int ret;

	(void)args;
	(void)mypid;

#if defined(PR_CAP_AMBIENT)
	/* skip for now */
#endif
#if defined(PR_CAPBSET_READ) &&	\
    defined(CAP_CHOWN)
	VOID_RET(int, prctl(PR_CAPBSET_READ, CAP_CHOWN));
#endif

#if defined(PR_CAPBSET_DROP) &&	\
    defined(CAP_CHOWN)
	VOID_RET(int, prctl(PR_CAPBSET_DROP, CAP_CHOWN));
#endif

#if defined(PR_GET_CHILD_SUBREAPER)
	{
		int reaper = 0;

		ret = prctl(PR_GET_CHILD_SUBREAPER, &reaper);
		(void)ret;

#if defined(PR_SET_CHILD_SUBREAPER)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_CHILD_SUBREAPER, !reaper));
			VOID_RET(int, prctl(PR_SET_CHILD_SUBREAPER, reaper));
		}
#endif
	}
#endif

#if defined(PR_GET_DUMPABLE)
	{
		ret = prctl(PR_GET_DUMPABLE);
		(void)ret;

#if defined(PR_SET_DUMPABLE)
		if (ret >= 0) {
			VOID_RET(int, prctl(PR_SET_DUMPABLE, ret));
		}
#endif
	}
#endif

#if defined(PR_GET_ENDIAN)
	/* PowerPC only, but try it on all arches */
	{
		int endian;

		ret = prctl(PR_GET_ENDIAN, &endian);
		(void)ret;

#if defined(PR_SET_ENDIAN)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_ENDIAN, endian));
		}
#endif
	}
#endif

#if defined(PR_GET_FP_MODE)
	/* MIPS only, but try it on all arches */
	{
		int mode;

		mode = prctl(PR_GET_FP_MODE);
		(void)mode;

#if defined(PR_SET_FP_MODE)
		if (mode >= 0) {
			VOID_RET(int, prctl(PR_SET_FP_MODE, mode));
		}
#endif
	}
#endif

#if defined(PR_SVE_GET_VL)
	/* Get vector length */
	{
		int vl;

		vl = prctl(PR_SVE_GET_VL);
		(void)vl;

#if defined(PR_SVE_SET_VL)
		if (vl >= 0) {
			VOID_RET(int, prctl(PR_SVE_SET_VL, vl));
		}
#endif
	}
#endif

#if defined(PR_GET_TAGGED_ADDR_CTRL)
	{
		int ctrl;

		/* exercise invalid args */
		VOID_RET(int, prctl(PR_GET_TAGGED_ADDR_CTRL, ~0, ~0, ~0, ~0));
		ctrl = prctl(PR_GET_TAGGED_ADDR_CTRL, 0, 0, 0, 0);
		(void)ctrl;

#if defined(PR_SET_TAGGED_ADDR_CTRL)
		if (ctrl >= 0) {
			/* exercise invalid args */
			VOID_RET(int, prctl(PR_SET_TAGGED_ADDR_CTRL, ~0, ~0, ~0, ~0));
			VOID_RET(int, prctl(PR_SET_TAGGED_ADDR_CTRL, ctrl, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_FPEMU)
	/* ia64 only, but try it on all arches */
	{
		int control;

		ret = prctl(PR_GET_FPEMU, &control);
		(void)ret;

#if defined(PR_SET_FPEMU)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_FPEMU, control));
		}
#endif
	}
#endif

#if defined(PR_GET_FPEXC)
	/* PowerPC only, but try it on all arches */
	{
		int mode;

		ret = prctl(PR_GET_FPEXC, &mode);
		(void)ret;

#if defined(PR_SET_FPEXC)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_FPEXC, mode));
		}
#endif
	}
#endif

#if defined(PR_GET_KEEPCAPS)
	{
		int flag = 0;

		ret = prctl(PR_GET_KEEPCAPS, &flag);
		(void)ret;

#if defined(PR_SET_KEEPCAPS)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_KEEPCAPS, !flag));
			VOID_RET(int, prctl(PR_SET_KEEPCAPS, flag));
		}
#endif
	}
#endif

#if defined(PR_MCE_KILL_GET)
	/* exercise invalid args */
	VOID_RET(int, prctl(PR_MCE_KILL_GET, ~0, ~0, ~0, ~0));
	/* now exercise what is expected */
	VOID_RET(int, prctl(PR_MCE_KILL_GET, 0, 0, 0, 0));
#endif

#if defined(PR_MCE_KILL)
	/* exercise invalid args */
	VOID_RET(int, prctl(PR_MCE_KILL, PR_MCE_KILL_CLEAR, ~0, ~0, ~0));
	VOID_RET(int, prctl(PR_MCE_KILL, PR_MCE_KILL_SET, ~0, ~0, ~0));
	VOID_RET(int, prctl(PR_MCE_KILL, ~0, ~0, ~0, ~0));
	/* now exercise what is expected */
	VOID_RET(int, prctl(PR_MCE_KILL, PR_MCE_KILL_CLEAR, 0, 0, 0));
#endif

#if defined(PR_SET_MM) &&	\
    defined(PR_SET_MM_BRK)
	VOID_RET(int, prctl(PR_SET_MM, PR_SET_MM_BRK, sbrk(0), 0, 0));
#endif

#if defined(PR_SET_MM) &&		\
    defined(PR_SET_MM_START_CODE) &&	\
    defined(PR_SET_MM_END_CODE)
	{
		char *start, *end;
		const intptr_t mask = ~((intptr_t)args->page_size - 1);
		intptr_t addr;

		(void)stress_text_addr(&start, &end);

		addr = ((intptr_t)start) & mask;
		VOID_RET(int, prctl(PR_SET_MM, PR_SET_MM_START_CODE, addr, 0, 0));

		addr = ((intptr_t)end) & mask;
		VOID_RET(int, prctl(PR_SET_MM, PR_SET_MM_END_CODE, addr, 0, 0));
	}
#endif

#if defined(PR_SET_MM) &&		\
    defined(PR_SET_MM_ENV_START)
	{
		const intptr_t mask = ~((intptr_t)args->page_size - 1);
		const intptr_t addr = ((intptr_t)environ) & mask;

		VOID_RET(int, prctl(PR_SET_MM, PR_SET_MM_ENV_START, addr, 0, 0));
	}
#endif

#if defined(PR_SET_MM) &&		\
    defined(PR_SET_MM_AUXV)
	{
		void *auxv = getauxv_addr();

		if (auxv) {
			VOID_RET(int, prctl(PR_SET_MM, PR_SET_MM_AUXV, auxv, 0, 0));
		}
	}
#endif

#if defined(PR_MPX_ENABLE_MANAGEMENT)
	/* no longer implemented, use invalid args to force -EINVAL */
	VOID_RET(int, prctl(PR_MPX_ENABLE_MANAGEMENT, ~0, ~0, ~0, ~0));
#endif

#if defined(PR_MPX_DISABLE_MANAGEMENT)
	/* no longer implemented, use invalid args to force -EINVAL */
	VOID_RET(int, prctl(PR_MPX_DISABLE_MANAGEMENT, ~0, ~0, ~0, ~0));
#endif

#if defined(PR_GET_NAME)
	{
		char name[17];

		(void)memset(name, 0, sizeof name);

		ret =prctl(PR_GET_NAME, name);
		(void)ret;

#if defined(PR_SET_NAME)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_NAME, name));
		}
#endif
	}
#endif

#if defined(PR_GET_NO_NEW_PRIVS)
	{
		ret = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
		(void)ret;

#if defined(PR_SET_NO_NEW_PRIVS)
		if (ret >= 0) {
			/* exercise invalid args */
			VOID_RET(int, prctl(PR_SET_NO_NEW_PRIVS, ret, ~0, ~0, ~0));
			/* now exercise what is expected */
			VOID_RET(int, prctl(PR_SET_NO_NEW_PRIVS, ret, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_PDEATHSIG)
	{
		int sig;

		ret = prctl(PR_GET_PDEATHSIG, &sig);
		(void)ret;

#if defined(PR_SET_PDEATHSIG)
		if (ret == 0) {
			/* Exercise invalid signal */
			VOID_RET(int, prctl(PR_SET_PDEATHSIG, 0x10000));
			VOID_RET(int, prctl(PR_SET_PDEATHSIG, sig));
		}
#endif
	}
#endif

#if defined(PR_SET_PTRACER)
	{
		ret = prctl(PR_SET_PTRACER, mypid, 0, 0, 0);
		(void)ret;
#if defined(PR_SET_PTRACER_ANY)
		VOID_RET(int, prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0));
#endif
		VOID_RET(int, prctl(PR_SET_PTRACER, 0, 0, 0, 0));
	}
#endif

#if defined(PR_GET_SECCOMP)
	{
		VOID_RET(int, prctl(PR_GET_SECCOMP));

#if defined(PR_SET_SECCOMP)
	/* skip this for the moment */
#endif
	}
#endif

#if defined(PR_GET_SECUREBITS)
	{
		ret = prctl(PR_GET_SECUREBITS, 0, 0, 0, 0);

#if defined(PR_SET_SECUREBITS)
		if (ret >= 0) {
			VOID_RET(int, prctl(PR_SET_SECUREBITS, ret, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_THP_DISABLE)
	{
		ret = prctl(PR_GET_THP_DISABLE, 0, 0, 0, 0);
		(void)ret;

#if defined(PR_SET_THP_DISABLE)
		if (ret >= 0) {
			/* exercise invalid args */
			VOID_RET(int, prctl(PR_SET_THP_DISABLE, 0, 0, ~0, ~0));
			/* now exercise what is expected */
			VOID_RET(int, prctl(PR_SET_THP_DISABLE, 0, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_TASK_PERF_EVENTS_DISABLE)
	VOID_RET(int, prctl(PR_TASK_PERF_EVENTS_DISABLE));
#endif

#if defined(PR_TASK_PERF_EVENTS_ENABLE)
	VOID_RET(int, prctl(PR_TASK_PERF_EVENTS_ENABLE));
#endif

#if defined(PR_GET_TID_ADDRESS)
	{
		uint64_t val;

		VOID_RET(int, prctl(PR_GET_TID_ADDRESS, &val));
	}
#endif

#if defined(PR_GET_TIMERSLACK)
	{
		int slack;

		slack = prctl(PR_GET_TIMERSLACK, 0, 0, 0, 0);
		(void)slack;

#if defined(PR_SET_TIMERSLACK)
		if (slack >= 0) {
			/* Zero timer slack will set to default timer slack */
			VOID_RET(int, prctl(PR_SET_TIMERSLACK, 0, 0, 0, 0));
			/* And restore back to original setting */
			VOID_RET(int, prctl(PR_SET_TIMERSLACK, slack, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_TIMING)
	{
		ret = prctl(PR_GET_TIMING, 0, 0, 0, 0);
		(void)ret;

#if defined(PR_SET_TIMING)
		if (ret >= 0) {
			VOID_RET(int, prctl(PR_SET_TIMING, ret, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_TSC)
	{
		/* x86 only, but try it on all arches */
		int state;

		ret = prctl(PR_GET_TSC, &state, 0, 0, 0);
		(void)ret;

#if defined(PR_SET_TSC)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_TSC, state, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_UNALIGN)
	{
		/* ia64, parisc, powerpc, alpha, sh, tile, but try it on all arches */
		unsigned int control;

		ret = prctl(PR_GET_UNALIGN, &control, 0, 0, 0);
		(void)ret;

#if defined(PR_SET_UNALIGN)
		if (ret == 0) {
			VOID_RET(int, prctl(PR_SET_UNALIGN, control, 0, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_GET_SPECULATION_CTRL)
	{
		unsigned long lval;

		/* exercise invalid args */
		VOID_RET(int, prctl(PR_GET_SPECULATION_CTRL, ~0, ~0, ~0, ~0));
 
#if defined(PR_SPEC_STORE_BYPASS)
		lval = (unsigned long)prctl(PR_GET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, 0, 0, 0);

		if (lval & PR_SPEC_PRCTL) {
			lval &= ~PR_SPEC_PRCTL;
#if defined(PR_SPEC_ENABLE)

			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_ENABLE, 0, 0));
#endif
#if defined(PR_SPEC_DISABLE)
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, PR_SPEC_DISABLE, 0, 0));
#endif
			/* ..and restore */
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_STORE_BYPASS, lval, 0, 0));
		}
#endif

#if defined(PR_SPEC_INDIRECT_BRANCH)
		lval = (unsigned long)prctl(PR_GET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, 0, 0, 0);
		if (lval & PR_SPEC_PRCTL) {
			lval &= ~PR_SPEC_PRCTL;
#if defined(PR_SPEC_ENABLE)
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, PR_SPEC_ENABLE, 0, 0));
#endif
#if defined(PR_SPEC_DISABLE)
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, PR_SPEC_DISABLE, 0, 0));
#endif
			/* ..and restore */
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_INDIRECT_BRANCH, lval, 0, 0));
		}
#endif

#if defined(PR_SPEC_L1D_FLUSH)
		lval = (unsigned long)prctl(PR_GET_SPECULATION_CTRL, PR_SPEC_L1D_FLUSH, 0, 0, 0);
		if (lval & PR_SPEC_PRCTL) {
			lval &= ~PR_SPEC_PRCTL;
#if defined(PR_SPEC_ENABLE)
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_L1D_FLUSH, PR_SPEC_ENABLE, 0, 0));
#endif
#if defined(PR_SPEC_DISABLE)
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_L1D_FLUSH, PR_SPEC_DISABLE, 0, 0));
#endif
			/* ..and restore */
			VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, PR_SPEC_L1D_FLUSH, lval, 0, 0));
		}
#endif
	}
#endif

#if defined(PR_SET_SPECULATION_CTRL)
	{
		/* exercise invalid args */
		VOID_RET(int, prctl(PR_SET_SPECULATION_CTRL, ~0, ~0, ~0, ~0));
	}
#endif

#if defined(PR_GET_IO_FLUSHER)
	{
		VOID_RET(int, prctl(PR_GET_IO_FLUSHER, 0, 0, 0, 0));

#if defined(PR_SET_IO_FLUSHER)
		VOID_RET(int, prctl(PR_SET_IO_FLUSHER, ret, 0, 0, 0));
#endif
	}
#endif

#if defined(PR_SCHED_CORE) &&	\
    defined(PR_SCHED_CORE_GET)
	{
		unsigned long cookie = 0;
		const pid_t bad_pid = stress_get_unused_pid_racy(false);

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, 0,
				PR_SCHED_CORE_SCOPE_THREAD, &cookie));

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, getpid(),
				PR_SCHED_CORE_SCOPE_THREAD, &cookie));

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, bad_pid,
				PR_SCHED_CORE_SCOPE_THREAD, &cookie));

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, 0,
				PR_SCHED_CORE_SCOPE_THREAD_GROUP, &cookie));

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, 0,
				PR_SCHED_CORE_SCOPE_PROCESS_GROUP, &cookie));

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_GET, getgid(),
				PR_SCHED_CORE_SCOPE_PROCESS_GROUP, &cookie));
	}
#endif

#if defined(PR_SCHED_CORE) &&		\
    defined(PR_SCHED_CORE_CREATE) &&	\
    defined(PR_SCHED_CORE_SHARE_TO)
	{
		const pid_t ppid = getppid();

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_CREATE, ppid,
				PR_SCHED_CORE_SCOPE_THREAD, 0));
	}
#endif

#if defined(PR_SCHED_CORE) &&		\
    defined(PR_SCHED_CORE_CREATE) &&	\
    defined(PR_SCHED_CORE_SHARE_FROM)
	{
		const pid_t ppid = getppid();

		VOID_RET(int, prctl(PR_SCHED_CORE, PR_SCHED_CORE_CREATE, ppid,
				PR_SCHED_CORE_SCOPE_THREAD, 0));
	}
#endif

#if defined(PR_PAC_RESET_KEYS)
	{
		/* exercise invalid args */
		VOID_RET(int, prctl(PR_PAC_RESET_KEYS, ~0, ~0, ~0, ~0));
	}
#endif

#if defined(PR_SET_VMA) &&	\
    defined(PR_SET_VMA_ANON_NAME)
	/*
	 *  exercise PR_SET_VMA, introduced in Linux 5.18 with
	 *  CONFIG_ANON_VMA_NAME enabled
	 */
	VOID_RET(int, prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, page_anon, page_anon_size, "stress-prctl"));
	VOID_RET(int, prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, page_anon, page_anon_size, "illegal[$name"));
	VOID_RET(int, prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, page_anon, page_anon_size, NULL));
#else
	(void)page_anon;
	(void)page_anon_size;
#endif
	stress_arch_prctl();

	stress_prctl_syscall_user_dispatch(args);

	/* exercise bad ptrcl command */
	{
		VOID_RET(int, prctl(-1, ~0, ~0, ~0, ~0));
		VOID_RET(int, prctl(0xf00000, ~0, ~0, ~0, ~0));
	}

	(void)ret;

	return 0;
}

/*
 *  stress_prctl()
 *	stress seccomp
 */
static int stress_prctl(const stress_args_t *args)
{
	void *page_anon;

	stress_set_proc_state(args->name, STRESS_STATE_RUN);
	page_anon = mmap(NULL, args->page_size, PROT_READ | PROT_WRITE,
				MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

	do {
		pid_t pid;
again:
		pid = fork();
		if (pid == -1) {
			if (stress_redo_fork(errno))
				goto again;
			if (!keep_stressing(args))
				goto finish;
			pr_fail("%s: fork failed, errno=%d (%s)\n",
				args->name, errno, strerror(errno));
			break;
		}
		if (pid == 0) {
			int rc;
			pid_t mypid = getpid();

			(void)sched_settings_apply(true);

			rc = stress_prctl_child(args, mypid, page_anon, args->page_size);
			_exit(rc);
		}
		if (pid > 0) {
			int status;

			/* Wait for child to exit or get killed by seccomp */
			if (shim_waitpid(pid, &status, 0) < 0) {
				if (errno != EINTR)
					pr_dbg("%s: waitpid failed, errno = %d (%s)\n",
						args->name, errno, strerror(errno));
			} else {
				/* Did the child hit a weird error? */
				if (WIFEXITED(status) &&
				    (WEXITSTATUS(status) != EXIT_SUCCESS)) {
					pr_fail("%s: aborting because of unexpected "
						"failure in child process\n", args->name);
					return EXIT_FAILURE;
				}
			}
		}
		inc_counter(args);
	} while (keep_stressing(args));

finish:
	stress_set_proc_state(args->name, STRESS_STATE_DEINIT);

	if (page_anon != MAP_FAILED)
		(void)munmap(page_anon, args->page_size);

	return EXIT_SUCCESS;
}

stressor_info_t stress_prctl_info = {
	.stressor = stress_prctl,
	.class = CLASS_OS,
	.help = help
};
#else
stressor_info_t stress_prctl_info = {
	.stressor = stress_unimplemented,
	.class = CLASS_OS,
	.help = help,
	.unimplemented_reason = "built without sys/prctl.h or prctl() system call"
};
#endif

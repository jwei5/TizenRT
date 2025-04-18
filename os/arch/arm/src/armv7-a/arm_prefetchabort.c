/****************************************************************************
 *
 * Copyright 2023 Samsung Electronics All Rights Reserved.
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
 * arch/arm/src/armv7-a/arm_prefetchabort.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include <tinyara/irq.h>
#ifdef CONFIG_PAGING
#include <tinyara/page.h>
#endif

#ifdef CONFIG_SYSTEM_REBOOT_REASON
#include <tinyara/reboot_reason.h>
#include <arch/reboot_reason.h>
#endif

#include "sched/sched.h"
#include "up_internal.h"
#ifdef CONFIG_APP_BINARY_SEPARATION
#include "mmu.h"
#endif
/****************************************************************************
 * Public Variables
 ****************************************************************************/

extern uint32_t system_exception_location;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: print_prefetchabort_detail
 ****************************************************************************/

static inline void print_prefetchabort_detail(uint32_t *regs, uint32_t ifar, uint32_t ifsr)
{
	/* Abort log must always start at a new line.*/
	lldbg_noarg("\n");
	_alert("#########################################################################\n");
	_alert("PANIC!!! Prefetch Abort at instruction : 0x%08x\n",  regs[REG_PC]);
	_alert("PC: %08x IFAR: %08x IFSR: %08x\n", regs[REG_PC], ifar, ifsr);
	_alert("#########################################################################\n\n\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: arm_prefetchabort
 *
 * Description:
 *   This is the prefetch abort exception handler. The ARM prefetch abort
 *   exception occurs when a memory fault is detected during an an
 *   instruction fetch.
 *
 ****************************************************************************/

#ifdef CONFIG_PAGING

uint32_t *arm_prefetchabort(uint32_t *regs, uint32_t ifar, uint32_t ifsr)
{
	uint32_t *savestate;

	/* Save the saved processor context in CURRENT_REGS where it can be
	 * accessed for register dumps and possibly context switching.
	 */

	savestate = (uint32_t *)CURRENT_REGS;
	CURRENT_REGS = regs;

	/* Get the (virtual) address of instruction that caused the prefetch
	 * abort. When the exception occurred, this address was provided in the
	 * lr register and this value was saved in the context save area as the
	 * PC at the REG_R15 index.
	 *
	 * Check to see if this miss address is within the configured range of
	 * virtual addresses.
	 */

	pginfo("VADDR: %08x VBASE: %08x VEND: %08x\n", regs[REG_PC], PG_PAGED_VBASE, PG_PAGED_VEND);

	if (regs[REG_R15] >= PG_PAGED_VBASE && regs[REG_R15] < PG_PAGED_VEND) {
		/* Save the offending PC as the fault address in the TCB of the
		 * currently executing task.  This value is, of course, already known
		 * in regs[REG_R15], but saving it in this location will allow common
		 * paging logic for both prefetch and data aborts.
		 */

		struct tcb_s *tcb = this_task();
		tcb->xcp.far = regs[REG_R15];

		/* Call pg_miss() to schedule the page fill.  A consequences of this
		 * call are:
		 *
		 * (1) The currently executing task will be blocked and saved on
		 *     on the g_waitingforfill task list.
		 * (2) An interrupt-level context switch will occur so that when
		 *     this function returns, it will return to a different task,
		 *     most likely the page fill worker thread.
		 * (3) The page fill worker task has been signalled and should
		 *     execute immediately when we return from this exception.
		 */

		pg_miss();

		/* Restore the previous value of CURRENT_REGS.
		 * NULL would indicate thatwe are no longer in an interrupt handler.
		 *  It will be non-NULL if we are returning from a nested interrupt.
		 */

		CURRENT_REGS = savestate;
	} else {
#ifdef CONFIG_SYSTEM_REBOOT_REASON
		up_reboot_reason_write(REBOOT_SYSTEM_PREFETCHABORT);
#endif

		if (!IS_SECURE_STATE()) {
			print_prefetchabort_detail(regs, ifar, ifsr);
		}

		PANIC();
	}

	return regs;
}

#else							/* CONFIG_PAGING */

uint32_t *arm_prefetchabort(uint32_t *regs, uint32_t ifar, uint32_t ifsr)
{
	/* Save the saved processor context in CURRENT_REGS where it can be
	 * accessed for register dumps and possibly context switching.
	 */
	uint32_t *saved_state = (uint32_t *)CURRENT_REGS;
	CURRENT_REGS = regs;

	system_exception_location = regs[REG_R15];

#ifdef CONFIG_SYSTEM_REBOOT_REASON
	up_reboot_reason_write(REBOOT_SYSTEM_PREFETCHABORT);
#endif

	/* Crash -- possibly showing diagnostic debug information. */

	if (!IS_SECURE_STATE()) {
		print_prefetchabort_detail(regs, ifar, ifsr);
	}

	PANIC();
	regs = (uint32_t *)CURRENT_REGS;
	CURRENT_REGS = saved_state;
	return regs; /* To keep the compiler happy */
}

#endif							/* CONFIG_PAGING */

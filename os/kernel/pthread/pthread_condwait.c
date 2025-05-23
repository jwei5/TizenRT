/****************************************************************************
 *
 * Copyright 2016-2017 Samsung Electronics All Rights Reserved.
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
 * kernel/pthread/pthread_condwait.c
 *
 *   Copyright (C) 2007-2009, 2012, 2016 Gregory Nutt. All rights reserved.
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
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <debug.h>

#include <tinyara/cancelpt.h>
#include "pthread/pthread.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: int pthread_cond_wait
 *
 * Description:
 *   A thread can wait for a condition variable to be signalled or broadcast.
 *
 * Parameters:
 *   None
 *
 * Return Value:
 *   None
 *
 * Assumptions:
 *
 ****************************************************************************/

int pthread_cond_wait(FAR pthread_cond_t *cond, FAR pthread_mutex_t *mutex)
{
	int status;
	int ret;

	svdbg("cond=0x%p mutex=0x%p\n", cond, mutex);

	/* pthread_cond_wait() is a cancellation point */
	(void)enter_cancellation_point();

	/* Make sure that non-NULL references were provided. */

	if (cond == NULL || mutex == NULL) {
		ret = EINVAL;
	}

	/* Make sure that the caller holds the mutex */

	else if (mutex->pid != (int)getpid()) {
		ret = EPERM;
	} else {
		uint16_t oldstate;

		/* Give up the mutex */

		svdbg("Give up mutex / take cond\n");

		sched_lock();
		mutex->pid = -1;
		ret = pthread_mutex_give(mutex);

		/* Take the semaphore */

		status = pthread_sem_take((FAR sem_t *)&cond->sem);
		if (ret == OK) {
			/* Report the first failure that occurs */

			ret = status;
		}

		sched_unlock();

		/* Reacquire the mutex
		 * When cancellation points are enabled, we need to
		 * hold the mutex when the pthread is canceled and
		 * cleanup handlers, if any, are entered.
		 */

		svdbg("Reacquire mutex...\n");

		oldstate = pthread_disable_cancel();
		status = pthread_mutex_take(mutex);
		pthread_enable_cancel(oldstate);

		if (ret == OK) {
			/* Report the first failure that occurs */

			ret = status;
		}
		
		/* Was all of the above successful? */

		if (ret == OK) {
			mutex->pid = getpid();
		}
	}

	svdbg("Returning %d\n", ret);
	leave_cancellation_point();
	return ret;
}

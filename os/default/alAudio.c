/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
* National Laboratory.
* Copyright (c) 2002 Deutches Elektronen-Synchrotron in der Helmholtz-
* Gemelnschaft (DESY).
* Copyright (c) 2002 Berliner Speicherring-Gesellschaft fuer Synchrotron-
* Strahlung mbH (BESSY).
* Copyright (c) 2002 Southeastern Universities Research Association, as
* Operator of Thomas Jefferson National Accelerator Facility.
* Copyright (c) 2002 The Regents of the University of California, as
* Operator of Los Alamos National Laboratory.
* This file is distributed subject to a Software License Agreement found
* in the file LICENSE that is included with this distribution. 
\*************************************************************************/
/* alAudio.c 
 *
 * alAudio.c,v 1.4 2009/10/15 14:50:20 jba Exp
 *
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "alh.h"

static pthread_mutex_t beep_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t beep_wake = PTHREAD_COND_INITIALIZER;
static pthread_once_t beep_setup = PTHREAD_ONCE_INIT;
/* states: -2 done, -1 shutdown, 0 idle, 1 playing, 2 request play */
static int beeping = 0;
static pthread_t beeper;

#define LOCK() assert(pthread_mutex_lock(&beep_lock)==0)
#define UNLOCK() assert(pthread_mutex_unlock(&beep_lock)==0)

static void beeper_shutdown(void)
{
    /* request shutdown and spin until beeper thread stops */
    LOCK();
    beeping = -1;
    while(beeping!=-2) {
        UNLOCK();
        pthread_cond_broadcast(&beep_wake);
        usleep(10000); /* 10ms */
        LOCK();
    }
    /* thread is stopped now */
    free(psetup.beepCmd);
    psetup.beepCmd = NULL;

    UNLOCK();
}

static void* beeper_thread(void* junk)
{
    LOCK();

    atexit(&beeper_shutdown);

    while(beeping>=0) {

        assert(pthread_cond_wait(&beep_wake, &beep_lock)==0);

        if(beeping==2) {
            beeping=1;
            UNLOCK();

            system(psetup.beepCmd);

            LOCK();
            /* be careful not to overwrite the shutdown command */
            if(beeping==1)
                beeping=0;
        }
    }
    UNLOCK();
    beeping=-2;
    return NULL;
}

static void setup(void)
{
    if(!psetup.beepCmd)
        return;

    if(pthread_create(&beeper, NULL, &beeper_thread, NULL)) {
        printf("Error creating beeper thread!\n");
        /* clear beepCmd so that future calls to alBeep() will
         * call XBell().
         */
        free(psetup.beepCmd);
        psetup.beepCmd = NULL;
        return;
    }
}

/* Audio device not implemented */

/******************************************************
  alBeep
******************************************************/
int alBeep(Display *displayBB)
{
    pthread_once(&beep_setup, &setup);

    if(!psetup.beepCmd) {
	XBell(displayBB,0);
	return 0;

    } else {
        LOCK();

        /* wakeup for new command.
         * also if still waiting for wakeup
         * if beeper didn't start fast enough
         * on the previous call.
         */
        if(beeping==0 || beeping==2) {
            beeping=2;
            pthread_cond_broadcast(&beep_wake);
        }

        UNLOCK();
        return 0;
    }
}



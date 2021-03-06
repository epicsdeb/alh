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
/* browser.c */

/************************DESCRIPTION***********************************
  Invokes Netscape browser
  Original Author : Kenneth Evans, Jr.
**********************************************************************/

/* Note that there are separate WIN32 and UNIX versions */

#define DEBUG 0

#ifndef WIN32
/*************************************************************************/
/*************************************************************************/
/* Netscape UNIX Version                                                        */
/*************************************************************************/
/*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xmu/WinUtil.h>

#ifndef NETSCAPEPATH
#define NETSCAPEPATH "netscape"
#endif

/* Function prototypes */

extern int kill(pid_t, int);     /* May not be defined for strict ANSI */

int callBrowser(char *url);
static Window checkNetscapeWindow(Window w);
static int execute(char *s);
static Window findNetscapeWindow(void);
static int ignoreXError(Display *display, XErrorEvent *xev);

/* Global variables */
extern Display *display;

/**************************** callBrowser ********************************/
int callBrowser(char *url)
/* Returns non-zero on success, 0 on failure      */
/* url is the URL that the browser is to display  */
/*   or "quit" to terminate the browser           */
{
	int (*oldhandler)(Display *, XErrorEvent *);
	static Window netscapew=(Window)0;
	static pid_t pid=0;
	int status;
	char command[BUFSIZ];
	char *envstring;

	/* Handle quit */
	if(!strcmp(url,"quit")) {
		if (pid) {
			kill(pid,SIGTERM);
			pid=0;
		}
		return 3;
	}
	/* Set handler to ignore possible BadWindow error */
	/*   (Would prefer a routine that tells if the window is defined) */
	oldhandler=XSetErrorHandler(ignoreXError);
	/* Check if the stored window value is valid */
	netscapew=checkNetscapeWindow(netscapew);
	/* Reset error handler */
	XSetErrorHandler(oldhandler);
	/* If stored window is not valid, look for a valid one */
	if(!netscapew) {
		netscapew=findNetscapeWindow();
		/* If no window found, exec Netscape */
		if(!netscapew) {
			envstring=getenv("BROWSER");
			if(!envstring) envstring=getenv("NETSCAPEPATH");
			if(!envstring) {
				sprintf(command,"%s -install '%s' &",NETSCAPEPATH,url);
			}
			else {
				sprintf(command,"%s -install '%s' &",envstring,url);
			}
#if DEBUG
			printf("execute(before): cmd=%s\n",command);
#endif	    
			status=execute(command);
#if DEBUG
			printf("execute(after): cmd=%s status=%d\n",command,status);
#endif	    
			return 1;
		}
	}
	/* Netscape window is valid, send url via -remote */
	/*   (Use -id for speed) */
	envstring=getenv("BROWSER");
	if(!envstring) envstring=getenv("NETSCAPEPATH");
	if(!envstring) {
		sprintf(command,"%s -id 0x%x -remote 'openURL(%s)' &",
		    NETSCAPEPATH,(unsigned int)netscapew,url);
	}
	else {
		sprintf(command,"%s -id 0x%x -remote 'openURL(%s)' &",
		    envstring,(unsigned int)netscapew,url);
	}
#if DEBUG
	printf("execute(before): cmd=%s\n",command);
#endif    
	status=execute(command);
#if DEBUG
	printf("execute(after): cmd=%s status=%d\n",command,status);
#endif    
	return 2;
}
/**************************** checkNetscapeWindow ************************/
static Window checkNetscapeWindow(Window w)
/* Checks if this window is the Netscape window and returns the window
   * if it is or 0 otherwise */
{
	Window wfound=(Window)0;
	static Atom typeatom,versionatom=(Atom)0;
	unsigned long nitems,bytesafter;
	int format,status;
	unsigned char *version=NULL;

	/* If window is NULL, return it */
	if(!w) return w;
	/* Get the atom for the version property (once) */
	if(!versionatom) versionatom=XInternAtom(display,"_MOZILLA_VERSION",False);
	/* Get the version property for this window if it exists */
	status=XGetWindowProperty(display,w,versionatom,0,
	    (65536/sizeof(long)),False,AnyPropertyType,
	    &typeatom,&format,&nitems,&bytesafter,&version);
	/* If the version property exists, it is the Netscape window */
	if(version && status == Success) wfound=w;
#if DEBUG
	printf("XGetWindowProperty: status=%d version=%d w=%x wfound=%x\n",
	    status,version,w,wfound);
#endif      
	/* Free space and return */
	if(version) XFree((void *)version);
	return wfound;
}
/**************************** execute ************************************/
static int execute(char *s)
/* From O'Reilly, Vol. 1, p. 438 */
{
	int status,pid,w;
	register void (*istat)(),(*qstat)();

	if((pid=fork()) == 0) {
		signal(SIGINT,SIG_DFL);
		signal(SIGQUIT,SIG_DFL);
		signal(SIGHUP,SIG_DFL);
		execl("/bin/sh","sh","-c",s,(char *)0);
		_exit(127);
	}
	istat=signal(SIGINT,SIG_IGN);
	qstat=signal(SIGQUIT,SIG_IGN);
	while((w=wait(&status)) != pid && w != -1) ;
	if(w == -1) status=-1;
	signal(SIGINT,istat);
	signal(SIGQUIT,qstat);
	return(status);
}
/**************************** findNetscapeWindow *************************/
static Window findNetscapeWindow(void)
{
	int screen=DefaultScreen(display);
	Window rootwindow=RootWindow(display,screen);
	Window *children,dummy,w,wfound=(Window)0;
	unsigned int nchildren;
	int i;

	/* Get the root window tree */
	if(!XQueryTree(display,rootwindow,&dummy,&dummy,&children,&nchildren))
		return (Window)0;
	/* Look at the children from the top of the stacking order */
	for(i=nchildren-1; i >= 0; i--) {
		w=XmuClientWindow(display,children[i]);
		/* Check if this is the Netscape window */
#if DEBUG
		printf("Child %d ",i);
#endif	
		wfound=checkNetscapeWindow(w);
		if(wfound) break;
	}
	if(children) XFree((void *)children);
	return wfound;
}
/**************************** ignoreXError *******************************/
static int ignoreXError(Display *display, XErrorEvent *xev)
{
#if DEBUG
	printf("In ignoreXError\n");
#endif    
	return 0;
}

#else     /*ifndef WIN32 */
/*************************************************************************/
/*************************************************************************/
/* WIN32 Version                                                        */
/*************************************************************************/
/*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#include <string.h>
#include <errno.h>

/* void medmPrintf(int priority, char *format, ...); */

int callBrowser(char *url);

/**************************** callBrowser (WIN32) ************************/
int callBrowser(char *url)
/* Returns non-zero on success, 0 on failure */
/* Should use the default browser            */
/* Does nothing with "quit"                  */
{
	static int first=1;
	static char *ComSpec;
	char command[BUFSIZ];
	intptr_t status;

	/* Handle quit */
	if(!strcmp(url,"quit")) {
		/* For compatibility, but do nothing */
		return(3);
	}

	/* Get ComSpec for the command shell (should be defined) */
	if (first) {
		first=0;
		ComSpec = getenv("ComSpec");
	}
	if (!ComSpec) return(0);     /* Abort with no message like the UNIX version*/
	/* Spawn the process that handles a url */
#if 0
	/* Works, command window that goes away */
	sprintf(command,"start \"%s\"",url);
	status = _spawnl(_P_WAIT, ComSpec, ComSpec, "/C", command, NULL);

	/* Works, command window that goes away */
	sprintf(command,"start \"%s\"",url);
	status = _spawnl(_P_DETACH, ComSpec, ComSpec, "/C", command, NULL);

	/* Works, command window that goes away */
	sprintf(command,"\"%s\"",url);
	status = _spawnl(_P_NOWAIT, "c:\\windows\\command\\start.exe",
	    "c:\\windows\\command\\start.exe", command, NULL);

	/* Works, command window that goes away */
	sprintf(command,"\"%s\"",url);
	status = _spawnl(_P_WAIT, ComSpec, "start", command, NULL);

	/* Works, command window that goes away */
	sprintf(command,"start \"%s\"",url);
	status = _spawnl(_P_NOWAIT, ComSpec, ComSpec, "/C", command, NULL);

	/* Doesn't work on 95 (No such file or directory), works on NT */
	sprintf(command,"start \"%s\"",url);
	status = _spawnl(_P_NOWAIT, ComSpec, "/C", command, NULL);

	/* Works on 95, not NT, no command window
	   *   No start.exe for NT */
	sprintf(command,"\"%s\"",url);
	status = _spawnlp(_P_DETACH, "start", "start", command, NULL);

	/* Doesn't work on 95 */
	sprintf(command,"\"start %s\"",url);
	status = _spawnl(_P_DETACH, ComSpec, ComSpec, "/C", command, NULL);
#else
	/* This seems to work on 95 and NT, with a command box on 95
	   *   It may have trouble if the URL has spaces */
	sprintf(command,"start %s",url);
	status = _spawnl(_P_DETACH, ComSpec, ComSpec, "/C", command, NULL);
#endif    
	if(status == -1) {
		char *errstring=strerror(errno);

		printf("\ncallBrowser: Cannot start browser:\n"
		    "%s %s\n"
		    "  %s\n",ComSpec,command,errstring);
		/* 	perror("callBrowser:"); */
		return(0);
	}
	return(1);
}
#endif     /* #ifndef WIN32 */

#if 0
/*************************************************************************/
/*************************************************************************/
/* Mosaic Version                                                        */
/*************************************************************************/
/*************************************************************************/

#ifndef MOSAICPATH
/* #define MOSAICPATH "/usr/bin/X11/mosaic" */
/* #define MOSAICPATH "/opt/local/bin/mosaic" */
#define MOSAICPATH "mosaic"
#endif

/**************************** callBrowser ********************************/
int callBrowser(char *url)
/* Returns non-zero on success, 0 on failure */
/* url is the URL that Mosaic is to display  */
/*   or "quit" to terminate Mosaic           */
{
	static pid_t pid=0;
	char filename[32];
	FILE *file;
	char path[BUFSIZ];
	char *envstring;

	signal(SIGCHLD,SIG_IGN);

	/* Handle quit */
	if(!strcmp(url,"quit")) {
		if (pid) {
			sprintf(filename,"/tmp/Mosaic.%d",pid);
			unlink(filename);
			kill(pid,SIGTERM);
			pid=0;
		}
		return 3;
	}
	/* If Mosaic is not up, exec it */
	if ((!pid) || kill(pid,0)) {
		if (!(pid=fork())) {
			envstring=getenv("MOSAICPATH");
			if(!envstring) {
				sprintf(path,"%s",MOSAICPATH);
			}
			else {
				sprintf(path,"%s",envstring);
			}
			execlp(path,path,url,(char *)0);
			perror(path);
			_exit(127);
		}
		return 1;
	}
	/* Mosaic is up, send message through file */
	sprintf(filename,"/tmp/Mosaic.%d",pid);
	if (!(file=fopen(filename,"w"))) return 0;
	fprintf(file,"goto\n%s\n",url);
	fclose(file);
	kill(pid,SIGUSR1);
	return 2;
}
#endif

#include <tcl.h>

/*
 * tclAppInit.c --
 *
 *	Provides a default version of the main program and Tcl_AppInit
 *	function for Tcl applications (without Tk).
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This function performs application-specific initialization. Most
 *	applications, especially those that incorporate additional packages,
 *	will have their own version of this function.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int Tcl_AppInit(Tcl_Interp *interp) {
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }

#ifdef DJGPP
    Tcl_SetVar(interp, "tcl_rcFileName", "~/tclsh.rc", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
#endif

    return TCL_OK;
}

int main(int argc, char **argv) {
	Tcl_Main(argc, argv, Tcl_AppInit);
	return(1);
}

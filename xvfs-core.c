#include <xvfs-core.h>
#include <tcl.h>

/*
 * There are three (3) modes of operation for Xvfs_Register:
 *    1. standalone -- We register our own Tcl_Filesystem
 *                     and handle requests under `//xvfs:/<fsName>`
 *    2. client -- A single Tcl_Filesystem is registered for the
 *                 interp to handle requests under `//xvfs:/` which
 *                 then dispatches to the appropriate registered
 *                 handler
 *    3. flexible -- Attempts to find a core Xvfs instance for the
 *                   process at runtime, if found do #2, otherwise
 *                   fallback to #1
 *
 */
static int Xvfs_Register_Standalone(Tcl_Interp *interp, const char *fsName, int protocolVersion, xvfs_proc_getChildren_t getChildrenProc, xvfs_proc_getData_t getDataProc) {
	Tcl_SetResult(interp, "Not implemented", NULL);
	return(TCL_ERROR);
}

int Xvfs_Register(Tcl_Interp *interp, const char *fsName, int protocolVersion, xvfs_proc_getChildren_t getChildrenProc, xvfs_proc_getData_t getDataProc) {
	return(Xvfs_Register_Standalone(interp, fsName, protocolVersion, getChildrenProc, getDataProc));
}

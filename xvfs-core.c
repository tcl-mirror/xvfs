#include <xvfs-core.h>
#include <tcl.h>

int Xvfs_Register(Tcl_Interp *interp, const char *fsName, int protocolVersion, xvfs_proc_getChildren_t getChildrenProc, xvfs_proc_getData_t getData) {
	Tcl_SetResult(interp, "Not implemented", NULL);
	return(TCL_ERROR);
}
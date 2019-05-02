#include <xvfs-core.h>
#include <tcl.h>

static int xvfs_tclvfs_standalone_pathInFilesystem(Tcl_Obj *path, ClientData *dataPtr) {
	return(TCL_ERROR);
}

static int xvfs_tclvfs_normalizePath(Tcl_Interp *interp, Tcl_Obj *path, int nextCheckpoint) {
	return(TCL_ERROR);
}

static Tcl_Obj *xvfs_tclvfs_listVolumes() {
	return(NULL);
}

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
static int xvfs_standalone_register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	Tcl_Filesystem *xvfsInfo;
	int tcl_ret;
	
	/*
	 * In standalone mode, we only support the same protocol we are
	 * compiling for.
	 */
	if (fsInfo->protocolVersion != XVFS_PROTOCOL_VERSION) {
		if (interp) {
			Tcl_SetResult(interp, "Protocol mismatch", NULL);
		}
		return(TCL_ERROR);
	}
	
	xvfsInfo = (Tcl_Filesystem *) Tcl_AttemptAlloc(sizeof(*xvfsInfo));
	if (!xvfsInfo) {
		if (interp) {
			Tcl_SetResult(interp, "Unable to allocate Tcl_Filesystem object", NULL);
		}
		return(TCL_ERROR);
	}
	
	xvfsInfo->typeName                   = "xvfs";
	xvfsInfo->structureLength            = sizeof(*xvfsInfo);
	xvfsInfo->version                    = TCL_FILESYSTEM_VERSION_1;
	xvfsInfo->pathInFilesystemProc       = xvfs_tclvfs_standalone_pathInFilesystem;
	xvfsInfo->dupInternalRepProc         = NULL;
	xvfsInfo->freeInternalRepProc        = NULL;
	xvfsInfo->internalToNormalizedProc   = NULL;
	xvfsInfo->createInternalRepProc      = NULL;
	xvfsInfo->normalizePathProc          = xvfs_tclvfs_normalizePath;
	xvfsInfo->filesystemPathTypeProc     = NULL;
	xvfsInfo->filesystemSeparatorProc    = NULL;
	xvfsInfo->statProc                   = NULL;
	xvfsInfo->accessProc                 = NULL;
	xvfsInfo->openFileChannelProc        = NULL;
	xvfsInfo->matchInDirectoryProc       = NULL;
	xvfsInfo->utimeProc                  = NULL;
	xvfsInfo->linkProc                   = NULL;
	xvfsInfo->listVolumesProc            = xvfs_tclvfs_listVolumes;
	xvfsInfo->fileAttrStringsProc        = NULL;
	xvfsInfo->fileAttrsGetProc           = NULL;
	xvfsInfo->fileAttrsSetProc           = NULL;
	xvfsInfo->createDirectoryProc        = NULL;
	xvfsInfo->removeDirectoryProc        = NULL;
	xvfsInfo->deleteFileProc             = NULL;
	xvfsInfo->copyFileProc               = NULL;
	xvfsInfo->renameFileProc             = NULL;
	xvfsInfo->copyDirectoryProc          = NULL;
	xvfsInfo->lstatProc                  = NULL;
	xvfsInfo->loadFileProc               = NULL;
	xvfsInfo->getCwdProc                 = NULL;
	xvfsInfo->chdirProc                  = NULL;

	tcl_ret = Tcl_FSRegister(NULL, xvfsInfo);
	if (tcl_ret != TCL_OK) {
		if (interp) {
			Tcl_SetResult(interp, "Tcl_FSRegister() failed", NULL);
		}
		
		return(tcl_ret);
	}
	
	return(TCL_OK);
}

int Xvfs_Register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	return(xvfs_standalone_register(interp, fsInfo));
}

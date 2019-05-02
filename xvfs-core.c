#include <xvfs-core.h>
#include <string.h>
#include <tcl.h>

#define XVFS_ROOT_MOUNTPOINT "//xvfs:/"

struct xvfs_tclfs_instance_info {
	struct Xvfs_FSInfo *fsInfo;
	Tcl_Obj            *mountpoint;
};
static struct xvfs_tclfs_instance_info xvfs_tclfs_standalone_info;

/*
 * Internal Core Utilities
 */
static const char *xvfs_relativePath(Tcl_Obj *path, struct xvfs_tclfs_instance_info *info) {
	const char *pathStr, *rootStr;
	int pathLen, rootLen;
	
	pathStr = Tcl_GetStringFromObj(path, &pathLen);
	rootStr = Tcl_GetStringFromObj(info->mountpoint, &rootLen);
	
	if (pathLen < rootLen) {
		return(NULL);
	}
	
	if (memcmp(pathStr, rootStr, rootLen) != 0) {
		return(NULL);
	}
	
	if (pathLen == rootLen) {
		return("");
	}

	/* XXX:TODO: Should this use the native OS path separator ? */
	if (pathStr[rootLen] != '/') {
		return(NULL);
	}
	
	return(pathStr + rootLen + 1);
}

/*
 * Internal Tcl_Filesystem functions, with the appropriate instance info
 */
static int xvfs_tclfs_pathInFilesystem(Tcl_Obj *path, ClientData *dataPtr, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *relativePath;
	
	relativePath = xvfs_relativePath(path, instanceInfo);
	if (!relativePath) {
		return(-1);
	}
	
	return(TCL_OK);
}

static int xvfs_tclfs_stat(Tcl_Obj *path, Tcl_StatBuf *statBuf, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *pathStr;
	int retval;

	pathStr = xvfs_relativePath(path, instanceInfo);
	
	retval = instanceInfo->fsInfo->getInfoProc(pathStr, statBuf);
	
	return(retval);
}

static Tcl_Obj *xvfs_tclfs_listVolumes(struct xvfs_tclfs_instance_info *instanceInfo) {
	return(NULL);
}

static Tcl_Channel xvfs_tclfs_openFileChannel(Tcl_Interp *interp, Tcl_Obj *path, int mode, int permissions, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *pathStr;

	pathStr = xvfs_relativePath(path, instanceInfo);
fprintf(stderr, "Called open(%s)!\n", pathStr);
	
	return(NULL);
}

/*
 * Tcl_Filesystem handlers for the standalone implementation
 */
static int xvfs_tclfs_standalone_pathInFilesystem(Tcl_Obj *path, ClientData *dataPtr) {
	return(xvfs_tclfs_pathInFilesystem(path, dataPtr, &xvfs_tclfs_standalone_info));
}

static int xvfs_tclfs_standalone_stat(Tcl_Obj *path, Tcl_StatBuf *statBuf) {
	return(xvfs_tclfs_stat(path, statBuf, &xvfs_tclfs_standalone_info));
}

static Tcl_Obj *xvfs_tclfs_standalone_listVolumes(void) {
	return(xvfs_tclfs_listVolumes(&xvfs_tclfs_standalone_info));
}

static Tcl_Channel xvfs_tclfs_standalone_openFileChannel(Tcl_Interp *interp, Tcl_Obj *path, int mode, int permissions) {
	return(xvfs_tclfs_openFileChannel(interp, path, mode, permissions, &xvfs_tclfs_standalone_info));
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
	Tcl_Filesystem *xvfs_tclfs_Info;
	int tcl_ret;
	static int registered = 0;
	
	/*
	 * Ensure this instance is not already registered
	 */
	if (registered) {
		return(TCL_OK);
	}
	registered = 1;

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
	
	xvfs_tclfs_Info = (Tcl_Filesystem *) Tcl_AttemptAlloc(sizeof(*xvfs_tclfs_Info));
	if (!xvfs_tclfs_Info) {
		if (interp) {
			Tcl_SetResult(interp, "Unable to allocate Tcl_Filesystem object", NULL);
		}
		return(TCL_ERROR);
	}
	
	xvfs_tclfs_Info->typeName                   = strdup("xvfs");
	xvfs_tclfs_Info->structureLength            = sizeof(*xvfs_tclfs_Info);
	xvfs_tclfs_Info->version                    = TCL_FILESYSTEM_VERSION_1;
	xvfs_tclfs_Info->pathInFilesystemProc       = xvfs_tclfs_standalone_pathInFilesystem;
	xvfs_tclfs_Info->dupInternalRepProc         = NULL;
	xvfs_tclfs_Info->freeInternalRepProc        = NULL;
	xvfs_tclfs_Info->internalToNormalizedProc   = NULL;
	xvfs_tclfs_Info->createInternalRepProc      = NULL;
	xvfs_tclfs_Info->normalizePathProc          = NULL;
	xvfs_tclfs_Info->filesystemPathTypeProc     = NULL;
	xvfs_tclfs_Info->filesystemSeparatorProc    = NULL;
	xvfs_tclfs_Info->statProc                   = xvfs_tclfs_standalone_stat;
	xvfs_tclfs_Info->accessProc                 = NULL;
	xvfs_tclfs_Info->openFileChannelProc        = xvfs_tclfs_standalone_openFileChannel;
	xvfs_tclfs_Info->matchInDirectoryProc       = NULL;
	xvfs_tclfs_Info->utimeProc                  = NULL;
	xvfs_tclfs_Info->linkProc                   = NULL;
	xvfs_tclfs_Info->listVolumesProc            = xvfs_tclfs_standalone_listVolumes;
	xvfs_tclfs_Info->fileAttrStringsProc        = NULL;
	xvfs_tclfs_Info->fileAttrsGetProc           = NULL;
	xvfs_tclfs_Info->fileAttrsSetProc           = NULL;
	xvfs_tclfs_Info->createDirectoryProc        = NULL;
	xvfs_tclfs_Info->removeDirectoryProc        = NULL;
	xvfs_tclfs_Info->deleteFileProc             = NULL;
	xvfs_tclfs_Info->copyFileProc               = NULL;
	xvfs_tclfs_Info->renameFileProc             = NULL;
	xvfs_tclfs_Info->copyDirectoryProc          = NULL;
	xvfs_tclfs_Info->lstatProc                  = NULL;
	xvfs_tclfs_Info->loadFileProc               = NULL;
	xvfs_tclfs_Info->getCwdProc                 = NULL;
	xvfs_tclfs_Info->chdirProc                  = NULL;

	xvfs_tclfs_standalone_info.fsInfo = fsInfo;
	xvfs_tclfs_standalone_info.mountpoint = Tcl_NewObj();
	Tcl_AppendStringsToObj(xvfs_tclfs_standalone_info.mountpoint, XVFS_ROOT_MOUNTPOINT, fsInfo->name, NULL);
	
	tcl_ret = Tcl_FSRegister(NULL, xvfs_tclfs_Info);
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

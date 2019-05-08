#include <xvfs-core.h>
#include <string.h>
#include <tcl.h>

#if defined(XVFS_MODE_FLEXIBLE) || defined(XVFS_MODE_SERVER)
#define XVFS_INTERNAL_SERVER_MAGIC "\xD4\xF3\x05\x96\x25\xCF\xAF\xFE"
#define XVFS_INTERNAL_SERVER_MAGIC_LEN 8

struct xvfs_tclfs_server_info {
	char magic[XVFS_PROTOCOL_SERVER_MAGIC_LEN];
	int (*registerProc)(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);
};
#endif /* XVFS_MODE_FLEXIBLE || XVFS_MODE_SERVER */

#if defined(XVFS_MODE_SERVER) || defined(XVFS_MODE_STANDALONE) || defined(XVFS_MODE_FLEXIBLE)
#define XVFS_ROOT_MOUNTPOINT "//xvfs:/"

struct xvfs_tclfs_instance_info {
	struct Xvfs_FSInfo *fsInfo;
	Tcl_Obj            *mountpoint;
};

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
	
	retval = instanceInfo->fsInfo->getStatProc(pathStr, statBuf);
	
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
#endif /* XVFS_MODE_SERVER || XVFS_MODE_STANDALONE || XVFS_MODE_FLEIXBLE */

#if defined(XVFS_MODE_STANDALONE) || defined(XVFS_MODE_FLEXIBLE)
/*
 * Tcl_Filesystem handlers for the standalone implementation
 */
static struct xvfs_tclfs_instance_info xvfs_tclfs_standalone_info;
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
static Tcl_Filesystem xvfs_tclfs_standalone_fs;
int xvfs_standalone_register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
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
	
	xvfs_tclfs_standalone_fs.typeName                   = "xvfs";
	xvfs_tclfs_standalone_fs.structureLength            = sizeof(xvfs_tclfs_standalone_fs);
	xvfs_tclfs_standalone_fs.version                    = TCL_FILESYSTEM_VERSION_1;
	xvfs_tclfs_standalone_fs.pathInFilesystemProc       = xvfs_tclfs_standalone_pathInFilesystem;
	xvfs_tclfs_standalone_fs.dupInternalRepProc         = NULL;
	xvfs_tclfs_standalone_fs.freeInternalRepProc        = NULL;
	xvfs_tclfs_standalone_fs.internalToNormalizedProc   = NULL;
	xvfs_tclfs_standalone_fs.createInternalRepProc      = NULL;
	xvfs_tclfs_standalone_fs.normalizePathProc          = NULL;
	xvfs_tclfs_standalone_fs.filesystemPathTypeProc     = NULL;
	xvfs_tclfs_standalone_fs.filesystemSeparatorProc    = NULL;
	xvfs_tclfs_standalone_fs.statProc                   = xvfs_tclfs_standalone_stat;
	xvfs_tclfs_standalone_fs.accessProc                 = NULL;
	xvfs_tclfs_standalone_fs.openFileChannelProc        = xvfs_tclfs_standalone_openFileChannel;
	xvfs_tclfs_standalone_fs.matchInDirectoryProc       = NULL;
	xvfs_tclfs_standalone_fs.utimeProc                  = NULL;
	xvfs_tclfs_standalone_fs.linkProc                   = NULL;
	xvfs_tclfs_standalone_fs.listVolumesProc            = xvfs_tclfs_standalone_listVolumes;
	xvfs_tclfs_standalone_fs.fileAttrStringsProc        = NULL;
	xvfs_tclfs_standalone_fs.fileAttrsGetProc           = NULL;
	xvfs_tclfs_standalone_fs.fileAttrsSetProc           = NULL;
	xvfs_tclfs_standalone_fs.createDirectoryProc        = NULL;
	xvfs_tclfs_standalone_fs.removeDirectoryProc        = NULL;
	xvfs_tclfs_standalone_fs.deleteFileProc             = NULL;
	xvfs_tclfs_standalone_fs.copyFileProc               = NULL;
	xvfs_tclfs_standalone_fs.renameFileProc             = NULL;
	xvfs_tclfs_standalone_fs.copyDirectoryProc          = NULL;
	xvfs_tclfs_standalone_fs.lstatProc                  = NULL;
	xvfs_tclfs_standalone_fs.loadFileProc               = NULL;
	xvfs_tclfs_standalone_fs.getCwdProc                 = NULL;
	xvfs_tclfs_standalone_fs.chdirProc                  = NULL;

	xvfs_tclfs_standalone_info.fsInfo = fsInfo;
	xvfs_tclfs_standalone_info.mountpoint = Tcl_NewObj();
	Tcl_AppendStringsToObj(xvfs_tclfs_standalone_info.mountpoint, XVFS_ROOT_MOUNTPOINT, fsInfo->name, NULL);
	
	tcl_ret = Tcl_FSRegister(NULL, &xvfs_tclfs_standalone_fs);
	if (tcl_ret != TCL_OK) {
		if (interp) {
			Tcl_SetResult(interp, "Tcl_FSRegister() failed", NULL);
		}
		
		return(tcl_ret);
	}
	
	return(TCL_OK);
}
#endif /* XVFS_MODE_STANDALONE || XVFS_MODE_FLEXIBLE */

#if defined(XVFS_MODE_FLEXIBLE)
int xvfs_flexible_register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	ClientData fsHandlerDataRaw;
	struct xvfs_tclfs_server_info *fsHandlerData;
	const Tcl_Filesystem *fsHandler;
	int (*xvfs_register)(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);
	Tcl_Obj *rootPathObj;

	xvfs_register = &xvfs_standalone_register;

	rootPathObj = Tcl_NewStringObj(XVFS_ROOT_MOUNTPOINT, -1);
	if (!rootPathObj) {
		return(xvfs_register(interp, fsInfo));
	}

	Tcl_IncrRefCount(rootPathObj);
	fsHandler = Tcl_FSGetFileSystemForPath(rootPathObj);
	Tcl_DecrRefCount(rootPathObj);

	if (!fsHandler) {
		return(xvfs_register(interp, fsInfo));
	}

	fsHandlerDataRaw = Tcl_FSData(fsHandler);
	if (!fsHandlerDataRaw) {
		return(xvfs_register(interp, fsInfo));
	}

	fsHandlerData = (struct xvfs_tclfs_server_info *) fsHandlerDataRaw;

	/*
	 * XXX:TODO: What is the chance that the handler for //xvfs:/ hold
	 * client data smaller than XVFS_PROTOCOL_SERVER_MAGIC_LEN ?
	 */
	if (memcmp(fsHandlerData->magic, XVFS_PROTOCOL_SERVER_MAGIC, sizeof(fsHandlerData->magic)) == 0) {
		xvfs_register = fsHandlerData->registerProc;
	}

	return(xvfs_register(interp, fsInfo));
}
#endif /* XVFS_MODE_FLEXIBLE */

#if defined(XVFS_MODE_SERVER)
int Xvfs_Register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	return(TCL_ERROR);
}
#endif /* XVFS_MODE_SERVER */

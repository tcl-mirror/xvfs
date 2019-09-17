#include <xvfs-core.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <tcl.h>

#ifdef XVFS_DEBUG
#include <stdio.h> /* Needed for XVFS_DEBUG_PRINTF */
static int xvfs_debug_depth = 0;
#define XVFS_DEBUG_PRINTF(fmt, ...) fprintf(stderr, "[XVFS:DEBUG:%-30s:%4i] %s" fmt "\n", __func__, __LINE__, "                                                                                " + (80 - (xvfs_debug_depth * 4)), __VA_ARGS__)
#define XVFS_DEBUG_PUTS(str) XVFS_DEBUG_PRINTF("%s", str);
#define XVFS_DEBUG_ENTER { xvfs_debug_depth++; XVFS_DEBUG_PUTS("Entered"); }
#define XVFS_DEBUG_LEAVE { XVFS_DEBUG_PUTS("Returning"); xvfs_debug_depth--; }
#else /* XVFS_DEBUG */
#define XVFS_DEBUG_PRINTF(fmt, ...) /**/
#define XVFS_DEBUG_PUTS(str) /**/
#define XVFS_DEBUG_ENTER /**/
#define XVFS_DEBUG_LEAVE /**/
#endif /* XVFS_DEBUG */

#if defined(XVFS_MODE_FLEXIBLE) || defined(XVFS_MODE_SERVER)
#define XVFS_INTERNAL_SERVER_MAGIC "\xD4\xF3\x05\x96\x25\xCF\xAF\xFE"
#define XVFS_INTERNAL_SERVER_MAGIC_LEN 8

struct xvfs_tclfs_server_info {
	char magic[XVFS_INTERNAL_SERVER_MAGIC_LEN];
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
static Tcl_Obj *xvfs_absolutePath(Tcl_Obj *path) {
	Tcl_Obj *currentDirectory;
	const char *pathStr;

	XVFS_DEBUG_ENTER;

	pathStr = Tcl_GetString(path);

	if (pathStr[0] != '/') {
		currentDirectory = Tcl_FSGetCwd(NULL);
		Tcl_IncrRefCount(currentDirectory);

		path = Tcl_ObjPrintf("%s/%s", Tcl_GetString(currentDirectory), pathStr);
		Tcl_IncrRefCount(path);
		Tcl_DecrRefCount(currentDirectory);
	} else {
		Tcl_IncrRefCount(path);
	}

	XVFS_DEBUG_PRINTF("Converted path \"%s\" to absolute path: \"%s\"", pathStr, Tcl_GetString(path));

	XVFS_DEBUG_LEAVE;
	return(path);
}

static const char *xvfs_relativePath(Tcl_Obj *path, struct xvfs_tclfs_instance_info *info) {
	const char *pathStr, *rootStr;
	const char *pathFinal;
	int pathLen, rootLen;

	XVFS_DEBUG_ENTER;

	rootStr = Tcl_GetStringFromObj(info->mountpoint, &rootLen);
	pathStr = Tcl_GetStringFromObj(path, &pathLen);

	XVFS_DEBUG_PRINTF("Finding relative path of \"%s\" from \"%s\" ...", pathStr, rootStr);

	if (pathLen < rootLen) {
		XVFS_DEBUG_PUTS("... none possible (length)");

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	if (memcmp(pathStr, rootStr, rootLen) != 0) {
		XVFS_DEBUG_PUTS("... none possible (prefix differs)");

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	if (pathLen == rootLen) {
		XVFS_DEBUG_PUTS("... short circuit: \"\"");

		XVFS_DEBUG_LEAVE;
		return("");
	}

	/* XXX:TODO: Should this use the native OS path separator ? */
	if (pathStr[rootLen] != '/') {
		XVFS_DEBUG_PUTS("... none possible (no seperator)");

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	pathFinal = pathStr + rootLen + 1;
	pathLen  -= rootLen + 1;

	if (pathLen == 1 && memcmp(pathFinal, ".", 1) == 0) {
		XVFS_DEBUG_PUTS("... short circuit: \".\" -> \"\"");

		XVFS_DEBUG_LEAVE;
		return("");
	}

	while (pathLen >= 2 && memcmp(pathFinal, "./", 2) == 0) {
		pathFinal += 2;
		pathLen   -= 2;
	}

	XVFS_DEBUG_PRINTF("... relative path: \"%s\"", pathFinal);

	XVFS_DEBUG_LEAVE;
	return(pathFinal);
}

static int xvfs_errorToErrno(int xvfs_error) {
	if (xvfs_error >= 0) {
		return(0);
	}

	switch (xvfs_error) {
		case XVFS_RV_ERR_ENOENT:
			return(ENOENT);
		case XVFS_RV_ERR_EINVAL:
			return(EINVAL);
		case XVFS_RV_ERR_EISDIR:
			return(EISDIR);
		case XVFS_RV_ERR_ENOTDIR:
			return(ENOTDIR);
		case XVFS_RV_ERR_EFAULT:
			return(EFAULT);
		case XVFS_RV_ERR_EROFS:
			return(EROFS);
		case XVFS_RV_ERR_INTERNAL:
			return(EINVAL);
		default:
			return(ERANGE);
	}
}

static const char *xvfs_strerror(int xvfs_error) {
	if (xvfs_error >= 0) {
		return("Not an error");
	}

	switch (xvfs_error) {
		case XVFS_RV_ERR_ENOENT:
		case XVFS_RV_ERR_EINVAL:
		case XVFS_RV_ERR_EISDIR:
		case XVFS_RV_ERR_ENOTDIR:
		case XVFS_RV_ERR_EFAULT:
		case XVFS_RV_ERR_EROFS:
			return(Tcl_ErrnoMsg(xvfs_errorToErrno(xvfs_error)));
		case XVFS_RV_ERR_INTERNAL:
			return("Internal error");
		default:
			return("Unknown error");
	}
}

static void xvfs_setresults_error(Tcl_Interp *interp, int xvfs_error) {
	if (!interp) {
		return;
	}

	Tcl_SetErrno(xvfs_errorToErrno(xvfs_error));
	Tcl_SetResult(interp, (char *) xvfs_strerror(xvfs_error), NULL);

	return;
}

/*
 * Xvfs Memory Channel
 */
struct xvfs_tclfs_channel_id {
	Tcl_Channel channel;
	struct xvfs_tclfs_instance_info *fsInstanceInfo;
	Tcl_Obj *path;
	Tcl_WideInt currentOffset;
	Tcl_WideInt fileSize;
	int eofMarked;
	int queuedEvents;
	int closed;
};
struct xvfs_tclfs_channel_event {
	Tcl_Event tcl;
	struct xvfs_tclfs_channel_id *channelInstanceData;
};
static Tcl_ChannelType xvfs_tclfs_channelType;

static Tcl_Channel xvfs_tclfs_openChannel(Tcl_Interp *interp, Tcl_Obj *path, struct xvfs_tclfs_instance_info *instanceInfo) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	Tcl_Channel channel;
	Tcl_StatBuf fileInfo;
	Tcl_Obj *channelName;
	int statRet;

	XVFS_DEBUG_ENTER;
	XVFS_DEBUG_PRINTF("Opening file \"%s\" ...", Tcl_GetString(path));

	statRet = instanceInfo->fsInfo->getStatProc(Tcl_GetString(path), &fileInfo);
	if (statRet < 0) {
		XVFS_DEBUG_PRINTF("... failed: %s", xvfs_strerror(statRet));

		xvfs_setresults_error(interp, XVFS_RV_ERR_ENOENT);

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	if (fileInfo.st_mode & 040000) {
		XVFS_DEBUG_PUTS("... failed (cannot open directories)");

		xvfs_setresults_error(interp, XVFS_RV_ERR_EISDIR);

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	channelInstanceData = (struct xvfs_tclfs_channel_id *) Tcl_Alloc(sizeof(*channelInstanceData));
	channelInstanceData->currentOffset = 0;
	channelInstanceData->eofMarked = 0;
	channelInstanceData->queuedEvents = 0;
	channelInstanceData->closed = 0;
	channelInstanceData->channel = NULL;

	channelName = Tcl_ObjPrintf("xvfs0x%llx", (unsigned long long) channelInstanceData);
	if (!channelName) {
		XVFS_DEBUG_PUTS("... failed");

		Tcl_Free((char *) channelInstanceData);

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}
	Tcl_IncrRefCount(channelName);

	channelInstanceData->fsInstanceInfo = instanceInfo;
	channelInstanceData->fileSize = fileInfo.st_size;
	channelInstanceData->path = path;
	Tcl_IncrRefCount(path);

	channel = Tcl_CreateChannel(&xvfs_tclfs_channelType, Tcl_GetString(channelName), channelInstanceData, TCL_READABLE);
	Tcl_DecrRefCount(channelName);
	if (!channel) {
		XVFS_DEBUG_PUTS("... failed");

		Tcl_DecrRefCount(path);
		Tcl_Free((char *) channelInstanceData);

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	channelInstanceData->channel = channel;

	XVFS_DEBUG_PRINTF("... ok (%p)", channelInstanceData);

	XVFS_DEBUG_LEAVE;
	return(channel);
}

static int xvfs_tclfs_closeChannel(ClientData channelInstanceData_p, Tcl_Interp *interp);
static int xvfs_tclfs_closeChannelEvent(Tcl_Event *event_p, int flags) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	struct xvfs_tclfs_channel_event *event;

	event = (struct xvfs_tclfs_channel_event *) event_p;
	channelInstanceData = event->channelInstanceData;

	channelInstanceData->queuedEvents--;

	xvfs_tclfs_closeChannel((ClientData) channelInstanceData, NULL);

	return(1);
}

static int xvfs_tclfs_closeChannel(ClientData channelInstanceData_p, Tcl_Interp *interp) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	struct xvfs_tclfs_channel_event *event;

	XVFS_DEBUG_ENTER;
	XVFS_DEBUG_PRINTF("Closing channel %p ...", channelInstanceData_p);

	channelInstanceData = (struct xvfs_tclfs_channel_id *) channelInstanceData_p;

	channelInstanceData->closed = 1;

	if (channelInstanceData->queuedEvents != 0) {
		XVFS_DEBUG_PUTS("... queued");

		event = (struct xvfs_tclfs_channel_event *) Tcl_Alloc(sizeof(*event));
		event->tcl.proc = xvfs_tclfs_closeChannelEvent;
		event->tcl.nextPtr = NULL;
		event->channelInstanceData = channelInstanceData;

		channelInstanceData->queuedEvents++;

		Tcl_QueueEvent((Tcl_Event *) event, TCL_QUEUE_TAIL);

		XVFS_DEBUG_LEAVE;
		return(0);
	}

	Tcl_DecrRefCount(channelInstanceData->path);
	Tcl_Free((char *) channelInstanceData);

	XVFS_DEBUG_PUTS("... ok");

	XVFS_DEBUG_LEAVE;
	return(0);
}

static int xvfs_tclfs_readChannel(ClientData channelInstanceData_p, char *buf, int bufSize, int *errorCodePtr) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	const unsigned char *data;
	Tcl_WideInt offset, length;
	char *path;

	channelInstanceData = (struct xvfs_tclfs_channel_id *) channelInstanceData_p;

	/*
	 * If we are already at the end of the file we can skip
	 * attempting to read it
	 */
	if (channelInstanceData->eofMarked) {
		return(0);
	}

	path = Tcl_GetString(channelInstanceData->path);
	offset = channelInstanceData->currentOffset;
	length = bufSize;

	data = channelInstanceData->fsInstanceInfo->fsInfo->getDataProc(path, offset, &length);

	if (length < 0) {
		*errorCodePtr = xvfs_errorToErrno(length);

		return(-1);
	}

	if (length == 0) {
		channelInstanceData->eofMarked = 1;
	} else {
		memcpy(buf, data, length);

		channelInstanceData->currentOffset += length;
	}

	return(length);
}

static int xvfs_tclfs_watchChannelEvent(Tcl_Event *event_p, int flags) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	struct xvfs_tclfs_channel_event *event;

	event = (struct xvfs_tclfs_channel_event *) event_p;
	channelInstanceData = event->channelInstanceData;

	channelInstanceData->queuedEvents--;

	if (channelInstanceData->closed) {
		return(1);
	}

	Tcl_NotifyChannel(channelInstanceData->channel, TCL_READABLE);

	return(1);
}

static void xvfs_tclfs_watchChannel(ClientData channelInstanceData_p, int mask) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	struct xvfs_tclfs_channel_event *event;

	if ((mask & TCL_READABLE) != TCL_READABLE) {
		return;
	}

	channelInstanceData = (struct xvfs_tclfs_channel_id *) channelInstanceData_p;

	/*
	 * If the read call has marked that we have reached EOF,
	 * do not signal any further
	 */
	if (channelInstanceData->eofMarked) {
		return;
	}

	event = (struct xvfs_tclfs_channel_event *) Tcl_Alloc(sizeof(*event));
	event->tcl.proc = xvfs_tclfs_watchChannelEvent;
	event->tcl.nextPtr = NULL;
	event->channelInstanceData = channelInstanceData;

	channelInstanceData->queuedEvents++;

	Tcl_QueueEvent((Tcl_Event *) event, TCL_QUEUE_TAIL);

	return;
}

static int xvfs_tclfs_seekChannel(ClientData channelInstanceData_p, long offset, int mode, int *errorCodePtr) {
	struct xvfs_tclfs_channel_id *channelInstanceData;
	Tcl_WideInt newOffset, fileSize;

	channelInstanceData = (struct xvfs_tclfs_channel_id *) channelInstanceData_p;

	newOffset = channelInstanceData->currentOffset;
	fileSize = channelInstanceData->fileSize;

	switch (mode) {
		case SEEK_CUR:
			newOffset += offset;
			break;
		case SEEK_SET:
			newOffset = offset;
			break;
		case SEEK_END:
			newOffset = fileSize + offset;
			break;
		default:
			*errorCodePtr = xvfs_errorToErrno(XVFS_RV_ERR_EINVAL);

			return(-1);
	}

	/*
	 * We allow users to seek right up to the end of the buffer, but
	 * no further, this way if they want to seek backwards from there
	 * it is possible to do so.
	 */
	if (newOffset < 0 || newOffset > fileSize) {
		*errorCodePtr = xvfs_errorToErrno(XVFS_RV_ERR_EINVAL);

		return(-1);
	}

	if (newOffset != channelInstanceData->currentOffset) {
		channelInstanceData->eofMarked = 0;
		channelInstanceData->currentOffset = newOffset;
	}

	return(channelInstanceData->currentOffset);
}

static void xvfs_tclfs_prepareChannelType(void) {
	xvfs_tclfs_channelType.typeName = "xvfs";
	xvfs_tclfs_channelType.version = TCL_CHANNEL_VERSION_2;
	xvfs_tclfs_channelType.closeProc = xvfs_tclfs_closeChannel;
	xvfs_tclfs_channelType.inputProc = xvfs_tclfs_readChannel;
	xvfs_tclfs_channelType.outputProc = NULL;
	xvfs_tclfs_channelType.watchProc = xvfs_tclfs_watchChannel;
	xvfs_tclfs_channelType.getHandleProc = NULL;
	xvfs_tclfs_channelType.seekProc = xvfs_tclfs_seekChannel;
	xvfs_tclfs_channelType.setOptionProc = NULL;
	xvfs_tclfs_channelType.getOptionProc = NULL;
	xvfs_tclfs_channelType.close2Proc = NULL;
	xvfs_tclfs_channelType.blockModeProc = NULL;
	xvfs_tclfs_channelType.flushProc = NULL;
	xvfs_tclfs_channelType.handlerProc = NULL;
	xvfs_tclfs_channelType.wideSeekProc = NULL;
	xvfs_tclfs_channelType.threadActionProc = NULL;
	xvfs_tclfs_channelType.truncateProc = NULL;
}

/*
 * Internal Tcl_Filesystem functions, with the appropriate instance info
 */
static int xvfs_tclfs_pathInFilesystem(Tcl_Obj *path, ClientData *dataPtr, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *relativePath;
	int retval;

	XVFS_DEBUG_ENTER;

	XVFS_DEBUG_PRINTF("Checking to see if path \"%s\" is in the filesystem ...", Tcl_GetString(path));

	path = xvfs_absolutePath(path);

	relativePath = xvfs_relativePath(path, instanceInfo);

	retval = TCL_OK;
	if (!relativePath) {
		retval = -1;
	}

	Tcl_DecrRefCount(path);

	XVFS_DEBUG_PRINTF("... %s", retval == -1 ? "no" : "yes");

	XVFS_DEBUG_LEAVE;
	return(retval);
}

static int xvfs_tclfs_stat(Tcl_Obj *path, Tcl_StatBuf *statBuf, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *pathStr;
	int retval;

	XVFS_DEBUG_ENTER;

	XVFS_DEBUG_PRINTF("Getting stat() on \"%s\" ...", Tcl_GetString(path));

	path = xvfs_absolutePath(path);

	pathStr = xvfs_relativePath(path, instanceInfo);

	retval = instanceInfo->fsInfo->getStatProc(pathStr, statBuf);
	if (retval < 0) {
		XVFS_DEBUG_PRINTF("... failed: %s", xvfs_strerror(retval));

		Tcl_SetErrno(xvfs_errorToErrno(retval));

		retval = -1;
	} else {
		XVFS_DEBUG_PUTS("... ok");
	}

	Tcl_DecrRefCount(path);

	XVFS_DEBUG_LEAVE;
	return(retval);
}

static int xvfs_tclfs_access(Tcl_Obj *path, int mode, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *pathStr;
	Tcl_StatBuf fileInfo;
	int statRetVal;

	XVFS_DEBUG_ENTER;

	XVFS_DEBUG_PRINTF("Getting access(..., %i) on \"%s\" ...", mode, Tcl_GetString(path));

	if (mode & W_OK) {
		XVFS_DEBUG_PUTS("... no (not writable)");

		XVFS_DEBUG_LEAVE;
		return(-1);
	}

	path = xvfs_absolutePath(path);

	pathStr = xvfs_relativePath(path, instanceInfo);
	if (!pathStr) {
		XVFS_DEBUG_PUTS("... no (not in our path)");

		Tcl_DecrRefCount(path);

		XVFS_DEBUG_LEAVE;
		return(-1);
	}

	statRetVal = instanceInfo->fsInfo->getStatProc(pathStr, &fileInfo);
	if (statRetVal < 0) {
		XVFS_DEBUG_PUTS("... no (not statable)");

		Tcl_DecrRefCount(path);

		XVFS_DEBUG_LEAVE;
		return(-1);
	}

	if (mode & X_OK) {
		if (!(fileInfo.st_mode & 040000)) {
			XVFS_DEBUG_PUTS("... no (not a directory and X_OK specified)");

			Tcl_DecrRefCount(path);

			XVFS_DEBUG_LEAVE;
			return(-1);
		}
	}

	Tcl_DecrRefCount(path);

	XVFS_DEBUG_PUTS("... ok");

	XVFS_DEBUG_LEAVE;
	return(0);
}

static Tcl_Channel xvfs_tclfs_openFileChannel(Tcl_Interp *interp, Tcl_Obj *path, int mode, int permissions, struct xvfs_tclfs_instance_info *instanceInfo) {
	Tcl_Channel retval;
	Tcl_Obj *pathRel;
	const char *pathStr;

	XVFS_DEBUG_ENTER;

	XVFS_DEBUG_PRINTF("Asked to open(\"%s\", %x)...", Tcl_GetString(path), mode);

	if (mode & O_WRONLY) {
		XVFS_DEBUG_PUTS("... failed (asked to open for writing)");

		xvfs_setresults_error(interp, XVFS_RV_ERR_EROFS);

		XVFS_DEBUG_LEAVE;
		return(NULL);
	}

	path = xvfs_absolutePath(path);

	pathStr = xvfs_relativePath(path, instanceInfo);

	pathRel = Tcl_NewStringObj(pathStr, -1);

	Tcl_DecrRefCount(path);

	XVFS_DEBUG_PUTS("... done, passing off to channel handler");

	retval = xvfs_tclfs_openChannel(interp, pathRel, instanceInfo);

	XVFS_DEBUG_LEAVE;
	return(retval);
}

static int xvfs_tclfs_verifyType(Tcl_Obj *path, Tcl_GlobTypeData *types, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *pathStr;
	Tcl_StatBuf fileInfo;
	int statRetVal;

	XVFS_DEBUG_ENTER;

	if (types) {
		XVFS_DEBUG_PRINTF("Asked to verify the existence and type of \"%s\" matches type=%i and perm=%i ...", Tcl_GetString(path), types->type, types->perm);
	} else {
		XVFS_DEBUG_PRINTF("Asked to verify the existence \"%s\" ...", Tcl_GetString(path));
	}

	statRetVal = xvfs_tclfs_stat(path, &fileInfo, instanceInfo);
	if (statRetVal != 0) {
		XVFS_DEBUG_PUTS("... no (cannot stat)");

		XVFS_DEBUG_LEAVE;
		return(0);
	}

	if (!types) {
		XVFS_DEBUG_PUTS("... yes");

		XVFS_DEBUG_LEAVE;
		return(1);
	}

	if (types->perm != TCL_GLOB_PERM_RONLY) {
		if (types->perm & (TCL_GLOB_PERM_W | TCL_GLOB_PERM_HIDDEN)) {
			XVFS_DEBUG_PUTS("... no (checked for writable or hidden, not supported)");

			XVFS_DEBUG_LEAVE;
			return(0);
		}

		if ((types->perm & TCL_GLOB_PERM_X) == TCL_GLOB_PERM_X) {
			if (!(fileInfo.st_mode & 040000)) {
				XVFS_DEBUG_PUTS("... no (checked for executable but not a directory)");

				XVFS_DEBUG_LEAVE;
				return(0);
			}
		}
	}

	if (types->type & (TCL_GLOB_TYPE_BLOCK | TCL_GLOB_TYPE_CHAR | TCL_GLOB_TYPE_PIPE | TCL_GLOB_TYPE_SOCK | TCL_GLOB_TYPE_LINK)) {
		XVFS_DEBUG_PUTS("... no (checked for block, char, pipe, sock, or link, not supported)");

		XVFS_DEBUG_LEAVE;
		return(0);
	}

	if ((types->type & TCL_GLOB_TYPE_DIR) == TCL_GLOB_TYPE_DIR) {
		if (!(fileInfo.st_mode & 040000)) {
			XVFS_DEBUG_PUTS("... no (checked for directory but not a directory)");

			XVFS_DEBUG_LEAVE;
			return(0);
		}
	}

	if ((types->type & TCL_GLOB_TYPE_FILE) == TCL_GLOB_TYPE_FILE) {
		if (!(fileInfo.st_mode & 0100000)) {
			XVFS_DEBUG_PUTS("... no (checked for file but not a file)");

			XVFS_DEBUG_LEAVE;
			return(0);
		}
	}

	if ((types->type & TCL_GLOB_TYPE_MOUNT) == TCL_GLOB_TYPE_MOUNT) {
		path = xvfs_absolutePath(path);
		pathStr = xvfs_relativePath(path, instanceInfo);
		if (!pathStr) {
			XVFS_DEBUG_PUTS("... no (checked for mount but not able to resolve path)");

			Tcl_DecrRefCount(path);

			XVFS_DEBUG_LEAVE;
			return(0);
		}

		if (strlen(pathStr) != 0) {
			XVFS_DEBUG_PUTS("... no (checked for mount but not our top-level directory)");

			Tcl_DecrRefCount(path);

			XVFS_DEBUG_LEAVE;
			return(0);
		}

		Tcl_DecrRefCount(path);
	}

	XVFS_DEBUG_PUTS("... yes");

	XVFS_DEBUG_LEAVE;
	return(1);
}

static int xvfs_tclfs_matchInDir(Tcl_Interp *interp, Tcl_Obj *resultPtr, Tcl_Obj *path, const char *pattern, Tcl_GlobTypeData *types, struct xvfs_tclfs_instance_info *instanceInfo) {
	const char *pathStr;
	const char **children, *child;
	Tcl_WideInt childrenCount, idx;
	Tcl_Obj *childObj;
	int tclRetVal;

	if (pattern == NULL) {
		if (xvfs_tclfs_verifyType(path, types, instanceInfo)) {
			return(TCL_OK);
		}

		return(TCL_ERROR);
	}

	XVFS_DEBUG_ENTER;

	path = xvfs_absolutePath(path);

	if (types) {
		XVFS_DEBUG_PRINTF("Checking for files matching %s in \"%s\" and type=%i and perm=%i ...", pattern, Tcl_GetString(path), types->type, types->perm);
	} else {
		XVFS_DEBUG_PRINTF("Checking for files matching %s in \"%s\" ...", pattern, Tcl_GetString(path));
	}

	pathStr = xvfs_relativePath(path, instanceInfo);
	if (!pathStr) {
		XVFS_DEBUG_PUTS("... error (not in our VFS)");

		Tcl_DecrRefCount(path);

		xvfs_setresults_error(interp, XVFS_RV_ERR_ENOENT);

		XVFS_DEBUG_LEAVE;
		return(TCL_OK);
	}

	childrenCount = 0;
	children = instanceInfo->fsInfo->getChildrenProc(pathStr, &childrenCount);
	if (childrenCount < 0) {
		XVFS_DEBUG_PRINTF("... error: %s", xvfs_strerror(childrenCount));

		Tcl_DecrRefCount(path);

		xvfs_setresults_error(interp, childrenCount);

		XVFS_DEBUG_LEAVE;
		return(TCL_ERROR);
	}

	for (idx = 0; idx < childrenCount; idx++) {
		child = children[idx];

		if (!Tcl_StringMatch(child, pattern)) {
			continue;
		}

		childObj = Tcl_DuplicateObj(path);
		Tcl_IncrRefCount(childObj);
		Tcl_AppendStringsToObj(childObj, "/", child, NULL);

		if (!xvfs_tclfs_verifyType(childObj, types, instanceInfo)) {
			Tcl_DecrRefCount(childObj);

			continue;
		}

		tclRetVal = Tcl_ListObjAppendElement(interp, resultPtr, childObj);
		Tcl_DecrRefCount(childObj);

		if (tclRetVal != TCL_OK) {
			XVFS_DEBUG_PUTS("... error (lappend)");
			Tcl_DecrRefCount(path);

			XVFS_DEBUG_LEAVE;
			return(tclRetVal);
		}
	}

	Tcl_DecrRefCount(path);

	XVFS_DEBUG_PRINTF("... ok (returning items: %s)", Tcl_GetString(resultPtr));

	XVFS_DEBUG_LEAVE;
	return(TCL_OK);
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

static int xvfs_tclfs_standalone_access(Tcl_Obj *path, int mode) {
	return(xvfs_tclfs_access(path, mode, &xvfs_tclfs_standalone_info));
}

static Tcl_Channel xvfs_tclfs_standalone_openFileChannel(Tcl_Interp *interp, Tcl_Obj *path, int mode, int permissions) {
	return(xvfs_tclfs_openFileChannel(interp, path, mode, permissions, &xvfs_tclfs_standalone_info));
}

static int xvfs_tclfs_standalone_matchInDir(Tcl_Interp *interp, Tcl_Obj *resultPtr, Tcl_Obj *pathPtr, const char *pattern, Tcl_GlobTypeData *types) {
	return(xvfs_tclfs_matchInDir(interp, resultPtr, pathPtr, pattern, types, &xvfs_tclfs_standalone_info));
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
static int xvfs_standalone_register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	int tclRet;
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

	xvfs_tclfs_standalone_fs.typeName                   = "xvfsInstance";
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
	xvfs_tclfs_standalone_fs.accessProc                 = xvfs_tclfs_standalone_access;
	xvfs_tclfs_standalone_fs.openFileChannelProc        = xvfs_tclfs_standalone_openFileChannel;
	xvfs_tclfs_standalone_fs.matchInDirectoryProc       = xvfs_tclfs_standalone_matchInDir;
	xvfs_tclfs_standalone_fs.utimeProc                  = NULL;
	xvfs_tclfs_standalone_fs.linkProc                   = NULL;
	xvfs_tclfs_standalone_fs.listVolumesProc            = NULL;
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

	Tcl_IncrRefCount(xvfs_tclfs_standalone_info.mountpoint);
	Tcl_AppendStringsToObj(xvfs_tclfs_standalone_info.mountpoint, XVFS_ROOT_MOUNTPOINT, fsInfo->name, NULL);
	
	tclRet = Tcl_FSRegister(NULL, &xvfs_tclfs_standalone_fs);
	if (tclRet != TCL_OK) {
		Tcl_DecrRefCount(xvfs_tclfs_standalone_info.mountpoint);

		if (interp) {
			Tcl_SetResult(interp, "Tcl_FSRegister() failed", NULL);
		}

		return(tclRet);
	}

	xvfs_tclfs_prepareChannelType();

	return(TCL_OK);
}
#endif /* XVFS_MODE_STANDALONE || XVFS_MODE_FLEXIBLE */

#if defined(XVFS_MODE_FLEXIBLE)
static int xvfs_flexible_register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	ClientData fsHandlerDataRaw;
	struct xvfs_tclfs_server_info *fsHandlerData;
	const Tcl_Filesystem *fsHandler;
	int (*xvfs_register)(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);
	Tcl_Obj *rootPathObj;

	XVFS_DEBUG_ENTER;

	xvfs_register = &xvfs_standalone_register;

	rootPathObj = Tcl_NewStringObj(XVFS_ROOT_MOUNTPOINT, -1);
	if (!rootPathObj) {
		XVFS_DEBUG_LEAVE;

		return(xvfs_register(interp, fsInfo));
	}

	Tcl_IncrRefCount(rootPathObj);
	fsHandler = Tcl_FSGetFileSystemForPath(rootPathObj);
	Tcl_DecrRefCount(rootPathObj);

	if (!fsHandler) {
		XVFS_DEBUG_LEAVE;

		return(xvfs_register(interp, fsInfo));
	}

	fsHandlerDataRaw = Tcl_FSData(fsHandler);
	if (!fsHandlerDataRaw) {
		XVFS_DEBUG_LEAVE;

		return(xvfs_register(interp, fsInfo));
	}

	fsHandlerData = (struct xvfs_tclfs_server_info *) fsHandlerDataRaw;

	/*
	 * XXX:TODO: What is the chance that the handler for //xvfs:/ hold
	 * client data smaller than XVFS_INTERNAL_SERVER_MAGIC_LEN ?
	 */
	if (memcmp(fsHandlerData->magic, XVFS_INTERNAL_SERVER_MAGIC, sizeof(fsHandlerData->magic)) == 0) {
		XVFS_DEBUG_PUTS("Found a server handler");
		xvfs_register = fsHandlerData->registerProc;
	}

	XVFS_DEBUG_LEAVE;

	return(xvfs_register(interp, fsInfo));
}
#endif /* XVFS_MODE_FLEXIBLE */

#if defined(XVFS_MODE_SERVER)
static Tcl_Filesystem xvfs_tclfs_dispatch_fs;
static Tcl_HashTable xvfs_tclfs_dispatch_map;
static struct xvfs_tclfs_server_info xvfs_tclfs_dispatch_fsdata;

static int xvfs_tclfs_dispatch_pathInFilesystem(Tcl_Obj *path, ClientData *dataPtr) {
	const char *pathStr, *rootStr;
	int pathLen, rootLen;

	XVFS_DEBUG_ENTER;

	XVFS_DEBUG_PRINTF("Verifying that \"%s\" belongs in XVFS ...", Tcl_GetString(path));
	
	rootStr = XVFS_ROOT_MOUNTPOINT;
	rootLen = strlen(XVFS_ROOT_MOUNTPOINT);

	pathStr = Tcl_GetStringFromObj(path, &pathLen);

	if (pathLen < rootLen) {
		XVFS_DEBUG_PUTS("... failed (length too short)");
		XVFS_DEBUG_LEAVE;
		return(-1);
	}

	if (memcmp(pathStr, rootStr, rootLen) != 0) {
		XVFS_DEBUG_PUTS("... failed (incorrect prefix)");
		XVFS_DEBUG_LEAVE;
		return(-1);
	}

	XVFS_DEBUG_PUTS("... yes");

	XVFS_DEBUG_LEAVE;

	return(TCL_OK);
}

static struct xvfs_tclfs_instance_info *xvfs_tclfs_dispatch_pathToInfo(Tcl_Obj *path) {
	Tcl_HashEntry *mapEntry;
	struct xvfs_tclfs_instance_info *retval;
	int rootLen;
	char *pathStr, *fsName, *fsNameEnds, origSep;

	XVFS_DEBUG_ENTER;

	if (xvfs_tclfs_dispatch_pathInFilesystem(path, NULL) != TCL_OK) {
		XVFS_DEBUG_LEAVE;

		return(NULL);
	}

	rootLen = strlen(XVFS_ROOT_MOUNTPOINT);
	pathStr = Tcl_GetString(path);

	fsName = ((char *) pathStr) + rootLen;

	fsNameEnds = strchr(fsName, '/');
	if (fsNameEnds) {
		origSep = *fsNameEnds;
		*fsNameEnds = '\0';
	}

	XVFS_DEBUG_PRINTF("... fsName = %s...", fsName);

	mapEntry = Tcl_FindHashEntry(&xvfs_tclfs_dispatch_map, fsName);

	if (fsNameEnds) {
		*fsNameEnds = origSep;
	}

	if (mapEntry) {
		retval = (struct xvfs_tclfs_instance_info *) Tcl_GetHashValue(mapEntry);
		XVFS_DEBUG_PRINTF("... found a registered filesystem: %p", retval);
	} else {
		retval = NULL;
		XVFS_DEBUG_PUTS("... found no registered filesystem.");
	}

	XVFS_DEBUG_LEAVE;
	return(retval);
}

static int xvfs_tclfs_dispatch_stat(Tcl_Obj *path, Tcl_StatBuf *statBuf) {
	struct xvfs_tclfs_instance_info *instanceInfo;

	instanceInfo = xvfs_tclfs_dispatch_pathToInfo(path);
	if (!instanceInfo) {
		Tcl_SetErrno(xvfs_errorToErrno(XVFS_RV_ERR_ENOENT));

		return(-1);
	}

	return(xvfs_tclfs_stat(path, statBuf, instanceInfo));
}

static int xvfs_tclfs_dispatch_access(Tcl_Obj *path, int mode) {
	struct xvfs_tclfs_instance_info *instanceInfo;

	instanceInfo = xvfs_tclfs_dispatch_pathToInfo(path);
	if (!instanceInfo) {
		return(-1);
	}

	return(xvfs_tclfs_access(path, mode, instanceInfo));
}

static Tcl_Channel xvfs_tclfs_dispatch_openFileChannel(Tcl_Interp *interp, Tcl_Obj *path, int mode, int permissions) {
	struct xvfs_tclfs_instance_info *instanceInfo;

	instanceInfo = xvfs_tclfs_dispatch_pathToInfo(path);
	if (!instanceInfo) {
		return(NULL);
	}

	return(xvfs_tclfs_openFileChannel(interp, path, mode, permissions, instanceInfo));
}

static int xvfs_tclfs_dispatch_matchInDir(Tcl_Interp *interp, Tcl_Obj *resultPtr, Tcl_Obj *pathPtr, const char *pattern, Tcl_GlobTypeData *types) {
	struct xvfs_tclfs_instance_info *instanceInfo;

	instanceInfo = xvfs_tclfs_dispatch_pathToInfo(pathPtr);
	if (!instanceInfo) {
		return(TCL_ERROR);
	}

	return(xvfs_tclfs_matchInDir(interp, resultPtr, pathPtr, pattern, types, instanceInfo));
}

int Xvfs_Init(Tcl_Interp *interp) {
	static int registered = 0;
	int tclRet;

	/* XXX:TODO: Make this thread-safe */
	if (registered) {
		return(TCL_OK);
	}
	registered = 1;

	xvfs_tclfs_dispatch_fs.typeName                   = "xvfsDispatch";
	xvfs_tclfs_dispatch_fs.structureLength            = sizeof(xvfs_tclfs_dispatch_fs);
	xvfs_tclfs_dispatch_fs.version                    = TCL_FILESYSTEM_VERSION_1;
	xvfs_tclfs_dispatch_fs.pathInFilesystemProc       = xvfs_tclfs_dispatch_pathInFilesystem;
	xvfs_tclfs_dispatch_fs.dupInternalRepProc         = NULL;
	xvfs_tclfs_dispatch_fs.freeInternalRepProc        = NULL;
	xvfs_tclfs_dispatch_fs.internalToNormalizedProc   = NULL;
	xvfs_tclfs_dispatch_fs.createInternalRepProc      = NULL;
	xvfs_tclfs_dispatch_fs.normalizePathProc          = NULL;
	xvfs_tclfs_dispatch_fs.filesystemPathTypeProc     = NULL;
	xvfs_tclfs_dispatch_fs.filesystemSeparatorProc    = NULL;
	xvfs_tclfs_dispatch_fs.statProc                   = xvfs_tclfs_dispatch_stat;
	xvfs_tclfs_dispatch_fs.accessProc                 = xvfs_tclfs_dispatch_access;
	xvfs_tclfs_dispatch_fs.openFileChannelProc        = xvfs_tclfs_dispatch_openFileChannel;
	xvfs_tclfs_dispatch_fs.matchInDirectoryProc       = xvfs_tclfs_dispatch_matchInDir;
	xvfs_tclfs_dispatch_fs.utimeProc                  = NULL;
	xvfs_tclfs_dispatch_fs.linkProc                   = NULL;
	xvfs_tclfs_dispatch_fs.listVolumesProc            = NULL;
	xvfs_tclfs_dispatch_fs.fileAttrStringsProc        = NULL;
	xvfs_tclfs_dispatch_fs.fileAttrsGetProc           = NULL;
	xvfs_tclfs_dispatch_fs.fileAttrsSetProc           = NULL;
	xvfs_tclfs_dispatch_fs.createDirectoryProc        = NULL;
	xvfs_tclfs_dispatch_fs.removeDirectoryProc        = NULL;
	xvfs_tclfs_dispatch_fs.deleteFileProc             = NULL;
	xvfs_tclfs_dispatch_fs.copyFileProc               = NULL;
	xvfs_tclfs_dispatch_fs.renameFileProc             = NULL;
	xvfs_tclfs_dispatch_fs.copyDirectoryProc          = NULL;
	xvfs_tclfs_dispatch_fs.lstatProc                  = NULL;
	xvfs_tclfs_dispatch_fs.loadFileProc               = NULL;
	xvfs_tclfs_dispatch_fs.getCwdProc                 = NULL;
	xvfs_tclfs_dispatch_fs.chdirProc                  = NULL;

	memcpy(xvfs_tclfs_dispatch_fsdata.magic, XVFS_INTERNAL_SERVER_MAGIC, XVFS_INTERNAL_SERVER_MAGIC_LEN);
	xvfs_tclfs_dispatch_fsdata.registerProc = Xvfs_Register;

	tclRet = Tcl_FSRegister((ClientData) &xvfs_tclfs_dispatch_fsdata, &xvfs_tclfs_dispatch_fs);
	if (tclRet != TCL_OK) {
		if (interp) {
			Tcl_SetResult(interp, "Tcl_FSRegister() failed", NULL);
		}

		return(tclRet);
	}

	/*
	 * Initialize the channel type we will use for I/O
	 */
	xvfs_tclfs_prepareChannelType();

	/*
	 * Initialize the map to lookup paths to registered
	 * filesystems
	 */
	Tcl_InitHashTable(&xvfs_tclfs_dispatch_map, TCL_STRING_KEYS);

	return(TCL_OK);
}

int Xvfs_Register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo) {
	Tcl_HashEntry *mapEntry;
	struct xvfs_tclfs_instance_info *instanceInfo;
	int dispatchInitRet;
	int new;

	dispatchInitRet = Xvfs_Init(interp);
	if (dispatchInitRet != TCL_OK) {
		return(dispatchInitRet);
	}

	/*
	 * Verify this is for a protocol we support
	 */
	if (fsInfo->protocolVersion != XVFS_PROTOCOL_VERSION) {
		if (interp) {
			Tcl_SetResult(interp, "Protocol mismatch", NULL);
		}
		return(TCL_ERROR);
	}

	/*
	 * Create the structure needed
	 */
	instanceInfo = (struct xvfs_tclfs_instance_info *) Tcl_Alloc(sizeof(*instanceInfo));
	instanceInfo->fsInfo = fsInfo;
	instanceInfo->mountpoint = Tcl_ObjPrintf("%s%s", XVFS_ROOT_MOUNTPOINT, fsInfo->name);
	Tcl_IncrRefCount(instanceInfo->mountpoint);

	/*
	 * Register a hash table entry for this name
	 */
	new = 0;
	mapEntry = Tcl_CreateHashEntry(&xvfs_tclfs_dispatch_map, fsInfo->name, &new);
	Tcl_SetHashValue(mapEntry, instanceInfo);

	return(TCL_OK);
}
#endif /* XVFS_MODE_SERVER */
#undef XVFS_DEBUG_PRINTF
#undef XVFS_DEBUG_PUTS
#undef XVFS_DEBUG_ENTER
#undef XVFS_DEBUG_LEAVE
#undef XVFS_INTERNAL_SERVER_MAGIC
#undef XVFS_INTERNAL_SERVER_MAGIC_LEN
#undef XVFS_ROOT_MOUNTPOINT

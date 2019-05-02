#ifndef XVFS_COMMON_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817
#define XVFS_COMMON_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817 1

#include <tcl.h>

#define XVFS_PROTOCOL_VERSION 1

typedef const char **(*xvfs_proc_getChildren_t)(const char *path, Tcl_WideInt *count);
typedef const unsigned char *(*xvfs_proc_getData_t)(const char *path, Tcl_WideInt start, Tcl_WideInt *length);
typedef int (*xvfs_proc_getInfo_t)(const char *path, Tcl_StatBuf *statBuf);

struct Xvfs_FSInfo {
	int                      protocolVersion;
	const char               *name;
	xvfs_proc_getChildren_t  getChildrenProc;
	xvfs_proc_getData_t      getDataProc;
	xvfs_proc_getInfo_t      getInfoProc;
};

int Xvfs_Register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);

#endif

#ifndef XVFS_COMMON_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817
#define XVFS_COMMON_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817 1

#include <tcl.h>

#define XVFS_PROTOCOL_VERSION 1

typedef const char **(*xvfs_proc_getChildren_t)(const char *path, Tcl_WideInt *count);
typedef const unsigned char *(*xvfs_proc_getData_t)(const char *path, Tcl_WideInt start, Tcl_WideInt *length);

struct Xvfs_FSInfo {
	int                      protocolVersion;
	const char               *fsName;
	xvfs_proc_getChildren_t  getChildrenProc;
	xvfs_proc_getData_t      getDataProc;
};

int Xvfs_Register(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);

#endif

#ifndef XVFS_CORE_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817
#define XVFS_CORE_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817 1

#include <tcl.h>

#define XVFS_PROTOCOL_VERSION 1
#define XVFS_PROTOCOL_SERVER_MAGIC "\xD4\xF3\x05\x96\x25\xCF\xAF\xFE"
#define XVFS_PROTOCOL_SERVER_MAGIC_LEN 8

typedef const char **(*xvfs_proc_getChildren_t)(const char *path, Tcl_WideInt *count);
typedef const unsigned char *(*xvfs_proc_getData_t)(const char *path, Tcl_WideInt start, Tcl_WideInt *length);
typedef int (*xvfs_proc_getStat_t)(const char *path, Tcl_StatBuf *statBuf);

struct Xvfs_FSInfo {
	int                      protocolVersion;
	const char               *name;
	xvfs_proc_getChildren_t  getChildrenProc;
	xvfs_proc_getData_t      getDataProc;
	xvfs_proc_getStat_t      getStatProc;
};

#define XVFS_REGISTER_INTERFACE(name) int name(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);

#if defined(XVFS_MODE_STANDALONE)
/*
 * In standalone mode, we just redefine calls to
 * Xvfs_Register() to go to the xvfs_standalone_register()
 * function
 */
#  define Xvfs_Register xvfs_standalone_register
XVFS_REGISTER_INTERFACE(Xvfs_Register)

#elif defined(XVFS_MODE_FLEXIBLE)
/*
 * In flexible mode we declare an external symbol named
 * Xvfs_Register(), as well as an internal symbol called
 * xvfs_flexible_register(), which we redefine future
 * calls to Xvfs_Register() to invoke
 */
extern XVFS_REGISTER_INTERFACE(Xvfs_Register)
#  define Xvfs_Register xvfs_flexible_register
XVFS_REGISTER_INTERFACE(Xvfs_Register)

#elif defined(XVFS_MODE_CLIENT)
/*
 * In client mode we declare an external symbol named
 * Xvfs_Register() that must be provided by the environment
 * we are loaded into
 */
extern XVFS_REGISTER_INTERFACE(Xvfs_Register)

#elif defined(XVFS_MODE_SERVER)
/*
 * In server mode we are going to implementing Xvfs_Register()
 * for flexible/client modes, just forward declare it
 */
XVFS_REGISTER_INTERFACE(Xvfs_Register)

#else
#  error Unsupported XVFS_MODE
#endif


#endif

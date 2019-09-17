#ifndef XVFS_CORE_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817
#define XVFS_CORE_H_1B4B28D60EBAA11D5FF85642FA7CA22C29E8E817 1

#include <tcl.h>

#define XVFS_PROTOCOL_VERSION 1

typedef const char **(*xvfs_proc_getChildren_t)(const char *path, Tcl_WideInt *count);
typedef const unsigned char *(*xvfs_proc_getData_t)(const char *path, Tcl_WideInt start, Tcl_WideInt *length);
typedef int (*xvfs_proc_getStat_t)(const char *path, Tcl_StatBuf *statBuf);

/*
 * Interface for the filesystem to fill out before registering.
 * The protocolVersion is provided first so that if this
 * needs to change over time it can be appropriately handled.
 */
struct Xvfs_FSInfo {
	int                      protocolVersion;
	const char               *name;
	xvfs_proc_getChildren_t  getChildrenProc;
	xvfs_proc_getData_t      getDataProc;
	xvfs_proc_getStat_t      getStatProc;
};

/*
 * Error codes for various calls.  This is part of the ABI and must
 * not be changed.
 */
#define XVFS_RV_ERR_ENOENT   (-8192)
#define XVFS_RV_ERR_EINVAL   (-8193)
#define XVFS_RV_ERR_EISDIR   (-8194)
#define XVFS_RV_ERR_ENOTDIR  (-8195)
#define XVFS_RV_ERR_EFAULT   (-8196)
#define XVFS_RV_ERR_EROFS    (-8197)
#define XVFS_RV_ERR_INTERNAL (-16383)

#define XVFS_REGISTER_INTERFACE(name) int name(Tcl_Interp *interp, struct Xvfs_FSInfo *fsInfo);

#if defined(XVFS_MODE_STANDALONE)
/*
 * In standalone mode, we just redefine calls to
 * Xvfs_Register() to go to the xvfs_standalone_register()
 * function
 */
#  define Xvfs_Register xvfs_standalone_register
static XVFS_REGISTER_INTERFACE(Xvfs_Register)

#elif defined(XVFS_MODE_FLEXIBLE)
/*
 * In flexible mode, we just redefine calls to
 * Xvfs_Register() to go to the xvfs_flexible_register()
 * function which will either dispatch to a common
 * core XVFS or use the xvfs_standalone_register()
 * function as a standalone would.
 */
#  define Xvfs_Register xvfs_flexible_register
static XVFS_REGISTER_INTERFACE(Xvfs_Register)

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
int Xvfs_Init(Tcl_Interp *interp);

#else
#  error Unsupported XVFS_MODE
#endif

/*
 * In flexible or standalone mode, directly include what
 * would otherwise be a separate translation unit, to
 * avoid symbols leaking
 */
#if defined(XVFS_MODE_FLEXIBLE) || defined(XVFS_MODE_STANDALONE)
#include <xvfs-core.c>
#endif

#endif

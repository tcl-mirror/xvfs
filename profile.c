#include <tcl.h>
#include <stdio.h>

extern int Xvfs_Init(Tcl_Interp *interp);
extern int Xvfs_example_Init(Tcl_Interp *interp);
int main(int argc, char **argv) {
	Tcl_Interp *interp;
	int tclRet;
	int try;

	interp = Tcl_CreateInterp();
	if (!interp) {
		fprintf(stderr, "Tcl_CreateInterp failed\n");

		return(1);
	}

	tclRet = Tcl_Init(interp);
	if (tclRet != TCL_OK) {
		fprintf(stderr, "Tcl_Init failed: %s\n", Tcl_GetStringResult(interp));

		return(1);
	}

	tclRet = Xvfs_Init(interp);
	if (tclRet != TCL_OK) {
		fprintf(stderr, "Xvfs_Init failed: %s\n", Tcl_GetStringResult(interp));

		return(1);
	}
	tclRet = Xvfs_example_Init(interp);
	if (tclRet != TCL_OK) {
		fprintf(stderr, "Xvfs_example_Init failed: %s\n", Tcl_GetStringResult(interp));

		return(1);
	}

	Tcl_Eval(interp, "proc benchmark args { glob -directory //xvfs:/example * }");

#ifdef XVFS_PROFILE_TESTS
	Tcl_Eval(interp, "proc exit args {}");
	Tcl_Eval(interp, "proc puts args {}");
	Tcl_SetVar(interp, "argv", "-verbose {}", 0);
	for (try = 0; try < 1000; try++) {
		Tcl_EvalFile(interp, "//xvfs:/example/main.tcl");
	}
#else
	for (try = 0; try < 1000000; try++) {
		Tcl_Eval(interp, "benchmark");
	}
#endif

	return(0);
}

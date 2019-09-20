#include <tcl.h>
#include <stdio.h>
#include <stdlib.h>

#undef  XVFS_DEBUG
#define XVFS_MODE_STANDALONE
#include "example.c"

int main(int argc, char **argv) {
	Tcl_Interp *interp;
	int profileTests, profileBenchmark;
	int tclRet;
	int try;

	profileTests = 0;
	profileBenchmark = 1000000;

	if (argc > 1) {
		profileBenchmark = atoi(argv[1]);
	}

	if (argc > 2) {
		profileTests = atoi(argv[2]);
	}

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

	tclRet = Xvfs_example_Init(interp);
	if (tclRet != TCL_OK) {
		fprintf(stderr, "Xvfs_example_Init failed: %s\n", Tcl_GetStringResult(interp));

		return(1);
	}

	Tcl_Eval(interp, "proc benchmark args { glob -directory //xvfs:/example * }");
	for (try = 0; try < profileBenchmark; try++) {
		Tcl_Eval(interp, "benchmark");
	}

	Tcl_Eval(interp, "proc exit args {}");
	Tcl_Eval(interp, "proc puts args {}");
	Tcl_SetVar(interp, "argv", "-verbose {}", 0);
	for (try = 0; try < profileTests; try++) {
		Tcl_EvalFile(interp, "//xvfs:/example/main.tcl");
	}

	return(0);
}

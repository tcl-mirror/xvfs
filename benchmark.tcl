#! /usr/bin/env tclsh

set LIB_SUFFIX [info sharedlibextension]
load -global ./xvfs${LIB_SUFFIX}; # Optional, uses a dispatcher
load ./example-flexible${LIB_SUFFIX} Xvfs_example

set benchmarkFormat "%-6s %-5s: %s"

proc benchmark {type name code} {
	time $code 100
	set time [time $code 10000]

	puts [format $::benchmarkFormat $type $name $time]
}

set rootDirMap(xvfs) "//xvfs:/example"
set rootDirMap(native) [file join [pwd] example]

set test(CD) {
	cd $::rootDir
	pwd
}

set test(Read) {
	set fd [open ${::rootDir}/main.tcl]
	read $fd
	close $fd
}

set test(Glob) {
	glob -directory ${::rootDir} *
}

foreach {testName testBody} [lsort -stride 2 -dictionary [array get test]] {
	foreach rootDirType {xvfs native} {
		set rootDir $rootDirMap($rootDirType)
		benchmark $rootDirType $testName $testBody
	}
}

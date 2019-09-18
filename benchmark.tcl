#! /usr/bin/env tclsh

set LIB_SUFFIX [info sharedlibextension]
load -global ./xvfs${LIB_SUFFIX}; # Optional, uses a dispatcher
load ./example-flexible${LIB_SUFFIX} Xvfs_example

set benchmarkFormat "%-6s  %-11s  %s"

proc benchmark {type name code} {
	set iterations 10000

	set work [string trim [lindex [split [string trim $code] "\n"] 0]]
	if {[string match "# iterations: *" $work]} {
		set iterations [lindex $work 2]
	}
	
	time $code [expr {max($iterations / 100, 1)}]
	set time [time $code $iterations]

	puts [format $::benchmarkFormat $type $name $time]
}

set rootDirMap(xvfs) "//xvfs:/example"
set rootDirMap(native) [file join [pwd] example]

proc recursiveGlob {dir} {
	foreach subDir [glob -nocomplain -directory $dir -types d *] {
		recursiveGlob $subDir
	}
}

array set test {
	CD {
		cd $::rootDir
		pwd
	}
	Read {
		set fd [open ${::rootDir}/main.tcl]
		read $fd
		close $fd
	}
	"Read Async" {
		# iterations: 1000
		set ::async_read_done false
		set fd [open ${::rootDir}/main.tcl]
		fconfigure $fd -blocking false
		fileevent $fd readable [list apply {{fd} {
			set input [read $fd 512]
			if {[eof $fd] && $input eq ""} {
				set ::async_read_done true
				return
			}
		}} $fd]
		vwait ::async_read_done
		close $fd
	}
	Glob {
		glob -directory ${::rootDir} *
	}
	Search {
		recursiveGlob ${::rootDir}
	}
	+Stat {
		file stat ${::rootDir}/main.tcl UNUSED
	}
	-Stat {
		catch {
			file stat ${::rootDir}/DOES-NOT-EXIST UNUSED
		}
	}
}

foreach {testName testBody} [lsort -stride 2 -dictionary [array get test]] {
	foreach rootDirType {xvfs native} {
		set rootDir $rootDirMap($rootDirType)
		benchmark $rootDirType $testName $testBody
	}
}

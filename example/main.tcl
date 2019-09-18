#! /usr/bin/env tclsh

package require tcltest

tcltest::testConstraint tcl87 [string match "8.7.*" [info patchlevel]]

tcltest::configure -verbose pbse
tcltest::configure {*}$argv

if {![info exists ::env(XVFS_ROOT_MOUNTPOINT)]} {
	set xvfsRootMountpoint "//xvfs:"
} else {
	set xvfsRootMountpoint $::env(XVFS_ROOT_MOUNTPOINT)
}

set rootDir "${xvfsRootMountpoint}/example"
set rootDirNative  [file join [pwd] example]
#set rootDir $rootDirNative
set testFile "${rootDir}/foo"

proc glob_verify {args} {
	set rv [glob -nocomplain -directory $::rootDir {*}$args]
	set verify [glob -nocomplain -directory $::rootDirNative {*}$args]

	if {[llength $rv] != [llength $verify]} {
		error "VERIFY FAILED: glob ... $args ($rv versus $verify)"
	}

	return $rv
}

proc test_summary {} {
	set format "| %7s | %7s | %7s | %7s |"
	set emptyRow [format $format "" "" "" ""]
	set emptyRowLength [string length $emptyRow]
	lappend output "/[string repeat - [expr {$emptyRowLength - 2}]]\\"
	lappend output [format $format "Passed" "Failed" "Skipped" "Total"]
	lappend output [regsub -all -- " " [regsub -all -- " \\| " $emptyRow " + "] "-"]
	lappend output [format $format \
		$::tcltest::numTests(Passed) \
		$::tcltest::numTests(Failed) \
		$::tcltest::numTests(Skipped) \
		$::tcltest::numTests(Total) \
	]
	lappend output "\\[string repeat - [expr {$emptyRowLength - 2}]]/"

	return [join $output "\n"]
}

tcltest::customMatch boolean [list apply {{expected actual} {
	if {!!$expected == !!$actual} {
		return true
	} else {
		return false
	}
}}]

tcltest::test xvfs-seek-basic "Xvfs Seek Test" -setup {
	set fd [open $testFile]
} -body {
	seek $fd 0 end
	seek $fd -1 current

	read $fd 1
} -cleanup {
	close $fd
	unset fd
} -result "\n"

tcltest::test xvfs-seek-past-eof "Xvfs Seek Past EOF File Test" -setup {
	set fd [open $testFile]
} -body {
	seek $fd 1 end
} -cleanup {
	close $fd
	unset fd
} -match glob -returnCodes error -result "*: invalid argument"

tcltest::test xvfs-seek-past-eof "Xvfs Seek Past EOF File Test" -setup {
	set fd [open $testFile]
} -body {
	seek $fd -10 current
} -cleanup {
	close $fd
	unset fd
} -match glob -returnCodes error -result "*: invalid argument"

tcltest::test xvfs-seek-read-past-eof "Xvfs Seek Then Read Past EOF Test" -setup {
	set fd [open $testFile]
} -body {
	seek $fd 0 end

	read $fd 1
	read $fd 1
} -cleanup {
	close $fd
	unset fd
} -result ""

tcltest::test xvfs-basic-open-neg "Xvfs Open Non-Existant File Test" -body {
	unset -nocomplain fd
	set fd [open $rootDir/does-not-exist]
} -cleanup {
	if {[info exists fd]} {
		close $fd
		unset fd
	}
} -returnCodes error -result "no such file or directory"

tcltest::test xvfs-basic-open-write "Xvfs Open For Writing Test" -body {
	unset -nocomplain fd
	set fd [open $rootDir/new-file w]
} -cleanup {
	if {[info exists fd]} {
		close $fd
		unset fd
	}
	catch {
		file delete $rootDir/new-file
	}
} -match glob -returnCodes error -result "*read*only file*system*"

tcltest::test xvfs-basic-open-directory "Xvfs Open Directory Test" -body {
	unset -nocomplain fd
	set fd [open $rootDir/lib]
	set fd
} -cleanup {
	if {[info exists fd]} {
		close $fd
		unset fd
	}
} -match glob -returnCodes error -result "*illegal operation on a directory"

tcltest::test xvfs-basic-two-files "Xvfs Multiple Open Files Test" -setup {
	set fd1 [open $testFile]
	set fd2 [open $testFile]
} -body {
	set data1 [read $fd1]
	close $fd1
	set data2 [read $fd2]
	close $fd2

	expr {$data1 eq $data2}
} -cleanup {
	unset -nocomplain fd1 fd2 data1 data2
} -match boolean -result true

tcltest::test xvfs-events "Xvfs Fileevent Test" -setup {
	set fd [open $testFile]
	seek $fd 0 end
	set size [tell $fd]
	seek $fd 0 start

	set done false
	set calls 0
	set output ""
} -body {
	fileevent $fd readable [list apply {{fd} {
		set pos [tell $fd]
		set x [read $fd 1]
		if {[string length $x] == 0} {
			set ::done true
			fileevent $fd readable ""
		}

		lappend ::output $pos
		incr ::calls
	}} $fd]
	vwait done

	list [expr {$calls == ($size + 1)}] [expr {[lsort -integer $output] eq $output}]
} -cleanup {
	close $fd
	update
	unset -nocomplain fd size done calls output
} -result {1 1}

tcltest::test xvfs-match-almost-root-neg "Xvfs Match Almost Root" -body {
	file exists ${rootDir}_DOES_NOT_EXIST
} -match boolean -result false

tcltest::test xvfs-glob-basic-any "Xvfs Glob Match Any Test" -body {
	llength [glob_verify *]
} -result 3

tcltest::test xvfs-glob-files-any "Xvfs Glob Match Any File Test" -body {
	llength [glob_verify -type f *]
} -result 2

tcltest::test xvfs-glob-dir-any "Xvfs Glob On a File Test" -body {
	glob -nocomplain -directory $testFile *
} -returnCodes error -result "not a directory"

tcltest::test xvfs-glob-basic-limited "Xvfs Glob Match Limited Test" -body {
	llength [glob_verify f*]
} -result 1

tcltest::test xvfs-glob-basic-limited-neg "Xvfs Glob Match Limited Negative Test" -body {
	llength [glob_verify x*]
} -result 0

tcltest::test xvfs-glob-basic-limited-prefixed "Xvfs Glob Match Limited But With Directory Prefix Test" -body {
	llength [glob_verify ./f*]
} -result 1

tcltest::test xvfs-glob-basic-limited-and-typed-prefixed "Xvfs Glob Match Limited Path and Type Positive Test" -body {
	llength [glob_verify -type f ./f*]
} -result 1

tcltest::test xvfs-glob-basic-limited-and-typed-prefixed-neg "Xvfs Glob Match Limited Path and Type Negative Test" -body {
	llength [glob_verify -type d ./f*]
} -result 0

tcltest::test xvfs-glob-basic-limited-prefixed-other-dir-1 "Xvfs Glob Match Directory Included in Search Test (Count)" -body {
	llength [glob_verify lib/*]
} -result 1

tcltest::test xvfs-glob-basic-limited-prefixed-other-dir-2 "Xvfs Glob Match Directory Included in Search Test (Value)" -body {
	lindex [glob_verify lib/*] 0
} -match glob -result "$rootDir/*"

tcltest::test xvfs-glob-no-dir "Xvfs Glob Non-Existant Directory Test" -body {
	glob_verify libx/*
} -returnCodes error -result "no such file or directory"

tcltest::test xvfs-glob-pipes "Xvfs Glob Pipes Test " -body {
	glob_verify -types {p b c s l} lib/*
} -result ""

tcltest::test xvfs-glob-writable "Xvfs Glob Writable Test " -body {
	glob -nocomplain -directory $rootDir -types w *
} -result ""

tcltest::test xvfs-glob-hidden "Xvfs Glob Hidden Test " -body {
	glob -nocomplain -directory $rootDir -types hidden *
} -result ""

tcltest::test xvfs-glob-executable "Xvfs Glob Executable Test " -body {
	glob -nocomplain -directory $rootDir -types x *
} -result $rootDir/lib

tcltest::test xvfs-access-basic-read "Xvfs acccess Read Basic Test" -body {
	file readable $testFile
} -match boolean -result true

tcltest::test xvfs-access-basic-write "Xvfs acccess Write Basic Test" -body {
	file writable $testFile
} -match boolean -result false

tcltest::test xvfs-access-basic-neg "Xvfs acccess Basic Negative Test" -body {
	file executable $testFile
} -match boolean -result false

tcltest::test xvfs-access-similar-neg "Xvfs acccess Similar Negative Test" -body {
	file executable ${rootDir}_DOES_NOT_EXIST
} -match boolean -result false

tcltest::test xvfs-exists-basic-neg "Xvfs exists Basic Negative Test" -body {
	file exists $rootDir/does-not-exist 
} -match boolean -result false

tcltest::test xvfs-stat-basic-file "Xvfs stat Basic File Test" -body {
	file stat $testFile fileInfo
	set fileInfo(type)
} -cleanup {
	unset -nocomplain fileInfo
} -result file

tcltest::test xvfs-stat-basic-file-neg "Xvfs stat Basic File Negative Test" -body {
	file stat $rootDir/does-not-exist fileInfo
} -cleanup {
	unset -nocomplain fileInfo
} -match glob -returnCodes error -result "*no such file or directory"

tcltest::test xvfs-stat-basic-dir "Xvfs stat Basic Directory Test" -body {
	file stat $rootDir/lib fileInfo
	set fileInfo(type)
} -cleanup {
	unset -nocomplain fileInfo
} -result directory

# Broken in Tcl 8.6 and earlier
tcltest::test xvfs-glob-advanced-dir-with-pattern "Xvfs Glob Match Pattern and Directory Together" -body {
	llength [glob ${rootDir}/*]
} -constraints tcl87 -result 3

tcltest::test xvfs-glob-file-dirname "Xvfs Relies on file dirname" -body {
	lindex [glob -directory [file dirname $testFile] *] 0
} -constraints tcl87 -match glob -result "$rootDir/*"

tcltest::test xvfs-cwd-1 "Xvfs Can Be cwd" -setup {
	set startDir [pwd]
} -body {
	cd $rootDir
	pwd
} -cleanup {
	cd $startDir
	unset startDir
} -constraints tcl87 -result $rootDir

tcltest::test xvfs-cwd-2 "Xvfs Can Be cwd" -setup {
	set startDir [pwd]
} -body {
	cd $rootDir
	cd lib
	lindex [glob *] 0
} -cleanup {
	cd $startDir
	unset startDir
} -constraints tcl87 -result "hello"

# Currently broken
tcltest::test xvfs-package "Xvfs Can Be Package Directory" -setup {
	set startAutoPath $auto_path
	lappend auto_path ${rootDir}/lib
} -body {
	package require hello
	set auto_path
} -cleanup {
	set auto_path $startAutoPath
	unset startAutoPath
} -constraints knownBug -result ""

# Output results
if {$::tcltest::numTests(Failed) != 0} {
	puts [test_summary]
	if {[info exists ::env(XVFS_TEST_EXIT_ON_FAILURE)]} {
		exit $::env(XVFS_TEST_EXIT_ON_FAILURE)
	}
	exit 1
}

puts [test_summary]
puts "ALL TESTS PASSED"

if {[info exists ::env(XVFS_TEST_EXIT_ON_SUCCESS)]} {
	exit $::env(XVFS_TEST_EXIT_ON_SUCCESS)
}
exit 0

#! /usr/bin/env tclsh

package require tcltest

tcltest::configure -verbose pbse
tcltest::configure {*}$argv

set rootDir "//xvfs:/example"
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

tcltest::customMatch boolean [list apply {{expected actual} {
	if {!!$expected == !!$actual} {
		return true
	} else {
		return false
	}
}}]

tcltest::testConstraint tcl87 [string match "8.7.*" [info patchlevel]]

tcltest::test xvfs-basic-seek "Xvfs Seek Test" -setup {
	set fd [open $testFile]
} -body {
	seek $fd 0 end
	seek $fd -1 current

	read $fd 1
} -cleanup {
	close $fd
	unset fd
} -result "\n"

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
	unset -nocomplain fd size done calls output
} -result {1 1}

tcltest::test xvfs-glob-basic-any "Xvfs Glob Match Any Test" -body {
	llength [glob_verify *]
} -result 3

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

# Broken in Tcl 8.6 and earlier
tcltest::test xvfs-glob-advanced-dir-with-pattern "Xvfs Glob Match Pattern and Directory Together" -body {
	llength [glob //xvfs:/example/*]
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
	set format "| %20s | %20s | %20s | %20s |"
	puts [string repeat - [string length [format $format - - - -]]]
	puts [format $format "Passed" "Failed" "Skipped" "Total"]
	puts [format $format \
		$::tcltest::numTests(Passed) \
		$::tcltest::numTests(Failed) \
		$::tcltest::numTests(Skipped) \
		$::tcltest::numTests(Total) \
	]
	puts [string repeat - [string length [format $format - - - -]]]

	exit 1
}

puts "ALL TESTS PASSED"

exit 0

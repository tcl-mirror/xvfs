set dir  "//xvfs:/example"
set dirNative  [file join [pwd] example]
set file "${dir}/foo"

set fd [open $file]
seek $fd 0 end
seek $fd -1 current
set check [read $fd 1]
if {$check != "\n"} {
	error "EXPECTED: (new line); GOT: [binary encode hex $check]"
}
close $fd

set fd1 [open $file]
set fd2 [open $file]
set data1 [read $fd1]
close $fd1
set data2 [read $fd2]
close $fd2
if {$data1 != $data2} {
	error "EXPECTED match, differs"
}

set fd [open $file]
seek $fd 0 end
set size [tell $fd]
close $fd
set fd [open $file]
set done false
set calls 0
set output ""
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
if {$calls != ($size + 1)} {
	error "EXPECTED [expr {$size + 1}], got $calls"
}
if {[lsort -integer $output] != $output} {
	error "EXPECTED [lsort -integer $output], GOT $output"
}
close $fd
update idle


proc glob_verify {args} {
	set rv [glob -nocomplain -directory $::dir {*}$args]
	set verify [glob -nocomplain -directory $::dirNative {*}$args]

	if {[llength $rv] != [llength $verify]} {
		error "VERIFY FAILED: glob ... $args ($rv versus $verify)"
	}

	return $rv
}

set check [glob_verify *]
if {[llength $check] < 2} {
	error "EXPECTED >=2, GOT [llength $check] ($check)"
}

set check [glob_verify f*]
if {[llength $check] != 1} {
	error "EXPECTED 1, GOT [llength $check] ($check)"
}

set check [glob_verify ./f*]
if {[llength $check] != 1} {
	error "EXPECTED 1, GOT [llength $check] ($check)"
}

set check [glob_verify -type f ./f*]
if {[llength $check] != 1} {
	error "EXPECTED 1, GOT [llength $check] ($check)"
}

set check [glob_verify -type d ./f*]
if {[llength $check] != 0} {
	error "EXPECTED 0, GOT [llength $check] ($check)"
}

set check [glob_verify x*]
if {[llength $check] != 0} {
	error "EXPECTED 0, GOT [llength $check] ($check)"
}

set check [glob_verify lib/*]
if {[llength $check] != 1} {
	error "EXPECTED 1, GOT [llength $check] ($check)"
}

set check [lindex $check 0]
if {![string match $dir/* $check]} {
	error "EXPECTED \"$dir/*\", GOT $check"
}

set check [glob_verify -type d *]
if {[llength $check] != 1} {
	error "EXPECTED 1, GOT [llength $check] ($check)"
}

set check [glob_verify -type d lib/*]
if {[llength $check] != 1} {
	error "EXPECTED 1, GOT [llength $check] ($check)"
}

cd $dir
cd lib
glob *


lappend auto_path ${dir}/lib
package require hello

puts "ALL TESTS PASSED"

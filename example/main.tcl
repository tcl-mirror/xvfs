set file "//xvfs:/example/foo"

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

puts "ALL TESTS PASSED"

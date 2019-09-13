set fd [open "//xvfs:/example/foo"]
seek $fd 0 end
seek $fd -1 current
set check [read $fd 1]
if {$check != "\n"} {
	error "EXPECTED: (new line); GOT: [binary encode hex $check]"
}

puts "ALL TESTS PASSED"

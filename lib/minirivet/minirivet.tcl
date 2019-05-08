#! /usr/bin/env tclsh

namespace eval ::minirivet {}

proc ::minirivet::parseStringToCode {string} {
	set fixMap [list]
	foreach char [list "\{" "\}" "\\"] {
		lappend fixMap $char "\}; puts -nonewline \"\\$char\"; puts -nonewline \{"
	}

	set code ""
	while {$string ne ""} {
		set endIndex [string first "<?" $string]
		if {$endIndex == -1} {
			set endIndex [expr {[string length $string] + 1}]
		}


		append code "puts -nonewline \{" [string map $fixMap [string range $string 0 $endIndex-1]] "\}; "
		set string [string range $string $endIndex end]
		set endIndex [string first "?>" $string]
		if {$endIndex == -1} {
			set endIndex [expr {[string length $string] + 1}]
		}

		set work [string range $string 0 2]
		if {$work eq "<?="} {
			set startIndex 3
			append code "puts -nonewline [string trim [string range $string 3 $endIndex-1]]; "
		} else {
			append code [string range $string 2 $endIndex-1] "\n"
		}

		set string [string range $string $endIndex+2 end]


	}

	return $code
}

proc ::minirivet::parseString {string} {
	set code [parseStringToCode $string]
	tailcall namespace eval ::request $code
}

proc ::minirivet::parse {file} {
	set fd [open $file]
	set data [read $fd]
	close $fd
	tailcall parseString $data
}

package provide minirivet 1

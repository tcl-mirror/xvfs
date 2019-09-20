#! /usr/bin/env tclsh

namespace eval ::minirivet {}

if {![info exists ::minirivet::_outputChannel] && ![info exists ::minirivet::_outputVariable]} {
	set ::minirivet::_outputChannel stdout
}

proc ::minirivet::setOutputChannel {channel} {
	unset -nocomplain ::minirivet::_outputVariable
	set ::minirivet::_outputChannel $channel
}

proc ::minirivet::setOutputVar {variable} {
	unset -nocomplain ::minirivet::_outputChannel
	set ::minirivet::_outputVariable $variable
}

proc ::minirivet::_emitOutput {string} {
	if {[info exists ::minirivet::_outputChannel]} {
		puts -nonewline $::minirivet::_outputChannel $string
	}
	if {[info exists ::minirivet::_outputVariable]} {
		append $::minirivet::_outputVariable $string
	}
	return
}

proc ::minirivet::parseStringToCode {string {outputCommand ""}} {
	if {$outputCommand eq ""} {
		set outputCommand [list ::minirivet::_emitOutput]
	}

	set code ""
	while {$string ne ""} {
		set endIndex [string first "<?" $string]
		if {$endIndex == -1} {
			set endIndex [expr {[string length $string] + 1}]
		}

		append code [list {*}$outputCommand [string range $string 0 $endIndex-1]] "; "
		set string [string range $string $endIndex end]
		set endIndex [string first "?>" $string]
		if {$endIndex == -1} {
			set endIndex [expr {[string length $string] + 1}]
		}

		set work [string range $string 0 2]
		if {$work eq "<?="} {
			set startIndex 3
			append code "$outputCommand [string trim [string range $string 3 $endIndex-1]]; "
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

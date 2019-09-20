#! /usr/bin/env tclsh

namespace eval ::xvfs {}

set ::xvfs::_xvfsDir [file dirname [info script]]

# Functions
proc ::xvfs::_emitLine {line} {
	if {[info command ::minirivet::_emitOutput] ne ""} {
		::minirivet::_emitOutput "${line}\n"
	} else {
		puts $line
	}
}

proc ::xvfs::printHelp {channel {errors ""}} {
	if {[llength $errors] != 0} {
		foreach error $errors {
			puts $channel "error: $error"
		}
		puts $channel ""
	}
	puts $channel "Usage: dir2c \[--help\] \[--output <filename>\] --directory <rootDirectory> --name <fsName>"
	flush $channel
}

proc ::xvfs::sanitizeCString {string} {
	set output [join [lmap char [split $string ""] {
		if {![regexp {[A-Za-z0-9./-]} $char]} {
			binary scan $char H* char
			set char "\\[format %03o 0x$char]"
		}

		set char
	}] ""]

	return $output
}

proc ::xvfs::sanitizeCStringList {list {prefix ""} {width 80}} {
	set lines [list]
	set row [list]
	foreach item $list {
		lappend row "\"[sanitizeCString $item]\""
		
		set rowString [join $row {, }]
		set rowString "${prefix}${rowString}"
		if {[string length $rowString] > $width} {
			set row [list]
			lappend lines $rowString
			unset rowString
		}
	}
	if {[info exists rowString]} {
		lappend lines $rowString
	}
	
	return [join $lines "\n"]
}

proc ::xvfs::binaryToCHex {binary {prefix ""} {width 10}} {
	set binary [binary encode hex $binary]
	set output [list]

	set width [expr {$width * 2}]
	set stopAt [expr {$width - 1}]

	set offset 0
	while 1 {
		set row [string range $binary $offset [expr {$offset + $stopAt}]]
		if {[string length $row] == 0} {
			break
		}
		incr offset [string length $row]

		set rowOutput [list]
		while {$row ne ""} {
			set value [string range $row 0 1]
			set row [string range $row 2 end]

			lappend rowOutput "\\x$value"
		}
		set rowOutput [join $rowOutput {}]
		set rowOutput "${prefix}\"${rowOutput}\""
		lappend output $rowOutput
	}

	if {[llength $output] == 0} {
		return "${prefix}\"\""
	}

	set output [join $output "\n"]
}

proc ::xvfs::processFile {fsName inputFile outputFile fileInfoDict} {
	array set fileInfo $fileInfoDict

	switch -exact -- $fileInfo(type) {
		"file" {
			set type "XVFS_FILE_TYPE_REG"
			set fd [open $inputFile]
			fconfigure $fd -encoding binary -translation binary -blocking true
			set data [read $fd]
			set size [string length $data]
			set data [string trimleft [binaryToCHex $data "\t\t\t"]]
			close $fd
		}
		"directory" {
			set type "XVFS_FILE_TYPE_DIR"
			set children $fileInfo(children)
			set size [llength $children]
			
			if {$size == 0} {
				set children "NULL"
			} else {
				set children [string trimleft [sanitizeCStringList $children "\t\t\t"]]
				# This initializes it using a C99 compound literal, C99 is required
				set children "(const char *\[\]) \{$children\}"
			}
		}
		default {
			return -code error "Unable to process $inputFile, unknown type: $fileInfo(type)"
		}
	}

	::xvfs::_emitLine "\t\{"
	::xvfs::_emitLine "\t\t.name = \"[sanitizeCString $outputFile]\","
	::xvfs::_emitLine "\t\t.type = $type,"
	::xvfs::_emitLine "\t\t.size = $size,"
	switch -exact -- $fileInfo(type) {
		"file" {
			::xvfs::_emitLine "\t\t.data.fileContents = (const unsigned char *) $data"
		}
		"directory" {
			::xvfs::_emitLine "\t\t.data.dirChildren  = $children"
		}
	}
	::xvfs::_emitLine "\t\},"
}

proc ::xvfs::processDirectory {fsName directory {subDirectory ""}} {
	set subDirectories [list]
	set outputFiles [list]
	set workingDirectory [file join $directory $subDirectory]
	set outputDirectory $subDirectory

	if {$subDirectory eq ""} {
		set isTopLevel true
	} else {
		set isTopLevel false
	}

	if {$isTopLevel} {
		::xvfs::_emitLine "static const struct xvfs_file_data xvfs_${fsName}_data\[\] = \{"
	}

	# XXX:TODO: Include hidden files ?
	set children [list]
	foreach file [glob -nocomplain -tails -directory $workingDirectory *] {
		if {$file in {. ..}} {
			continue
		}

		set inputFile [file join $workingDirectory $file]
		set outputFile [file join $outputDirectory [encoding convertto utf-8 $file]]

		unset -nocomplain fileInfo
		catch {
			file lstat $inputFile fileInfo
		}
		if {![info exists fileInfo]} {
			puts stderr "warning: Unable to access $inputFile, skipping"
		}
		
		lappend children [file tail $file]

		if {$fileInfo(type) eq "directory"} {
			lappend subDirectories $outputFile
			continue
		}

		processFile $fsName $inputFile $outputFile [array get fileInfo]
		lappend outputFiles $outputFile
	}

	foreach subDirectory $subDirectories {
		lappend outputFiles {*}[processDirectory $fsName $directory $subDirectory]
	}
	
	set inputFile $directory
	set outputFile $outputDirectory
	unset -nocomplain fileInfo
	file stat $inputFile fileInfo
	set fileInfo(children) $children

	processFile $fsName $inputFile $outputFile [array get fileInfo]
	lappend outputFiles $outputFile

	if {$isTopLevel} {
		::xvfs::_emitLine "\};"
	}

	return $outputFiles
}

proc ::xvfs::main {argv} {
	# Main entry point
	## 1. Parse arguments
	if {[llength $argv] % 2 != 0} {
		lappend argv ""
	}

	foreach {arg val} $argv {
		switch -exact -- $arg {
			"--help" {
				printHelp stdout
				exit 0
			}
			"--directory" {
				set rootDirectory $val
			}
			"--name" {
				set fsName $val
			}
			"--output" - "--header" {
				# Ignored, handled as part of some other process
			}
			default {
				printHelp stderr [list "Invalid option: $arg $val"]
				exit 1
			}
		}
	}

	## 2. Validate arguments
	set errors [list]
	if {![info exists rootDirectory]} {
		lappend errors "--directory must be specified"
	}
	if {![info exists fsName]} {
		lappend errors "--name must be specified"
	}

	if {[llength $errors] != 0} {
		printHelp stderr $errors
		exit 1
	}

	## 3. Start processing directory and producing initial output
	set ::xvfs::outputFiles [processDirectory $fsName $rootDirectory]

	set ::xvfs::fsName $fsName
	set ::xvfs::rootDirectory $rootDirectory
}

proc ::xvfs::run {argv} {
	uplevel #0 { package require minirivet }

	set ::xvfs::argv $argv
	::minirivet::parse [file join $::xvfs::_xvfsDir xvfs.c.rvt]
}

proc ::xvfs::setOutputChannel {channel} {
	uplevel #0 { package require minirivet }
	tailcall ::minirivet::setOutputChannel $channel
}

proc ::xvfs::setOutputVariable {variable} {
	uplevel #0 { package require minirivet }
	tailcall ::minirivet::setOutputVariable $variable
}

proc ::xvfs::staticIncludeHeaderData {headerData} {
	set ::xvfs::xvfsCoreH $headerData
}

proc ::xvfs::staticIncludeHeader {pathToHeaderFile} {
	set fd [open $pathToHeaderFile]
	::xvfs::staticIncludeHeaderData [read $fd]
	close $fd
}

package provide xvfs 1

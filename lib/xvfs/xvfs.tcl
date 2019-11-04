#! /usr/bin/env tclsh

namespace eval ::xvfs {}
namespace eval ::xvfs::callback {}

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
			lappend lines "${rowString},"
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
			if {[info exists fileInfo(fileContents)]} {
				set data $fileInfo(fileContents)
			} else {
				set fd [open $inputFile]
				fconfigure $fd -encoding binary -translation binary -blocking true
				set data [read $fd]
				close $fd
			}
			set size [string length $data]
			set data [string trimleft [binaryToCHex $data "\t\t\t"]]
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
		set subDirectoryName [file join $outputDirectory $file]

		if {[info command ::xvfs::callback::setOutputFileName] ne ""} {
			set outputFile [::xvfs::callback::setOutputFileName $file $workingDirectory $inputFile $outputDirectory $outputFile]
			if {$outputFile eq "/"} {
				continue
			}
		}

		unset -nocomplain fileInfo
		catch {
			file lstat $inputFile fileInfo
		}
		if {![info exists fileInfo]} {
			puts stderr "warning: Unable to access $inputFile, skipping"
		}

		if {$fileInfo(type) eq "directory"} {
			lappend subDirectories $subDirectoryName
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
	if {[info command ::xvfs::callback::setOutputFileName] ne ""} {
		set outputFile [::xvfs::callback::setOutputFileName $directory $directory $inputFile $outputDirectory $outputFile]
	}

	if {$outputFile ne "/"} {
		unset -nocomplain fileInfo
		file stat $inputFile fileInfo
		set children [list]
		set outputFileLen [string length $outputFile]
		foreach child $outputFiles {
			if {[string range /$child 0 $outputFileLen] eq "/${outputFile}"} {
				set child [string trimleft [string range $child $outputFileLen end] /]
				if {![string match "*/*" $child]} {
					lappend children $child
				}
			}
		}
		set fileInfo(children) $children

		processFile $fsName $inputFile $outputFile [array get fileInfo]
		lappend outputFiles $outputFile
	}

	if {$isTopLevel} {
		if {[info command ::xvfs::callback::addOutputFiles] ne ""} {
			lappend outputFiles {*}[::xvfs::callback::addOutputFiles $fsName]
		}

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

proc ::xvfs::run {args} {
	uplevel #0 { package require minirivet }

	set ::xvfs::argv $args
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

proc ::xvfs::_tryFit {list} {
	set idx -1
	set lastItem -100000
	foreach item $list {
		incr idx

		if {$item <= $lastItem} {
			return ""
		}

		set difference [expr {$item - $idx}]
		if {$idx != 0} {
			set divisor [expr {$item / $idx}]
		} else {
			set divisor 1
		}
		lappend differences $difference
		lappend divisors $divisor

		set lastItem $item
	}

	foreach divisor [lrange $divisors 1 end] {
		incr divisorCount
		incr divisorValue $divisor
	}
	set divisor [expr {$divisorValue / $divisorCount}]

	for {set i 0} {$i < [llength $list]} {incr i} {
		lappend outList $i
	}

	set mapFunc " - ${difference}"

	set newList [lmap v $list { expr "\$v${mapFunc}" }]
	if {$newList eq $outList} {
		return $mapFunc
	}

	if {$divisor != 1} {
		set mapFunc " / ${divisor}"
		set newList [lmap v $list { expr "\$v${mapFunc}" }]
		if {$newList eq $outList} {
			return $mapFunc
		}

		set subMapFunc [_tryFit $newList]
		if {$subMapFunc != ""} {
			return " / ${divisor}${subMapFunc}"
		}
	}

	return ""
}

proc ::xvfs::generatePerfectHashFunctionCall {cVarName cVarLength invalidValue nameList args} {
	# Manage config
	## Default config
	array set config {
		useCacheFirst  false
		cacheValue     true
		enableCache    false
	}
	set config(cacheFile) [file join [file normalize ~/.cache] xvfs phf-cache.db]

	## User config
	foreach {configKey configVal} $args {
		if {![info exists config($configKey)]} {
			error "Invalid option: $configKey"
		}
	}
	array set config $args

	if {$config(enableCache)} {
		package require sqlite3
	}

	# Adjustment for computing the expense of a function call by its length
	# Calls that take longer should be made longer, so make CRC32 longer
	# than Adler32
	set lengthAdjustment [list Tcl_ZlibCRC32 Tcl_CRCxxx32]

	# Check for a cached entry
	if {$config(enableCache) && $config(useCacheFirst)} {
		catch {
			set hashKey $nameList

			sqlite3 ::xvfs::phfCache $config(cacheFile)
			::xvfs::phfCache eval {CREATE TABLE IF NOT EXISTS cache(hashKey PRIMARY KEY, function BLOB);}
			::xvfs::phfCache eval {SELECT function FROM cache WHERE hashKey = $hashKey LIMIT 1;} cacheRow {}
		}
		catch {
			::xvfs::phfCache close
		}

		if {[info exists cacheRow(function)]} {
			set phfCall $cacheRow(function)
			set phfCall [string map [list @@CVARNAME@@ $cVarName @@CVARLENGTH@@ $cVarLength @@INVALIDVALUE@@ $invalidValue] $phfCall]

			return $phfCall
		}
	}

	set minVal 0
	set maxVal [llength $nameList]
	set testExpr_(0) {[zlib adler32 $nameItem $alpha] % $gamma}
	set testExpr(1) {[zlib crc32 $nameItem $alpha] % $gamma}
	set testExpr_(2) {[zlib adler32 $nameItem [zlib crc32 $nameItem $alpha]] % $gamma}
	set testExpr_(3) {[zlib crc32 $nameItem [zlib adler32 $nameItem $alpha]] % $gamma}
	set testExprC(0) {((Tcl_ZlibAdler32(${alpha}LU, (unsigned char *) @@CVARNAME@@, @@CVARLENGTH@@) % ${gamma}LU)${fitMod})}
	set testExprC(1) {((Tcl_ZlibCRC32(${alpha}LU, (unsigned char *) @@CVARNAME@@, @@CVARLENGTH@@) % ${gamma}LU)${fitMod})}
	set testExprC(2) {((Tcl_ZlibAdler32(Tcl_ZlibCRC32(${alpha}LU, (unsigned char *) @@CVARNAME@@, @@CVARLENGTH@@), (unsigned char *) @@CVARNAME@@, @@CVARLENGTH@@) % ${gamma}LU)${fitMod})}
	set testExprC(3) {((Tcl_ZlibCRC32(Tcl_ZlibAdler32(${alpha}LU, (unsigned char *) @@CVARNAME@@, @@CVARLENGTH@@), (unsigned char *) @@CVARNAME@@, @@CVARLENGTH@@) % ${gamma}LU)${fitMod})}

	# Short-circuit for known cases
	if {$maxVal == 1} {
		return 0
	}

	set round -1

	while true {
		incr round

		set gamma [expr {$maxVal + ($round % ($maxVal * 128))}]
		set alpha [expr {$round / 6}]

		foreach {testExprID testExprContents} [array get testExpr] {
			set unFitList [list]
			foreach nameItem $nameList {
				set testExprVal [expr $testExprContents]
				lappend unFitList $testExprVal
			}

			set failed false
			set fitMod [_tryFit $unFitList]
			if {$fitMod eq ""} {
				set failed true
			}

			if {!$failed} {
				break
			}
		}

		if {!$failed} {
			break
		}

	}

	set phfCall [string map [list { - 0LU} ""] [subst $testExprC($testExprID)]]

	# Check cache for a better answer
	if {$config(enableCache)} {
		catch {
			set hashKey $nameList
			set cacheDir [file dirname $config(cacheFile)]
			file mkdir $cacheDir

			unset -nocomplain cacheRow

			sqlite3 ::xvfs::phfCache $config(cacheFile)
			::xvfs::phfCache eval {CREATE TABLE IF NOT EXISTS cache(hashKey PRIMARY KEY, function BLOB);}
			::xvfs::phfCache eval {SELECT function FROM cache WHERE hashKey = $hashKey LIMIT 1;} cacheRow {}

			set updateCache false
			if {[info exists cacheRow(function)]} {
				if {[string length [string map $lengthAdjustment $cacheRow(function)]] <= [string length [string map $lengthAdjustment $phfCall]]} {
					# Use the cached value since it is better
					set phfCall $cacheRow(function)
				} else {
					set updateCache true
				}
			} else {
				set updateCache true
			}

			if {$updateCache && $config(cacheValue)} {
				# Save to cache
				::xvfs::phfCache eval {INSERT OR REPLACE INTO cache (hashKey, function) VALUES ($hashKey, $phfCall);}
			}
		}

		catch {
			::xvfs::phfCache close
		}
	}

	set phfCall [string map [list @@CVARNAME@@ $cVarName @@CVARLENGTH@@ $cVarLength @@INVALIDVALUE@@ $invalidValue] $phfCall]

	return $phfCall
}

proc ::xvfs::generateHashTable {outCVarName cVarName cVarLength invalidValue nameList args} {
	# Manage config
	## Default config
	array set config {
		prefix        ""
		hashTableSize 10
		validate      0
		onValidated   "break;"
	}

	## User config
	foreach {configKey configVal} $args {
		if {![info exists config($configKey)]} {
			error "Invalid option: $configKey"
		}
	}
	array set config $args

	if {[llength $nameList] < $config(hashTableSize)} {
		set config(hashTableSize) [llength $nameList]
	}

	set maxLength 0
	set index -1
	foreach name $nameList {
		incr index
		set length [string length $name]
		set hash [expr {[zlib adler32 $name 0] % $config(hashTableSize)}]

		lappend indexesAtLength($length) $index
		lappend indexesAtHash($hash) $index

		if {$length > $maxLength} {
			set maxLength $length
		}
	}

	set maxIndexes 0
	foreach {hash indexes} [array get indexesAtHash] {
		set indexesCount [llength $indexes]

		if {$indexesCount > $maxIndexes} {
			set maxIndexes $indexesCount
		}
	}

	lappend outputHeader "${config(prefix)}long ${outCVarName}_idx;"
	lappend outputHeader "${config(prefix)}int ${outCVarName}_hash;"

	for {set hash 0} {$hash < $config(hashTableSize)} {incr hash} {
		if {[info exists indexesAtHash($hash)]} {
			set indexes $indexesAtHash($hash)
		} else {
			set indexes [list]
		}

		lappend indexes $invalidValue
		lappend outputHeader "${config(prefix)}static const long ${outCVarName}_hashTable_${hash}\[\] = \{"
		lappend outputHeader "${config(prefix)}\t[join $indexes {, }]"
		lappend outputHeader "${config(prefix)}\};"
	}

	lappend outputHeader "${config(prefix)}static const long * const ${outCVarName}_hashTable\[${config(hashTableSize)}\] = \{"

	for {set hash 0} {$hash < $config(hashTableSize)} {incr hash} {
		lappend outputHeader "${config(prefix)}\t${outCVarName}_hashTable_${hash},"
	}

	lappend outputHeader "${config(prefix)}\};"
	lappend outputBody "${config(prefix)}${outCVarName}_hash = Tcl_ZlibAdler32(0, (unsigned char *) ${cVarName}, ${cVarLength}) % ${config(hashTableSize)};"
	lappend outputBody "${config(prefix)}for (${outCVarName}_idx = 0; ${outCVarName}_idx <= ${maxIndexes}; ${outCVarName}_idx++) \{"
	lappend outputBody "${config(prefix)}\t${outCVarName} = ${outCVarName}_hashTable\[${outCVarName}_hash\]\[${outCVarName}_idx\];"
	lappend outputBody "${config(prefix)}\tif (${outCVarName} == $invalidValue) \{"
	lappend outputBody "${config(prefix)}\t\tbreak;"
	lappend outputBody "${config(prefix)}\t\}"
	lappend outputBody ""
	lappend outputBody "${config(prefix)}\tif (${config(validate)}) \{"
	lappend outputBody "${config(prefix)}\t\t${config(onValidated)}"
	lappend outputBody "${config(prefix)}\t\}"
	lappend outputBody "${config(prefix)}\}"

	return [dict create header [join $outputHeader "\n"] body [join $outputBody "\n"]]
}

package provide xvfs 1

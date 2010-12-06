-- Copyright (C)2010 D. R. Commander
-- Copyright (C)2009 Sun Microsystems, Inc.
--
-- This library is free software and may be redistributed and/or modified under
-- the terms of the wxWindows Library License, Version 3.1 or (at your option)
-- any later version.  The full license is in the LICENSE.txt file included
-- with this distribution.
--
-- This library is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- wxWindows Library License for more details.

set disk to (path to startup disk) as string
set tmpfile to path to temporary items from user domain
set tmpfile to POSIX path of tmpfile & "libjpeg-turbo_uninstall.log"
set uninstaller to disk & "opt:libjpeg-turbo:bin:uninstall"
set uninstaller to "sh " & POSIX path of uninstaller & " 2>&1 >" & tmpfile
set success to 0

display dialog "You are about to uninstall libjpeg-turbo.  Proceed?" buttons {"Yes", "No"} default button "No"

if button returned of result is "Yes" then
	try
		do shell script (uninstaller) with administrator privileges
		set success to 1
	on error errstr number errnum
		if errnum is -128 then
			display dialog "Uninstall aborted." buttons {"OK"}
		else if errnum is 255 then
			set errmsg to "The uninstall script could not remove some of the files or directories installed by the libjpeg-turbo package.  Consult:" & return & return & tmpfile & return & return & "for more details."
			display dialog errmsg buttons {"OK"} default button "OK" with icon caution
		else if errnum is 127 then
			display dialog "Could not find the libjpeg-turbo uninstall script.  The libjpeg-turbo package may have already been uninstalled." buttons {"OK"} default button "OK" with icon stop
		else
			set errmsg to "ERROR " & errnum & ": " & errstr
			display dialog errmsg buttons {"OK"} default button "OK" with icon stop
		end if
	end try
	if success is 1 then
		display dialog "libjpeg-turbo has been successfully uninstalled." buttons {"OK"}
	end if
else
	display dialog "Uninstall aborted." buttons {"OK"}
end if

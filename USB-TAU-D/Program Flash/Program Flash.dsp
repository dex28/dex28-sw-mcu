# Microsoft Developer Studio Project File - Name="Program Flash" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=Program Flash - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "Program Flash.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "Program Flash.mak" CFG="Program Flash - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Program Flash - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "Program Flash - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "Program Flash - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f "Program Flash.mak""
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "Program Flash.exe"
# PROP BASE Bsc_Name "Program Flash.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "NMAKE /f "Program Flash.mak""
# PROP Rebuild_Opt "/a"
# PROP Target_File "Program Flash.exe"
# PROP Bsc_Name "Program Flash.bsc"
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "Program Flash - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f "Program Flash.mak""
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "Program Flash.exe"
# PROP BASE Bsc_Name "Program Flash.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line ""C:\Program Files\Atmel\AVR Tools\STK500\STK500" -s -dATmega128 -ccom1 -ms -e -pf -if..\USB-TAU-D.hex "
# PROP Rebuild_Opt "clean"
# PROP Target_File "Program Flash.exe"
# PROP Bsc_Name "Program Flash.bsc"
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "Program Flash - Win32 Release"
# Name "Program Flash - Win32 Debug"

!IF  "$(CFG)" == "Program Flash - Win32 Release"

!ELSEIF  "$(CFG)" == "Program Flash - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project

# Microsoft Developer Studio Project File - Name="USBPIO" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) External Target" 0x0106

CFG=USBPIO - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "USBPIO.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "USBPIO.mak" CFG="USBPIO - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "USBPIO - Win32 Release" (based on "Win32 (x86) External Target")
!MESSAGE "USBPIO - Win32 Debug" (based on "Win32 (x86) External Target")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "USBPIO - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Cmd_Line "NMAKE /f USBPIO.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "USBPIO.exe"
# PROP BASE Bsc_Name "USBPIO.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Cmd_Line "make"
# PROP Rebuild_Opt ""
# PROP Target_File "usbpio.hex"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ELSEIF  "$(CFG)" == "USBPIO - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Cmd_Line "NMAKE /f USBPIO.mak"
# PROP BASE Rebuild_Opt "/a"
# PROP BASE Target_File "USBPIO.exe"
# PROP BASE Bsc_Name "USBPIO.bsc"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Cmd_Line "make"
# PROP Rebuild_Opt ""
# PROP Target_File "usbpio.hex"
# PROP Bsc_Name ""
# PROP Target_Dir ""

!ENDIF 

# Begin Target

# Name "USBPIO - Win32 Release"
# Name "USBPIO - Win32 Debug"

!IF  "$(CFG)" == "USBPIO - Win32 Release"

!ELSEIF  "$(CFG)" == "USBPIO - Win32 Debug"

!ENDIF 

# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\usbpio.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\xhfc.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "zap_xhfc"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\zap_xhfc\xhfc24succ.h
# End Source File
# Begin Source File

SOURCE=.\zap_xhfc\xhfc_pci2pi.c
# End Source File
# Begin Source File

SOURCE=.\zap_xhfc\xhfc_pci2pi.h
# End Source File
# Begin Source File

SOURCE=.\zap_xhfc\zap_xhfc_su.c
# End Source File
# Begin Source File

SOURCE=.\zap_xhfc\zap_xhfc_su.h
# End Source File
# End Group
# Begin Group "mISDN"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\mISDN\xhfc24succ.h
# End Source File
# Begin Source File

SOURCE=.\mISDN\xhfc_pci2pi.c
# End Source File
# Begin Source File

SOURCE=.\mISDN\xhfc_pci2pi.h
# End Source File
# Begin Source File

SOURCE=.\mISDN\xhfc_su.c
# End Source File
# Begin Source File

SOURCE=.\mISDN\xhfc_su.h
# End Source File
# End Group
# Begin Source File

SOURCE=.\Makefile
# End Source File
# Begin Source File

SOURCE=.\prog.bat
# End Source File
# Begin Source File

SOURCE=.\script.ld
# End Source File
# Begin Source File

SOURCE=.\usbpio.lst
# End Source File
# Begin Source File

SOURCE=.\usbpio.map
# End Source File
# End Target
# End Project

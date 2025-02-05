
Verpatch - a tool to patch win32 version resources on .exe or .dll files,

Version: 1.0.9 (21-Nov-2011)

Verpatch is a command line tool for adding and editing the version information
of Windows executable files (applications, DLLs, kernel drivers)
without rebuilding the executable.

It can also add or replace Win32 (native) resources, and do some other
modifications of executable files.

Verpatch sets ERRORLEVEL 0 on success, otherwise errorlevel is non-zero.
Verpatch modifies files in place, so please make copies of precious files.


Command line syntax
===================

verpatch filename [version] [/options]

Where
 - filename : any Windows PE file (exe, dll, sys, ocx...) that can have native resources
 - version : one to four decimal numbers, separated by dots, ex.: 1.2.3.4
   Additional text can follow the numbers; see examples below. Ex.: "1.2.3.4 extra text"

Common Options:

/va - creates a version resource. Use when the file has no version resource at all,
     or existing version resource should be replaced.
     If this option not specified, verpatch will read version resourse from the file.
/s name "value" - add a version resource string attribute
     The name can be either a full attribute name or alias; see below.
/sc "comment" - add or replace Comments string (shortcut for /s Comments "comment")
/pv <version>   - specify Product version
    where <version> arg has same form as the file version (1.2.3.4 or "1.2.3.4 text")
/fn - preserves Original filename, Internal name in the existing version resource of the file.


Other options:

/langid <number> - language id for new version resource.
     Use with /va. Default is language neutral.
     <number> is combination of primary and sublanguage IDs. ENU is 1033 or 0x409.
/vo - outputs the version info in RC format to stdout.
     This can be used with /xi to dump a version resource without modification.
     Output of /vo can be pasted to a .rc file and compiled with rc.
/xi- test mode. does all operations in memory but does not modify the file
/xlb - test mode. Re-parses the version resource after modification.
/rpdb - removes path to the .pdb file in debug information; leaves only file name.
/rf #id file - add or replace a raw binary resource from file (see below)
/noed - do not check for extra data appended to exe file
/vft2 num - specify driver subtype (VFT2_xxx value, see winver.h)
     The application type (VFT_xxx) is retained from the existing version resource of the file,
     or filled automatically, based on the filename extension (exe->app, sys->driver, anything else->dll)


Examples
========

verpatch d:\foo.dll 1.2.33.44
	- replaces only the file version, all 4 numbers, 
	the Original file name and Internal name strings are set to "foo.dll".
        File foo.dll should already have a version resource (since /va not specified)

verpatch d:\foo.dll 33.44  /s comment "a comment"
	- replaces only two last numbers of the file version and adds a comment.
        File foo.dll should already have a version resource.

verpatch d:\foo.dll "33.44 special release" /pv 1.2.3.4
	- same as previous, with additional text in the version argument.
        - Product version is also specified

verpatch d:\foo.dll "1.2.33.44" /va /s description "foo.dll"
     /s company "My Company" /s copyright "(c) 2009"
   - creates or replaces version resource to foo.dll, with several string values.
     ( all options should be one line)

verpatch d:\foo.dll /vo /xi
	- dumps the version resource in RC format, does not update the file.


	
Remarks
=======

Verpatch replaces the version number in existing file version info resource
with the values given on the command line.

In "patch" mode (no /va option), the PE file should have a version resource,
which is parsed, and then parameters specified on the command line are applied.

If the file has no version resource, or you want to discard the existing resource, use /va switch.
All nesessary strings can be specified with the /s option.

Quotes surrounding arguments are needed for the command shell (cmd.exe), 
for any argument that contains spaces.
Also other characters should be escaped (ex. &, |, and ^ for cmd.exe).
Null values can be specified as empty string ("").

The command line can become very long, so you may want to use a batch file or script.
See the example batch files.

Verpatch can be run on same PE file any number of times.

The Version argument can be specified as 1 to 4 dot separated decimal numbers.
Addtional suffix can follow the version numbers, separated by a dash (-) or space.
If the separator is space, the whole version argument must be enclosed in quotes.

If less than 4 numbers are given, they are considered as minor numbers.
The higher version parts are retained from existing version resource.
For example, if the existing version info has version number 1.2.3.4
and 55.66 specified on the command line, the result will be 1.2.55.66.

The program ensures that the version numbers in the binary part
of the version structure and in the string part (as text) are same.

By default, Original File Name and Internal File Name are replaced to the actual filename.
Use /fn to preserve existing values in the version resource.

String attribute names for option /s must be language-neutral, 
not translations (example: PrivateBuild, not "Private Build Description").
See below for the list of known attrbute names and their aliases.
The examples above use the aliases.

String arguments for File version and Product version parameters are handled
 in a special way, the /s switch should not be used to set these:
 - The File version can be specified as the 2nd positional argument only
 - The Product version can be specified using /pv switch


Misc. functions
================

The /rf switch adds a resource from a file, or replaces a resource with same type and id.

The argument "#id" is a 32-bit hex number, prefixed with #.
Low 16 bits of this value are resource id; can not be 0.
Next 8 bits are resource type: one of RT_xxx symbols in winuser.h, or user defined.
If the type value is 0, RT_RCDATA (10) is assumed.
High 8 bits of the #id arg are reserved0.
The language code of resources added by this switch is 0 (Neutral).
Named resource types and ids are not implemented.
The file is added as opaque binary chunk; the resource size is rounded up to 4 bytes
and padded with zero bytes.

The program detects extra data appended to executable files, saves it and appends 
again after modifying resources.
Command switch /noed disables checking for extra data.

Such extra data is used by some installers, self-extracting archives and other applications.
However, the way we restore the data may be not compatible with these applications.
Please, verify that executable files that contain extra data work correctly after modification.
Make backup of valuable files before modification.


====================================================================
Known string keys in VS_VERSION_INFO resource
====================================================================

The aliases in the right column can be used with /s switch,
in place of language-neutral (LN) attribute names. 
Attribute names are not case sensitive.

-------------------+----+-------------------------------+------------
 Lang.Neutral name |note| English translation           | Aliases
-------------------+----+-------------------------------+------------
Comments                    Comments                      comment
CompanyName           E     Company                       company
FileDescription       E     Description                   description, desc
FileVersion           *1    File Version
InternalName                Internal Name                 title
                      *2    Language
LegalCopyright        E     Copyright                     copyright, (c)
LegalTrademarks       E     Legal Trademarks              tm, (tm)
OriginalFilename            Original File Name
ProductName                 Product Name                  product
ProductVersion        *1    Product Version               pv, productversion, productver, prodver
PrivateBuild                Private Build Description     pb, private
SpecialBuild                Special Build  Description    sb, build
OleSelfRegister       A     - 
AssemblyVersion       N

Notes
*1: FileVersion, ProductVersion values should begin with same 1.2.3.4 version number as in the binary header.
Can be any text. Windows Explorer displays the version numbers from the binary header.
Explorer of WinXP also displays File Version text in the strings box.

*2: The "Language" value is the name of the language code specified in the header of the string block of VS_VERSION_INFO resource.
(or taken from VarFileInfo block?)
It is displayed by Windows Explorer, but is not contained in the version data.

E: Displayed by Windows Explorer in Vista+
A: Intended for some API (OleSelfRegister is used in COM object registration)
N: Added by some .NET compilers. This version number is not contained in the
    binary part of the version struct and can differ from the file version.
    To change it, just use switch /s AssemblyVersion [value]

====================================================================



Known issues and TO DO's:
=========================

 - Does not work on old PE files that have link version 5.x (before VC6?)
   No known workaround; this seems to be limitation of Windows UpdateResource API.

 - Import of version resource does not work if encoded not in UTF-16.

 - Does not work on files signed with digital certificates (TO DO)

 -  A second version resource may be added to a file
   that already has a version resource in other language. Switch /va won't help.
   TO DO: ensure that a file has only one version resource!
   
 - When verpatch is invoked from command prompt, or batch file, the string
   arguments can contain only ANSI characters, because cmd.exe batch files cannot be 
   in Uncode format. If you need to include characters not in current locale,
   use other shell languages that fully support Unicode (PowerShell, vbs, js).
   
 - TO DO: In RC source output (/vo), special characters in strings are not quoted;
   so /vo may produce invalid RC input
   
 - The parser of binary version resources handles only the most common type of structure.
   If the parser breaks because of unhandled structure format, try /va switch to
   skip reading existing version resource and re-create it from scratch.
   Consider using WINE or other open source implementations?
   
 - option to add extra 0 after version strings : "string\0"
   (tentative, requiested by a reader for some old VB code) 

 - For files with extra data, checksum is not re-calculated. Revise.


Source code 
============
The source is provided as a Visual C++ 2005 project, it can be compiled with VC 2005, 2008, 2010 Express.
It demonstrates use of the UpdateResource and imagehlp.dll API.
It does not demonstrate use of c++, good coding manners or anything else.
Dependencies on C++ libraries available only with the full Visual C 2008 have been removed.


LICENSE TERMS: CPOL (CodeProject Open License)
http://www.codeproject.com/info/licenses.aspx

~~

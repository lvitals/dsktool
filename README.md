# DSKTOOL v1.4

**By Ricardo Bittencourt**  
_Updated by Tony Cruise (2010)_  
_Modernized by Leandro V. Catarin (2025)_

- - -

## Index

1.  Introduction
2.  Syntax
3.  Examples
4.  Implemented Suggestions
5.  What's New
6.  Acknowledgments
7.  The Author
    
- - -

## 1\. Introduction

Hello, MSX community!

In 2025, with the goal of preserving the legacy of this essential tool, I set out to modernize **DSKTOOL** for retro computing enthusiasts. The **.DSK** format remains crucial for MSX emulation and preservation, but the tools needed an update.

Among the new features, advanced **boot sector** operations allow deeper disk management—perfect for homebrew developers and preservationists. The command syntax has been modernized for intuitive use while keeping the original commands available for compatibility.

- - -

## 2\. Syntax

dsktool <command> <dsk\_file> \[files\] \[options\]

### Commands

| Command | Description |
| --- | --- |
| `ls` | List disk contents |
| `x` | Extract files |
| `rm` | Delete files |
| `a` | Add files |

### Boot Sector Operations

|     |     |
| --- | --- |
| Command | Description |
| `sx` | Save boot sector to a binary file |
| `sw` | Write boot sector from a binary file |
| `s` | Initialize boot sector (_0 = zeros, 1 = MSX-DOS default_) |

📌 **Note:**

*   The `[files]` parameter supports wildcards (`*` and `?`).

*   For `a`, use `--boot-label=LABEL` to set the 11-character volume label in the boot sector.
*   For `a`, use `--hidden-system-file=NAME` for each file that must be marked as `Hidden+System`.
    
*   For boot operations, specify either a filename (`sx/sw`) or an initialization mode (`s`).
    

- - -

## 3\. Examples

### 3.1. List contents with the modern command

dsktool ls GAMEPACK.DSK

### 3.2. Extract all `.COM` files while preserving case

dsktool x UTILS.DSK \*.COM

### 3.3. Advanced disk surgery - Replace boot sector

dsktool sw SYSTEM.DSK CUSTOM.BIN

dsktool a SYSTEM.DSK NEWKERNEL.SYS

### 3.4. Create a new disk with a custom boot

dsktool s BLANK.DSK 0

dsktool sw BLANK.DSK MYBOOT.BIN

dsktool a BLANK.DSK MYPROG\*.\*

### 3.5. Forensic recovery - Dump boot sector

dsktool sx MYSTERY.DSK BOOTSAVE.BIN

- - -

## 4\. Implemented Suggestions

New suggestions are always welcome! The MSX scene continues to evolve, and our tools should too.

- - -

## 5\. What's New


### **\[1.4\] 2026 - Leandro V. Catarin**

✅ Added wildcard support in `[files]` parameter (`*` and `?`).  
✅ Added `--boot-label=LABEL` to define the 11-character volume label in the boot sector.  
✅ Added `--hidden-system-file=NAME` to mark files as `Hidden+System`.  

### **\[1.3\] 2025 - Leandro V. Catarin**

✅ Added boot sector operations (`save`, `restore`, `initialize`).   
✅ Modernized command set (_ls/x/rm_) while keeping original commands.  
✅ Improved error handling and user feedback.  
✅ Preparations for future disk format support.  
✅ Refactored codebase for multi-platform support.

### **\[1.2\] 2010 - Tony Cruise**

✅ Fixed memory leak in `add_single_file`.

### **\[1.1\]**

✅ Fixed bug with files larger than **64 KB**.

- - -

## 6\. Acknowledgments

Standing on the shoulders of giants, I acknowledge:

🖥️ **Ricardo Bittencourt** and **Tony Cruise**, original creators of dsktool.  
📖 **Eduardo Barbosa** and **Edison Moraes**, authors of the "MSX Bible".  
🔧 **DJ Delorie** and **Charles Sandmann**, DJGPP pioneers.  
🆓 **Richard Stallman**, free software champion.  
🌍 **MSX.org community**, keeping our passion alive.  
🇧🇷 **Brazilian MSX community**, for keeping the retro flame burning!

- - -

## 7\. The Author

**Leandro V. Catarin**  
💾 MSX enthusiast since childhood, now giving back to the community that shaped my computing journey.  
📧 Contact: _\[__leandro.vital@yahoo.com.br__\]_

- - -

🚀 **Keep the MSX spirit alive!**

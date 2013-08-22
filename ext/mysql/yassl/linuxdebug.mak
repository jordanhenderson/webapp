#Generated by VisualGDB (http://visualgdb.com)
#DO NOT EDIT THIS FILE MANUALLY UNLESS YOU ABSOLUTELY NEED TO
#USE VISUALGDB PROJECT PROPERTIES DIALOG INSTEAD

BINARYDIR := LinuxDebug

#Toolchain
CC := gcc
CXX := g++
LD := $(CXX)
AR := ar
OBJCOPY := objcopy

#Additional flags
PREPROCESSOR_MACROS :=
INCLUDE_DIRS :=../include/ ./include/ ../taocrypt/include/ ../taocrypt/mySTL/
LIBRARY_DIRS :=
LIBRARY_NAMES :=
ADDITIONAL_LINKER_INPUTS :=
MACOS_FRAMEWORKS :=

CFLAGS := -ggdb -ffunction-sections -DHAVE_CONFIG_H -Dget_tty_password=yassl_mysql_get_tty_password -Dget_tty_password_ext=yassl_mysql_get_tty_password_ext
CXXFLAGS := -ggdb -ffunction-sections -DHAVE_YASSL -DYASSL_PREFIX -DHAVE_OPENSSL -DMULTI_THREADED -DHAVE_CONFIG_H -Dget_tty_password=yassl_mysql_get_tty_password -Dget_tty_password_ext=yassl_mysql_get_tty_password_ext
ASFLAGS :=
LDFLAGS := -Wl,-gc-sections
COMMONFLAGS := 

START_GROUP := -Wl,--start-group
END_GROUP := -Wl,--end-group

#Additional options detected from testing the toolchain
IS_LINUX_PROJECT := 1
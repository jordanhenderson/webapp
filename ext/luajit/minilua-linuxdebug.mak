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
INCLUDE_DIRS := 
LIBRARY_DIRS := 
LIBRARY_NAMES := m
ADDITIONAL_LINKER_INPUTS := 
MACOS_FRAMEWORKS := 

CFLAGS := -ggdb -ffunction-sections 
CXXFLAGS := -ggdb -ffunction-sections 
ASFLAGS := 
LDFLAGS := -Wl,-gc-sections
COMMONFLAGS := 

START_GROUP := -Wl,--start-group
END_GROUP := -Wl,--end-group

#Additional options detected from testing the toolchain
IS_LINUX_PROJECT := 1
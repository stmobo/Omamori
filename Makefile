#
# Source directories and stuff:
MAIN_SRC	:= ./src
ARCH_SRC	:= $(MAIN_SRC)/arch/x86
BOOT_SRC	:= $(MAIN_SRC)/boot/x86
CORE_SRC	:= $(MAIN_SRC)/core
DEVICE_SRC	:= $(MAIN_SRC)/device
LIB_SRC		:= $(MAIN_SRC)/lib
INCLUDE_DIR := $(MAIN_SRC)/include

SRC_DIRS := $(ARCH_SRC) $(BOOT_SRC) $(CORE_SRC) $(DEVICE_SRC) $(LIB_SRC)
MAIN_OBJ  := ./obj
ARCH_OBJ	:= $(MAIN_OBJ)/arch/x86
BOOT_OBJ	:= $(MAIN_OBJ)/boot/x86
CORE_OBJ	:= $(MAIN_OBJ)/core
DEVICE_OBJ	:= $(MAIN_OBJ)/device
LIB_OBJ		:= $(MAIN_OBJ)/lib

# find all files of a certain type
HDR_FILES := $(shell find src/include -type f -name *.h)
CPP_FILES := $(shell find src -type f -name *.cpp)
ASM_FILES := $(shell find src -type f -name *.s)

# a few other things
CPP_OBJ_FILES :=  $(subst src,obj,$(patsubst %.cpp, %.o, $(CPP_FILES)))
ASM_OBJ_FILES :=  $(subst src,obj,$(patsubst %.s, %.o, $(ASM_FILES)))
OBJ_FILES := $(CPP_OBJ_FILES) $(ASM_OBJ_FILES)
DEP_FILES := $(patsubst %.cpp, %.d, $(CPP_FILES))

# generate a link order
LINK_ORDER_FIRST := /root/opt/cross/lib/gcc/i686-elf/4.8.2/crtbegin.o /root/opt/cross/lib/gcc/i686-elf/4.8.2/crtend.o $(OBJ_DIR)/early_boot.o $(OBJ_DIR)/arch/x86/crti.o $(OBJ_DIR)/boot/x86/boot.o $(OBJ_DIR)/arch/x86/isr_ll.o $(OBJ_DIR)/core/sys.o $(OBJ_DIR)/arch/x86/sys_ll.o
LINK_ORDER_LAST := $(OBJ_DIR)/boot/x86/main.o $(OBJ_DIR)/arch/x86/crtn.o
LINK_ORDER_MID := $(filter-out $(LINK_ORDER_FIRST) $(LINK_ORDER_LAST), $(OBJ_FILES))

# remap stuff to use our cross-compiler
AS := $(HOME)/opt/cross/bin/i686-elf-as
CXX := $(HOME)/opt/cross/bin/i686-elf-g++
CXXFLAGS := -I$(INCLUDE_DIR) -ffreestanding -O2 -g -Wall -Wextra -fno-exceptions -fno-rtti -std=c++11
CC := $(HOME)/opt/cross/bin/i686-elf-gcc
LD := $(HOME)/opt/cross/bin/i686-elf-gcc
LDFLAGS := -ffreestanding -g -O2 -nostdlib

all: omamori.elf

omamori.elf: $(OBJ_FILES)
		@$(LD) -T $(MAIN_SRC)/linker.ld -o ./omamori.elf $(LINK_ORDER_FIRST) $(LINK_ORDER_MID) $(LINK_ORDER_LAST) -lgcc

$(CPP_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.cpp
		@$(CXX) $(CXXFLAGS) -c $< -o $@
	
$(ASM_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.s
		@$(AS) -c $< -o $@
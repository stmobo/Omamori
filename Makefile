#
# Source directories and stuff:
MAIN_SRC	:= src
MAIN_OBJ    := obj
INCLUDE_DIR := $(MAIN_SRC)/include
ISO_DIR     := isodir

ARCH_SRC	:= $(MAIN_SRC)/arch/x86
BOOT_SRC	:= $(MAIN_SRC)/boot/x86
CORE_SRC	:= $(MAIN_SRC)/core
DEVICE_SRC	:= $(MAIN_SRC)/device
LIB_SRC 	:= $(MAIN_SRC)/lib
SRC_DIRS    := $(ARCH_SRC) $(BOOT_SRC) $(CORE_SRC) $(DEVICE_SRC) $(LIB_SRC)
OBJ_DIRS    := $(subst src,obj,$(SRC_DIRS))

# find all files of a certain type
HDR_FILES := $(shell find $(INCLUDE_DIR) -type f -name *.h)
CPP_FILES := $(shell find $(SRC_DIRS) -type f -name *.cpp)
ASM_FILES := $(shell find $(SRC_DIRS) -type f -name *.s)

# a few other things
CPP_OBJ_FILES :=  $(subst src,obj,$(patsubst %.cpp, %.o, $(CPP_FILES)))
ASM_OBJ_FILES :=  $(subst src,obj,$(patsubst %.s, %.o, $(ASM_FILES)))
OBJ_FILES     := $(CPP_OBJ_FILES) $(ASM_OBJ_FILES)
DEP_FILES     := $(patsubst %.cpp, %.d, $(CPP_FILES))

# generate a link order
LINK_ORDER_FIRST := /root/opt/cross/lib/gcc/i686-elf/4.8.2/crtbegin.o /root/opt/cross/lib/gcc/i686-elf/4.8.2/crtend.o $(MAIN_OBJ)/boot/x86/early_boot.o $(MAIN_OBJ)/arch/x86/crti.o $(MAIN_OBJ)/boot/x86/boot.o $(MAIN_OBJ)/arch/x86/isr_ll.o $(MAIN_OBJ)/core/sys.o $(MAIN_OBJ)/arch/x86/sys_ll.o
LINK_ORDER_LAST  := $(MAIN_OBJ)/boot/x86/main.o $(MAIN_OBJ)/arch/x86/crtn.o
LINK_ORDER_MID   := $(filter-out $(LINK_ORDER_FIRST) $(LINK_ORDER_LAST), $(OBJ_FILES))

# remap stuff to use our cross-compiler
AS       := $(HOME)/opt/cross/bin/i686-elf-as
CXX      := $(HOME)/opt/cross/bin/i686-elf-g++
CXXFLAGS := -MMD -I$(INCLUDE_DIR) -ffreestanding -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-parameter -Wno-unused-but-set-variable -Wno-conversion-null -Wno-write-strings -fno-exceptions -fno-rtti -std=c++11
CC       := $(HOME)/opt/cross/bin/i686-elf-gcc
LD       := $(HOME)/opt/cross/bin/i686-elf-gcc
LDFLAGS  := -ffreestanding -g -O2 -nostdlib

all: omamori.iso

omamori.iso: omamori.elf
		@cp omamori.elf $(ISO_DIR)/boot/omamori.elf
		@grub-mkrescue -o omamori.iso $(ISO_DIR) > /dev/null 2>&1
		
omamori.elf: $(OBJ_FILES)
		@$(LD) -T $(MAIN_SRC)/linker.ld $(LDFLAGS) -o ./omamori.elf $(LINK_ORDER_FIRST) $(LINK_ORDER_MID) $(LINK_ORDER_LAST) -lgcc

$(CPP_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.cpp
		@$(CXX) $(CXXFLAGS) -c $< -o $@
	
$(ASM_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.s
		@$(AS) -c $< -o $@
		
clean:
		@$(RM) $(OBJ_FILES)
		@$(RM) $(DEP_FILES)
		@$(RM) omamori.iso
		@$(RM) omamori.elf
        
debug: omamori.elf
		@objcopy --only-keep-debug omamori.elf omamori.sym
		@objcopy --strip-debug omamori.elf
		@readelf -s omamori.sym | sort -k 2,2 > ./symbols
		
.PHONY: clean debug

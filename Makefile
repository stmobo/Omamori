# variables and stuff
# if we typed in "arch=whatever" instead of "ARCH=whatever", then deal with that and move on
ifndef ARCH
# if we didn't select an arch at all, then just use x86 as the default.
ifndef arch
arch := x86
endif
ARCH := $(arch)
endif

#
# Source directories and stuff:
MAIN_SRC	:= src
MAIN_OBJ    := obj
INCLUDE_DIR := $(MAIN_SRC)/include

ISO_DIR     := isodir

ACPICA_SRC	:= $(MAIN_SRC)/acpica
ARCH_SRC	:= $(MAIN_SRC)/arch/$(ARCH)
BOOT_SRC	:= $(MAIN_SRC)/boot/$(ARCH)
CORE_SRC	:= $(MAIN_SRC)/core
DEVICE_SRC	:= $(MAIN_SRC)/device
LIB_SRC 	:= $(MAIN_SRC)/lib
SRC_DIRS    := $(ACPICA_SRC) $(ARCH_SRC) $(BOOT_SRC) $(CORE_SRC) $(DEVICE_SRC) $(LIB_SRC)
OBJ_DIRS    := $(subst src/,obj/,$(SRC_DIRS))

# find all files of a certain type
HDR_FILES    := $(shell find $(INCLUDE_DIR) -type f -name *.h)
C_FILES      := $(shell find $(SRC_DIRS) -type f -name *.c)
CPP_FILES    := $(shell find $(SRC_DIRS) -type f -name *.cpp)
ASM_FILES    := $(shell find $(SRC_DIRS) -type f -name *.s)

# a few other things
C_OBJ_FILES   :=  $(subst src/,obj/,$(patsubst %.c, %.o, $(C_FILES)))
CPP_OBJ_FILES :=  $(subst src/,obj/,$(patsubst %.cpp, %.o, $(CPP_FILES)))
ASM_OBJ_FILES :=  $(subst src/,obj/,$(patsubst %.s, %.o, $(ASM_FILES)))
OBJ_FILES     :=  $(ASM_OBJ_FILES) $(C_OBJ_FILES) $(CPP_OBJ_FILES)
DEP_FILES     :=  $(patsubst %.o, %.d, $(CPP_OBJ_FILES) $(C_OBJ_FILES))
CRTBEGIN_OBJ  :=  $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ    :=  $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)

# generate a link order
LINK_ORDER_FIRST := $(MAIN_OBJ)/arch/$(ARCH)/crti.o $(CRTBEGIN_OBJ) $(MAIN_OBJ)/boot/$(ARCH)/early_boot.o $(MAIN_OBJ)/boot/$(ARCH)/boot.o $(MAIN_OBJ)/arch/$(ARCH)/isr_ll.o $(MAIN_OBJ)/core/sys.o $(MAIN_OBJ)/arch/$(ARCH)/sys_ll.o
LINK_ORDER_LAST  := $(MAIN_OBJ)/boot/$(ARCH)/arch_init.o $(MAIN_OBJ)/core/main.o $(CRTEND_OBJ) $(MAIN_OBJ)/arch/$(ARCH)/crtn.o
LINK_ORDER_MID   := $(filter-out $(LINK_ORDER_FIRST) $(LINK_ORDER_LAST), $(OBJ_FILES))

# remap stuff to use our cross-compiler
AS       := $(HOME)/opt/cross/bin/i686-elf-as
CC       := $(HOME)/opt/cross/bin/i686-elf-gcc
CCFLAGS  := -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/acpica -I$(INCLUDE_DIR)/newlib -MMD -MP -std=gnu99 -ffreestanding -g -O2 -Wall -Wextra -Wno-unused-parameter -fno-omit-frame-pointer -fno-strict-aliasing
CXX      := $(HOME)/opt/cross/bin/i686-elf-g++
CXXFLAGS := -MMD -MP -I$(INCLUDE_DIR) -I$(INCLUDE_DIR)/newlib -I$(MAIN_SRC)/lib/lua -D__$(ARCH)__ -ffreestanding -g -O2 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-parameter -Wno-unused-but-set-variable -Wno-conversion-null -Wno-write-strings -fno-exceptions -fno-rtti -fno-omit-frame-pointer -std=c++11
LD       := $(HOME)/opt/cross/bin/i686-elf-gcc
LDFLAGS  := -nostdlib -ffreestanding -g -O2 -L./lib/i686-elf/lib
LDLIBS   := -lm -lc -lgcc

all: omamori.iso

omamori.iso: omamori.elf
		@cp omamori.elf $(ISO_DIR)/boot/omamori.elf
		@grub-mkrescue -o omamori.iso $(ISO_DIR) > /dev/null 2>&1
		
omamori.elf: omamori_embedded_debug.elf
		@$(HOME)/opt/cross/bin/i686-elf-objcopy -S omamori_embedded_debug.elf omamori.elf
		
debug: omamori_embedded_debug.elf
		@$(HOME)/opt/cross/bin/i686-elf-objcopy --only-keep-debug omamori_embedded_debug.elf omamori.sym
		@$(HOME)/opt/cross/bin/i686-elf-readelf -s omamori.sym | sort -k 2,2 > ./symbols
		
omamori_embedded_debug.elf: $(OBJ_FILES)
		@$(LD) -T $(MAIN_SRC)/linker.ld $(LDFLAGS) -o ./omamori_embedded_debug.elf $(LINK_ORDER_FIRST) $(LINK_ORDER_MID) $(LINK_ORDER_LAST) $(LDLIBS)
		
clean:
		@$(RM) $(OBJ_FILES)
		@$(RM) $(DEP_FILES)
		@$(RM) omamori.iso
		@$(RM) omamori.elf
		
$(C_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.c
		@$(CC) $(CCFLAGS) -c $< -o $@
		
$(CPP_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.cpp
		@$(CXX) $(CXXFLAGS) -c $< -o $@
	
$(ASM_OBJ_FILES): $(MAIN_OBJ)/%.o : $(MAIN_SRC)/%.s
		@$(AS) -c $< -o $@
		
-include $(DEP_FILES)
		
.PHONY: clean debug

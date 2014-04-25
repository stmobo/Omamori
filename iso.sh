#!/bin/sh
OPTIONS="-ffreestanding -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-parameter -Wno-unused-but-set-variable -Wno-conversion-null -Wno-write-strings -Wno-narrowing -fno-exceptions -fno-rtti -std=c++11"
OBJ_DIR="./obj"
SRC_DIR="./src"

echo "Cleaning old files."
rm obj/*
rm omamori.elf

echo "Assembling .s files."
for i in $SRC_DIR/*.s; do
    s=${i##*/}
    n=${s%%.*}
    echo "Assembling:" $n".s"
    i686-elf-as -c $SRC_DIR/$n.s -o $OBJ_DIR/$n.o
done

# ugly hack, make more elegant later
mv $OBJ_DIR/boot.o $OBJ_DIR/__boot.o
mv $OBJ_DIR/sys.o $OBJ_DIR/_sys.o
mv $OBJ_DIR/isr_ll.o $OBJ_DIR/_isr_ll.o
mv $OBJ_DIR/crti.o $OBJ_DIR/__crti.o
mv $OBJ_DIR/crtn.o $OBJ_DIR/zzzz_crtn.o

echo "Compiling .cpp files."
for i in $SRC_DIR/*.cpp; do
    s=${i##*/}
    n=${s%%.*}
    echo "Compiling:" $n".cpp"
    i686-elf-g++ -c $SRC_DIR/$n.cpp -o $OBJ_DIR/$n.o $OPTIONS
done

mv $OBJ_DIR/irq.o $OBJ_DIR/_irq.o
mv $OBJ_DIR/main.o $OBJ_DIR/zzz_main.o

echo "Linking final product."
i686-elf-gcc -T $SRC_DIR/linker.ld -o ./omamori.elf -ffreestanding -g -O2 -nostdlib "/root/opt/cross/lib/gcc/i686-elf/4.8.2/crtbegin.o" "/root/opt/cross/lib/gcc/i686-elf/4.8.2/crtend.o" $OBJ_DIR/*.o -lgcc

echo "Making debug symbol file."
objcopy --only-keep-debug omamori.elf omamori.sym
objcopy --strip-debug omamori.elf

echo "Creating .iso."
cp omamori.elf isodir/boot/omamori.elf
grub-mkrescue -o omamori.iso isodir
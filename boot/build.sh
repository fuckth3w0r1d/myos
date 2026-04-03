nasm -I include/ -o ../build/mbr.bin mbr.S
dd if=../build/mbr.bin of=../vdisk.img bs=512 count=1 conv=notrunc
nasm -I include/ -o ../build/loader.bin loader.S
dd if=../build/loader.bin of=../vdisk.img bs=512 count=4 seek=2 conv=notrunc
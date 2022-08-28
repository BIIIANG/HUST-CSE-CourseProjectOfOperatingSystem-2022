##################################################
# Makefile of our final project
##################################################

SRC:=final_project_task1.asm
BIN:=$(subst .asm,.com,$(SRC))

.PHONY : everything clean

everything : $(BIN)
	sudo mount -o loop pm.img /mnt/floppy/
	sudo cp $(BIN) /mnt/floppy/ -v
	sudo umount /mnt/floppy/

$(BIN) : $(SRC)
	nasm $< -o $@

format:
	dd if=/dev/null of=pm.img bs=512 count=1 conv=notrunc
	losetup /dev/loop10 pm.img
	mkfs.msdos /dev/loop10
	fsck.msdos /dev/loop10
	losetup -d /dev/loop10

bochs:
	bochs -f bochsrc.txt

clean:
	rm -rf $(BIN)

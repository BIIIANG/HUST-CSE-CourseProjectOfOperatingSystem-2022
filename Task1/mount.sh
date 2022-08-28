#!/bin/bash
if [ ! -d /mnt/floppy ]; then 
    sudo mkdir /mnt/floppy; 
fi
nasm final_project_task1.asm -o final_project_task1.com
sudo mount -o loop pm.img /mnt/floppy
sudo cp final_project_task1.com /mnt/floppy
sudo umount /mnt/floppy
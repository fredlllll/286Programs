del bootloader.obj
del main.obj
wcc -2 -d0 -wx -ms -s -zl bootloader.c
wcc -2 -d0 -wx -ms -s -zl main.c
wlink file bootloader.obj format raw bin name bootloader.bin option NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00
wlink file main.obj format raw bin name main.bin option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00 order clname CODE SEGMENT start_segment
python assemble_binary.py
import os
import shutil
import sys

        
sources = [
"main"
]

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass
        
def compile_source(filename):
    result = os.system("wcc -2 -d0 -wx -ms -s -zl "+filename)
    if result != 0:
        sys.exit("\r\nfailed to compile "+filename)
        
def link_source(filename_no_ext):
    result = os.system("wlink file "+filename_no_ext+".obj format raw bin name "+filename_no_ext+".bin option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00 order clname CODE SEGMENT start_segment")
    if result != 0:
        sys.exit("\r\nfailed to link "+filename_no_ext+".obj")
    
        
def process_sources():
    for src in sources:
        silent_remove(src+".obj")

    for src in sources:
        compile_source(src+".c")
        
    for src in sources:
        link_source(src)
    
def process_bootloader():
    silent_remove("bootloader.tmp.obj")
    #modify bootloader code  
    with open("bootloader.c", 'r') as f:
        bootloader_code = f.read()
        
    with open("main.bin", 'rb') as f:
        main = f.read()
    main_sectors = len(main) // 512 +1
    print(f"\r\nmain uses {main_sectors} sectors\r\n")
      
    bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
    with open("bootloader.tmp.c",'w') as f:
        f.write(bootloader_code)
     
    result = os.system("wcc -2 -d0 -wx -ms -s -zl bootloader.tmp.c")
    os.remove("bootloader.tmp.c")
    if result != 0:
        sys.exit("\r\nfailed to compile bootloader.c")
    os.system("wlink file bootloader.tmp.obj format raw bin name bootloader.bin option NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00")
    if result != 0:
        sys.exit("\r\nfailed to link bootloader.obj")
        
    with open('bootloader.bin','rb') as f:
        bootloader = f.read()
        
    signature = 0x55AA.to_bytes(2,'big')
    bootloader = bootloader.ljust(510, b'\0') +signature

    floppy_sectors = 2*80*18

    main = main.ljust((floppy_sectors-1)*512,b'\0')
  
    return bootloader,main
    

def main_func():
    process_sources()
    bootloader, main = process_bootloader()
    
    with open('output.img', 'wb') as f:
    f.write(bootloader)
    f.write(main)

    print("\r\nsuccess! wrote output to output.img")
    
main_func()


 



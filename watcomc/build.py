import os
import shutil
import sys

def silent_remove(filename):
    try:
        os.remove(filename)
    except:
        pass

#delete old files
silent_remove("bootloader.tmp.obj")
silent_remove("main.obj")
#compile main
result = os.system("wcc -2 -d0 -wx -ms -s -zl main.c")
if result != 0:
  sys.exit("failed to compile main.c")
#link main
result = os.system("wlink file main.obj format raw bin name main.bin option NODEFAULTLIBS,verbose,start=main_,OFFSET=0x7E00 order clname CODE SEGMENT start_segment")
if result != 0:
  sys.exit("failed to link main.obj")

with open("main.bin", 'rb') as f:
  main = f.read()

main_sectors = len(main) // 512 +1
print(f"main uses {main_sectors} sectors")

#modify bootloader code  
with open("bootloader.c", 'r') as f:
  bootloader_code = f.read()
  
bootloader_code = bootloader_code.replace("%%num_sectors%%", str(main_sectors))
with open("bootloader.tmp.c",'w') as f:
  f.write(bootloader_code)
 
result = os.system("wcc -2 -d0 -wx -ms -s -zl bootloader.tmp.c")
os.remove("bootloader.tmp.c")
if result != 0:
  sys.exit("failed to compile bootloader.c")
os.system("wlink file bootloader.tmp.obj format raw bin name bootloader.bin option NODEFAULTLIBS,verbose,start=init_,OFFSET=0x7C00")
if result != 0:
  sys.exit("failed to link bootloader.obj")

  
with open('bootloader.bin','rb') as f:
  bootloader = f.read()
 
signature = 0x55AA.to_bytes(2,'big')
bootloader = bootloader.ljust(510, b'\0') +signature


main = main.ljust(main_sectors*512,b'\0')

with open('output.img', 'wb') as f:
  f.write(bootloader)
  f.write(main)
  
print("success! wrote output to output.img")
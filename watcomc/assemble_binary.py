with open('bootloader.bin','rb') as f:
  bootloader = f.read()
  
with open('main.bin', 'rb') as f:
  main = f.read()
 
signature = 0x55AA.to_bytes(2,'big')
bootloader = bootloader.ljust(510, b'\0') +signature

main_sectors = len(main) // 512 +1
main = main.ljust(main_sectors*512,b'\0')

with open('output.img', 'wb') as f:
  f.write(bootloader)
  f.write(main)
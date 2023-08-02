void print_asm (unsigned char inchar, unsigned short page_color);
#pragma aux print_asm = \
    "mov ah, 0x0e"   \
    "int 0x10"       \
    modify [ah]      \
    parm   [al][bx]

void print(const char* text){
  char ch;
  while (ch = *text++){
    print_asm (ch, 0x0001);
  }
}

void _cstart(void){
  //shut up linker who cant find _cstart_ that it doesnt need
}
#pragma code_seg ( "start_segment" )
void main(void){
  print("this is a test");
  while (1){
    __asm { hlt };
  }
}
#pragma code_seg ()

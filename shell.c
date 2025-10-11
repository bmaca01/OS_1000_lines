#include "user.h"

void main(void) {
    //*((volatile int *) 0x80200000) = 0x1234;    // should cause exception 
                                                // since trying to write to 
                                                // non-userland memory
    printf("WOWOWOWWO HELLO FROM SHELL");
    //for (;;);
}

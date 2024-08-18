void main(void) {
    for (;;) {
        __asm__ volatile("mov $0, %%eax\n"
                         "mov $1000, %%ecx\n"
                         "int $100"
                         : /* output */
                         : /* input */
                         : "eax" /* clobbers */);
    }
}

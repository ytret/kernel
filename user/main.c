void main(void) {
    __asm__ volatile("mov $2, %%eax\n"
                     "int $100"
                     : /* output */
                     : /* input */
                     : "eax" /* clobbers */);
    for (;;) {}
}

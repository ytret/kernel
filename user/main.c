void main(void) {
    __asm__ volatile ("int $100");
    for (;;) {}
}

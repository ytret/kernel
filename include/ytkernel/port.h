#pragma once

#include <stdint.h>

static inline void port_outb(uint16_t port, uint8_t byte) {
    __asm__ volatile("outb %0, %1"
                     : /* no outputs */
                     : "a"(byte), "Nd"(port)
                     : "memory");
}

static inline uint8_t port_inb(uint16_t port) {
    uint8_t byte;
    __asm__ volatile("inb %1, %0" : "=a"(byte) : "Nd"(port) : "memory");
    return (byte);
}

static inline void port_outl(uint16_t port, uint32_t dword) {
    __asm__ volatile("outl %0, %1"
                     : /* no outputs */
                     : "a"(dword), "Nd"(port)
                     : "memory");
}

static inline uint32_t port_inl(uint16_t port) {
    uint32_t dword;
    __asm__ volatile("inl %1, %0" : "=a"(dword) : "Nd"(port) : "memory");
    return (dword);
}

target_sources(kernel PRIVATE

    apic/ioapic.c
    apic/lapic.c
    arch_boot.c
    arch_smp.c
    arch_taskmgr.c
    arch_vmm.c
    arch.c
    gdt.c
    idt.c
    kbd_arch.c
    mbi.c
    pic.c
    pit.c
    serial_arch.c
    vga.c

    boot.s
    entry.s
    gdt.s
    idt.s
    isrs.s
    stacktrace.s
    taskmgr.s
    vmm.s

)

target_compile_definitions(kernel PRIVATE -DYTKERNEL_ARCH_X86=1)
target_link_options(kernel PRIVATE
    -T${CMAKE_SOURCE_DIR}/src/arch/x86/linker.ld
)
set_target_properties(kernel PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/x86/linker.ld
)

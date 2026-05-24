target_sources(kernel PRIVATE

    apic/ioapic.c
    apic/lapic.c
    arch_smp.c
    arch_taskmgr.c
    arch.c
    gdt.c
    idt.c
    vmm.c

    boot.s
    entry.s
    gdt.s
    idt.s
    isrs.s
    panic.s
    vmm.s

)

target_link_options(kernel PRIVATE
    -T${CMAKE_SOURCE_DIR}/src/arch/x86/linker.ld
)
set_target_properties(kernel PROPERTIES
    LINK_DEPENDS ${CMAKE_SOURCE_DIR}/src/arch/x86/linker.ld
)

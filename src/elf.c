#include <stdbool.h>
#include <stddef.h>

#include "elf.h"
#include "kprintf.h"
#include "pmm.h"
#include "vmm.h"

#define MAGIC_NUM         ((uint32_t)0x464C457F)
#define BITS_32BIT        ((uint8_t)1)
#define BITS_64BIT        ((uint8_t)2)
#define BYTE_ORDER_LITTLE ((uint8_t)1)
#define BYTE_ORDER_BIG    ((uint8_t)2)
#define HEADER_VERSION    ((uint8_t)1)
#define OS_ABI_SYSV       ((uint8_t)0)
#define TYPE_RELOC        ((uint16_t)1)
#define TYPE_EXEC         ((uint16_t)2)
#define TYPE_SHARED       ((uint16_t)3)
#define TYPE_CORE         ((uint16_t)4)
#define ARCH_X86          ((uint16_t)3)

#define PHDR_TYPE_IGNORE  ((uint32_t)0)
#define PHDR_TYPE_LOAD    ((uint32_t)1)
#define PHDR_TYPE_DYNAMIC ((uint32_t)2)
#define PHDR_TYPE_INTERP  ((uint32_t)3)
#define PHDR_TYPE_NOTE    ((uint32_t)4)
#define PHDR_FLAG_EXEC    ((uint32_t)1)
#define PHDR_FLAG_WRITE   ((uint32_t)2)
#define PHDR_FLAG_READ    ((uint32_t)4)

#define SHDR_TYPE_IGNORE   ((uint32_t)0)
#define SHDR_TYPE_PROGBITS ((uint32_t)1)
#define SHDR_TYPE_SYMTAB   ((uint32_t)2)
#define SHDR_TYPE_STRTAB   ((uint32_t)3)
#define SHDR_TYPE_RELA     ((uint32_t)4)
#define SHDR_TYPE_NOBITS   ((uint32_t)8)
#define SHDR_TYPE_REL      ((uint32_t)9)
#define SHDR_FLAG_WRITE    ((uint32_t)1)
#define SHDR_FLAG_ALLOC    ((uint32_t)2)

typedef struct __attribute__((packed)) {
    uint32_t magic_num;
    uint8_t bits;
    uint8_t byte_order;
    uint8_t header_version;
    uint8_t os_abi;
    uint8_t abi_version;
    uint8_t padding[7];
    uint16_t elf_type;
    uint16_t arch;
    uint32_t elf_version;
    uint32_t entry;
    uint32_t ph_offset;
    uint32_t sh_offset;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t ph_entry_size;
    uint16_t ph_num_entries;
    uint16_t sh_entry_size;
    uint16_t sh_num_entries;
    uint16_t shstrndx;
} elf_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t reserved;
    uint32_t file_size;
    uint32_t mem_size;
    uint32_t flags;
    uint32_t align;
} prog_hdr_t;

typedef struct __attribute__((packed)) {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addr_align;
    uint32_t entry_size;
} sect_hdr_t;

static bool check_hdr_valid(elf_hdr_t const *p_hdr);

static void dump_general(elf_hdr_t const *p_hdr);

static void dump_prog_hdrs(elf_hdr_t const *p_hdr);
static void dump_prog_hdr(prog_hdr_t const *p_phdr);

static void dump_sect_hdrs(elf_hdr_t const *p_hdr);
static void dump_sect_hdr(elf_hdr_t const *p_hdr, sect_hdr_t const *p_shdr);

static char const *sect_name(elf_hdr_t const *p_hdr, sect_hdr_t const *p_shdr);

static void const *offset(elf_hdr_t const *p_hdr, uint32_t offset);

bool elf_load(uint32_t *p_dir, uint32_t addr, uint32_t *p_entry) {
    elf_hdr_t const *p_hdr = ((elf_hdr_t const *)addr);
    check_hdr_valid(p_hdr);

    // Check if the header is loadable.
    if (BITS_32BIT != p_hdr->bits) {
        kprintf("elf: cannot load a non-32-bit executable\n");
        return false;
    }
    if (BYTE_ORDER_LITTLE != p_hdr->byte_order) {
        kprintf("elf: cannot load a non-little-endian executable\n");
        return false;
    }
    if (OS_ABI_SYSV != p_hdr->os_abi) {
        kprintf("elf: cannot load a non-SYSV-executable\n");
        return false;
    }

    // Check the program header table entry size.
    if (p_hdr->ph_entry_size != sizeof(prog_hdr_t)) {
        kprintf("elf: cannot load an executable with program header size of %u"
                " bytes\n",
                p_hdr->ph_entry_size);
        return false;
    }

    // Load the program segments.
    prog_hdr_t const *p_phdrs = offset(p_hdr, p_hdr->ph_offset);
    for (size_t idx = 0; idx < p_hdr->ph_num_entries; idx++) {
        prog_hdr_t const *p_phdr = &p_phdrs[idx];
        if (PHDR_TYPE_LOAD != p_phdr->type) {
            // Not a loadable segment.
            continue;
        }

        // Check if the segment is to be loaded in the user part of VAS.
        if (p_phdr->vaddr < VMM_USER_START) {
            kprintf("elf: load failed: program header %u vaddr 0x%08X is not in"
                    " the user part of VAS\n",
                    idx, p_phdr->vaddr);
            return false;
        }

        // Check that the virtual address is page-aligned.
        if (p_phdr->vaddr & 0xFFF) {
            kprintf("elf: load failed: program header %u vaddr 0x%08X is not"
                    " page-aligned\n",
                    idx, p_phdr->vaddr);
            return false;
        }

        // Check that the file size is not greater than memory size.
        if (p_phdr->file_size > p_phdr->mem_size) {
            kprintf(
                "elf: load failed: program header %u file size %u is greater"
                " than memory size %u\n",
                idx, p_phdr->file_size, p_phdr->mem_size);
            return false;
        }

        // Allocate memory.
        uint32_t start_virt = p_phdr->vaddr;
        uint32_t end_virt = ((start_virt + p_phdr->mem_size + 0xFFF) & ~0xFFF);
        for (uint32_t virt = start_virt; virt < end_virt; virt += 4096) {
            uint32_t phys = pmm_pop_page();
            vmm_map_user_page(p_dir, virt, phys);

            // Temporarily map the pages in the kernel VAS.
            vmm_map_kernel_page(virt, phys);
        }

        // Copy.
        __builtin_memcpy(((void *)start_virt), offset(p_hdr, p_phdr->offset),
                         p_phdr->file_size);
        __builtin_memset(((void *)(start_virt + p_phdr->file_size)), 0,
                         (p_phdr->mem_size - p_phdr->file_size));

        // Unmap the pages in the kernel(!) VAS.
        for (uint32_t virt = start_virt; virt < end_virt; virt += 4096) {
            vmm_unmap_kernel_page(virt);
        }
    }

    *p_entry = p_hdr->entry;

    return true;
}

void elf_dump(uint32_t addr) {
    elf_hdr_t const *p_hdr = ((elf_hdr_t const *)addr);
    check_hdr_valid(p_hdr);

    dump_general(p_hdr);
    dump_prog_hdrs(p_hdr);
    dump_sect_hdrs(p_hdr);
}

static bool check_hdr_valid(elf_hdr_t const *p_hdr) {
    if (p_hdr->magic_num != MAGIC_NUM) {
        kprintf("elf: check_hdr: invalid magic number: 0x%08X\n",
                p_hdr->magic_num);
        return false;
    }

    if ((p_hdr->bits != BITS_32BIT) && (p_hdr->bits != BITS_64BIT)) {
        kprintf("elf: check_hdr: invalid field 'bits': %u\n", p_hdr->bits);
        return false;
    }

    if ((p_hdr->byte_order != BYTE_ORDER_LITTLE) &&
        (p_hdr->byte_order != BYTE_ORDER_BIG)) {
        kprintf("elf: check_hdr: invalid field 'byte order': %u\n",
                p_hdr->byte_order);
        return false;
    }

    if (p_hdr->header_version != HEADER_VERSION) {
        kprintf("elf: check_hdr: unknown header version: %u\n",
                p_hdr->header_version);
        return false;
    }

    if (p_hdr->os_abi != OS_ABI_SYSV) {
        kprintf("elf: check_hdr: unknown OS ABI: %u\n", p_hdr->os_abi);
        return false;
    }

    return true;
}

static void dump_general(elf_hdr_t const *p_hdr) {
    kprintf("ELF");

    // 32-bit or 64-bit.
    if (BITS_32BIT == p_hdr->bits) {
        kprintf(" 32-bit");
    } else {
        kprintf(" 64-bit");
    }

    // CPU architecture.
    if (ARCH_X86 == p_hdr->arch) {
        kprintf(" x86");
    } else {
        kprintf(" unknown arch");
    }

    // Byte order.
    if (BYTE_ORDER_LITTLE == p_hdr->byte_order) {
        kprintf(" little-endian");
    } else {
        kprintf(" big-endian");
    }

    // Executable type.
    if (TYPE_RELOC == p_hdr->elf_type) {
        kprintf(" relocatable");
    } else if (TYPE_EXEC == p_hdr->elf_type) {
        kprintf(" executable");
    } else if (TYPE_SHARED == p_hdr->elf_type) {
        kprintf(" shared");
    } else if (TYPE_CORE == p_hdr->elf_type) {
        kprintf(" core");
    }

    // ABI.
    if (OS_ABI_SYSV == p_hdr->os_abi) {
        kprintf(", SYSV ABI (version %u)", p_hdr->abi_version);
    }

    kprintf("\n");

    kprintf("Entry: 0x%08X\n", p_hdr->entry);
}

static void dump_prog_hdrs(elf_hdr_t const *p_hdr) {
    if (p_hdr->ph_entry_size != ((uint16_t)sizeof(prog_hdr_t))) {
        kprintf(
            "elf: unexpected declared program header size: %u (expected %u)",
            p_hdr->ph_entry_size, sizeof(prog_hdr_t));
        return;
    }

    kprintf("Program headers:\n");
    kprintf("TYPE  OFFSET      VADDR        FILE SIZE    MEM SIZE  ALIGN  FLAGS"
            "\n");

    prog_hdr_t const *p_phdrs = offset(p_hdr, p_hdr->ph_offset);
    for (size_t idx = 0; idx < p_hdr->ph_num_entries; idx++) {
        dump_prog_hdr(&p_phdrs[idx]);
    }
}

static void dump_prog_hdr(prog_hdr_t const *p_phdr) {
    if (PHDR_TYPE_IGNORE == p_phdr->type) {
        kprintf("ignore");
        return;
    } else if (PHDR_TYPE_LOAD == p_phdr->type) {
        kprintf("load  ");
    } else if (PHDR_TYPE_DYNAMIC == p_phdr->type) {
        kprintf("dyn   ");
    } else if (PHDR_TYPE_INTERP == p_phdr->type) {
        kprintf("intr  ");
    } else if (PHDR_TYPE_NOTE == p_phdr->type) {
        kprintf("note  ");
    } else {
        kprintf("unknown type");
        return;
    }

    char p_flags[4] = "   ";
    p_flags[0] = ((p_phdr->flags & PHDR_FLAG_READ) ? 'r' : '-');
    p_flags[1] = ((p_phdr->flags & PHDR_FLAG_WRITE) ? 'w' : '-');
    p_flags[2] = ((p_phdr->flags & PHDR_FLAG_EXEC) ? 'x' : '-');

    kprintf("0x%08x  0x%08x  %10u  %10u  %5u  %5s\n", p_phdr->offset,
            p_phdr->vaddr, p_phdr->file_size, p_phdr->mem_size, p_phdr->align,
            p_flags);
}

static void dump_sect_hdrs(elf_hdr_t const *p_hdr) {
    if (p_hdr->sh_entry_size != ((uint16_t)sizeof(sect_hdr_t))) {
        kprintf(
            "elf: unexpected declared section header size: %u (expected %u)",
            p_hdr->ph_entry_size, sizeof(prog_hdr_t));
        return;
    }

    kprintf("Section headers:\n");
    kprintf(
        "NAME                 TYPE      OFFSET        SIZE  ALIGN  FLAGS\n");

    sect_hdr_t const *p_shdrs = offset(p_hdr, p_hdr->sh_offset);
    for (size_t idx = 0; idx < p_hdr->sh_num_entries; idx++) {
        dump_sect_hdr(p_hdr, &p_shdrs[idx]);
    }
}

static void dump_sect_hdr(elf_hdr_t const *p_hdr, sect_hdr_t const *p_shdr) {
    char const *p_type = "unknown";
    switch (p_shdr->type) {
    case SHDR_TYPE_IGNORE:
        return;
    case SHDR_TYPE_PROGBITS:
        p_type = "progbits";
        break;
    case SHDR_TYPE_SYMTAB:
        p_type = "symtab";
        break;
    case SHDR_TYPE_STRTAB:
        p_type = "strtab";
        break;
    case SHDR_TYPE_RELA:
        p_type = "rela";
        break;
    case SHDR_TYPE_NOBITS:
        p_type = "nobits";
        break;
    case SHDR_TYPE_REL:
        p_type = "rel";
        break;
    }

    char const *p_name = sect_name(p_hdr, p_shdr);
    if (!p_name) { p_name = "(anon)"; }

    char p_flags[3] = "  ";
    p_flags[0] = ((p_shdr->flags & SHDR_FLAG_WRITE) ? 'w' : '-');
    p_flags[1] = ((p_shdr->flags & SHDR_FLAG_ALLOC) ? 'a' : '-');

    kprintf("%-15s  %8s  0x%08X  %10u  %5u  %5s\n", p_name, p_type,
            p_shdr->offset, p_shdr->size, p_shdr->addr_align, p_flags);
}

static char const *sect_name(elf_hdr_t const *p_hdr, sect_hdr_t const *p_shdr) {
    if (0 == p_hdr->shstrndx) { return NULL; }

    // Get the string table.
    sect_hdr_t const *p_shdrs = offset(p_hdr, p_hdr->sh_offset);
    sect_hdr_t const *p_shstr = &p_shdrs[p_hdr->shstrndx];
    char const *p_strtbl = offset(p_hdr, p_shstr->offset);

    // Get the string.
    return p_strtbl + p_shdr->name;
}

static void const *offset(elf_hdr_t const *p_hdr, uint32_t offset) {
    uint32_t hdr_addr = ((uint32_t)p_hdr);
    uint32_t res_addr = hdr_addr + offset;
    return (void const *)res_addr;
}

#include "arch_vmm.h"
#include "memfun.h"
#include "pmm.h"
#include "usermem.h"

static bool prv_usermem_check_krgn(uintptr_t k_start, size_t size);
static bool prv_usermem_check_urgn(uintptr_t u_start, size_t size);

kerr_t usermem_copy_to_k(void *kdst, const void *usrc, size_t size) {
    const uintptr_t k_start = (uintptr_t)kdst;
    if (!prv_usermem_check_krgn(k_start, size)) { return KERR_BAD_ADDR; }

    const uintptr_t u_start = (uintptr_t)usrc;
    if (!prv_usermem_check_urgn(u_start, size)) { return KERR_BAD_ADDR; }

    kmemcpy(kdst, usrc, size);
    return KERR_NONE;
}

kerr_t usermem_copy_to_u(void *udst, const void *ksrc, size_t size) {
    const uintptr_t k_start = (uintptr_t)ksrc;
    if (!prv_usermem_check_krgn(k_start, size)) { return KERR_BAD_ADDR; }

    const uintptr_t u_start = (uintptr_t)udst;
    if (!prv_usermem_check_urgn(u_start, size)) { return KERR_BAD_ADDR; }

    kmemcpy(udst, ksrc, size);
    return KERR_NONE;
}

static bool prv_usermem_check_krgn(uintptr_t k_start, size_t size) {
    if (k_start >= UINTPTR_MAX - size) { return false; }
    return true;
}

static bool prv_usermem_check_urgn(uintptr_t u_start, size_t size) {
    if (u_start >= UINTPTR_MAX - size) { return false; }
    const uintptr_t u_end = u_start + size;

    if (u_start < VMM_USER_START) { return false; }
    if (u_end > VMM_USER_END) { return false; }

    const uintptr_t u_start_pg = PMM_PAGE_ALIGN_DOWN(u_start);
    const uintptr_t u_end_pg = PMM_PAGE_ALIGN_UP(u_end);
    for (uintptr_t page = u_start_pg; page < u_end_pg; page += PMM_PAGE_SIZE) {
        if (!vmm_is_addr_mapped(page)) { return false; }
    }

    return true;
}

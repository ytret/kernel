#include <ytalloc/ytalloc.h>

#include "arch.h"
#include "assert.h"
#include "kinttypes.h"
#include "list.h"
#include "log.h"
#include "memfun.h"
#include "vmm2.h"
#include "vmm2_arch.h"

#define VMM_PREHEAP_REGIONS     32
#define VMM_PREHEAP_STATIC_SIZE (VMM_PREHEAP_REGIONS * sizeof(vmm_rgn_t))

#define VMM_FIRST_ADDR ((vaddr_t)0)
#define VMM_LAST_ADDR  ((vaddr_t)VADDR_MAX)

// FIXME
#define VMM_PAGE_ALIGN_UP(x)   PMM_PAGE_ALIGN_UP(x)
#define VMM_PAGE_ALIGN_DOWN(x) PMM_PAGE_ALIGN_DOWN(x)

typedef struct {
    list_node_t node;
    vmm_rgn_type_t type;
    vaddr_t start;
    vaddr_t end_incl;
    vmm_prot_t prot;
} vmm_rgn_t;

typedef struct {
    list_t list; //!< List of regions (item type #vmm_rgn_t).
} vmm_rgnlist_t;

struct vmm_vas {
    vmm_vas_arch_t *arch;
    vmm_rgnlist_t regions;
};

static vmm_vas_t g_vmm_kvas;
static alloc_static_t g_vmm_preheap_rgns;
static uint8_t g_vmm_preheap_buf[VMM_PREHEAP_STATIC_SIZE];

static void prv_vmm_init_kvas(void);
static void prv_vmm_add_boot_regions(vmm_rgnlist_t *rgns);
static void prv_vmm_add_kernel_regions(vmm_rgnlist_t *rgns);

static vmm_rgn_t *prv_vmm_new_rgn(void);
static void prv_vmm_free_rgn(vmm_rgn_t *rgn);
static const char *prv_vmm_rgn_type_name(vmm_rgn_type_t type);

static void prv_vmm_rgnlist_init(vmm_rgnlist_t *rgns);
static bool prv_vmm_rgnlist_insert(vmm_rgnlist_t *rgns, vmm_rgn_t *rgn);
static vmm_rgn_t *prv_vmm_find_rgn_by_addr(vmm_rgnlist_t *rgns, vaddr_t virt);
static void prv_vmm_rgnlist_dump(const vmm_rgnlist_t *rgns);
static bool prv_vmm_rgnlist_check(const vmm_rgnlist_t *rgns);
static bool prv_vmm_check_rgn(const vmm_rgn_t *rgn);

void vmm2_init(void) {
    alloc_static_init(&g_vmm_preheap_rgns, g_vmm_preheap_buf,
                      VMM_PREHEAP_STATIC_SIZE);
    prv_vmm_init_kvas();
}

vmm_vas_t *vmm_new_vas(void);
void vmm_free_vas(vmm_vas_t *vas);

vmm_vas_t *vmm_get_kvas(void) {
    return &g_vmm_kvas;
}

void vmm_enter_vas(const vmm_vas_t *vas);

bool vmm_map_range(vmm_vas_t *vas, vaddr_t virt, vaddr_t phys, size_t num_pages,
                   vmm_rgn_type_t type, vmm_prot_t prot) {
    DEBUG_ASSERT(vas != NULL);
    ASSERT(VMM_ADDR_PGOFF(virt) == 0);
    ASSERT(VMM_ADDR_PGOFF(virt) == 0);
    ASSERT(num_pages < SIZE_MAX / VMM_PAGE_SIZE);
    const size_t size = num_pages * VMM_PAGE_SIZE;
    ASSERT(virt <= VADDR_MAX - size);
    const vaddr_t end_excl = virt + size;
    const vaddr_t end_incl = end_excl - 1;

    vmm_rgn_t *const new_rgn = prv_vmm_new_rgn();
    new_rgn->start = virt;
    new_rgn->end_incl = end_incl;
    new_rgn->type = type;
    new_rgn->prot = prot;
    if (!prv_vmm_rgnlist_insert(&vas->regions, new_rgn)) {
        prv_vmm_free_rgn(new_rgn);
        return false;
    }

    // FIXME: do I need to check if any page in the region is already mapped?
    vaddr_t v_page;
    paddr_t p_page;
    for (v_page = new_rgn->start, p_page = phys; v_page <= new_rgn->end_incl;
         v_page += VMM_PAGE_SIZE, p_page += VMM_PAGE_SIZE) {
        vmm_arch_map(vas->arch, v_page, p_page, prot);
    }

    return phys;
}

bool vmm_alloc(vmm_vas_t *vas, size_t num_pages, vmm_rgn_type_t type,
               vmm_prot_t prot);
bool vmm_alloc_and_map(vmm_vas_t *vas, vaddr_t phys, size_t num_pages,
                       vmm_rgn_type_t type, vmm_prot_t prot);
vaddr_t vmm_free_range(vmm_vas_t *vas, vaddr_t start, size_t num_pages);

bool vmm_query_page(const vmm_vas_t *vas, vaddr_t virt, vmm_pginfo_t *pginfo);

static void prv_vmm_init_kvas(void) {
    kmemset(&g_vmm_kvas, 0, sizeof(vmm_vas_t));

    g_vmm_kvas.arch = vmm_arch_new_vas();

    prv_vmm_rgnlist_init(&g_vmm_kvas.regions);
    prv_vmm_add_boot_regions(&g_vmm_kvas.regions);
    prv_vmm_add_kernel_regions(&g_vmm_kvas.regions);

    LOG_DEBUG("kernel virtual address space regions:");
    prv_vmm_rgnlist_dump(&g_vmm_kvas.regions);

    const bool kvas_ok = prv_vmm_rgnlist_check(&g_vmm_kvas.regions);
    ASSERT(kvas_ok);
}

static void prv_vmm_add_boot_regions(vmm_rgnlist_t *rgns) {
    vaddr_t start;
    vaddr_t end_incl;
    vmm_arch_get_boot_rgn(&start, &end_incl);

    start = VMM_PAGE_ALIGN_DOWN(start);
    end_incl = VMM_PAGE_ALIGN_UP(end_incl + 1) - 1;

    vmm_rgn_t *const boot_rgn =
        alloc_static(&g_vmm_preheap_rgns, sizeof(vmm_rgn_t));
    boot_rgn->start = start;
    boot_rgn->end_incl = end_incl;
    boot_rgn->type = VMM_REGION_BOOT;
    boot_rgn->prot = VMM_PROT_READ | VMM_PROT_WRITE;

    ASSERT(prv_vmm_rgnlist_insert(rgns, boot_rgn));
}

static void prv_vmm_add_kernel_regions(vmm_rgnlist_t *rgns) {
    const vaddr_t start = VMM_PAGE_ALIGN_DOWN(VMM_KERNEL_VMA);
    const vaddr_t end_incl = VMM_PAGE_ALIGN_UP(arch_get_kernel_virt_end()) - 1;

    vmm_rgn_t *const kernel_rgn =
        alloc_static(&g_vmm_preheap_rgns, sizeof(vmm_rgn_t));
    kernel_rgn->start = start;
    kernel_rgn->end_incl = end_incl;
    kernel_rgn->type = VMM_REGION_KERNEL;
    kernel_rgn->prot = VMM_PROT_READ | VMM_PROT_WRITE;

    ASSERT(prv_vmm_rgnlist_insert(rgns, kernel_rgn));
}

static vmm_rgn_t *prv_vmm_new_rgn(void) {
    // FIXME: check if the heap is ready
    vmm_rgn_t *const rgn = alloc_static(&g_vmm_preheap_rgns, sizeof(vmm_rgn_t));
    kmemset(rgn, 0, sizeof(vmm_rgn_t));
    return rgn;
}

static void prv_vmm_free_rgn(vmm_rgn_t *rgn) {
    // FIXME: if 'rng' is not in the static buffer, but on the heap, free it
    (void)rgn;
}

static const char *prv_vmm_rgn_type_name(vmm_rgn_type_t type) {
    switch (type) {
    case VMM_REGION_UNUSED: return "VMM_REGION_UNUSED";
    case VMM_REGION_BOOT:   return "VMM_REGION_BOOT";
    case VMM_REGION_KERNEL: return "VMM_REGION_KERNEL";
    }
    return "<unknown region type>";
}

static void prv_vmm_rgnlist_init(vmm_rgnlist_t *rgns) {
    // The first region covers the whole address space and later gets split into
    // smaller regions.
    vmm_rgn_t *const first_rgn = prv_vmm_new_rgn();
    first_rgn->start = VMM_FIRST_ADDR;
    first_rgn->end_incl = VMM_LAST_ADDR;
    first_rgn->type = VMM_REGION_UNUSED;
    first_rgn->prot = 0;

    list_init(&rgns->list, &first_rgn->node);
}

/**
 * Inserts a region into the region list.
 *
 * Address range belonging to @a rgn must be unused, otherwise the insertion
 * will fail.
 *
 * @returns `true` if the region was inserted, otherwise `false`.
 */
static bool prv_vmm_rgnlist_insert(vmm_rgnlist_t *rgns, vmm_rgn_t *insert_rgn) {
    vmm_rgn_t *const start_rgn =
        prv_vmm_find_rgn_by_addr(rgns, insert_rgn->start);
    vmm_rgn_t *const end_rgn =
        prv_vmm_find_rgn_by_addr(rgns, insert_rgn->end_incl);

    if (start_rgn != end_rgn) {
        LOG_ERROR("cannot insert region 0x%" PRIx0VA "..0x%" PRIx0VA
                  ", it overlaps with two existing regions",
                  insert_rgn->start, insert_rgn->end_incl);
        return false;
    }
    if (start_rgn->type != VMM_REGION_UNUSED) {
        LOG_ERROR("cannot insert region 0x%" PRIx0VA "..0x%" PRIx0VA
                  ", it overlaps with used region",
                  insert_rgn->start, insert_rgn->end_incl);
        return false;
    }
    if (!prv_vmm_check_rgn(insert_rgn)) {
        LOG_ERROR("cannot insert region 0x%" PRIx0VA "..0x%" PRIx0VA
                  ", it failed checks",
                  insert_rgn->start, insert_rgn->end_incl);
        return false;
    }

    vmm_rgn_t *const found_rgn = start_rgn;

    if (found_rgn->start == insert_rgn->start) {
        // Before: [      found_rgn      ]
        // After:  [insert_rgn][found_rgn]
        LOG_FLOW("cut beginning of found region");

        found_rgn->start = insert_rgn->end_incl + 1;

        list_insert_before(&rgns->list, &found_rgn->node, &insert_rgn->node);

        LOG_FLOW("inserted pt 0x%" PRIx0VA "..0x%" PRIx0VA, insert_rgn->start,
                 insert_rgn->end_incl);
        LOG_FLOW("shrunken pt 0x%" PRIx0VA "..0x%" PRIx0VA, found_rgn->start,
                 found_rgn->end_incl);

        return true;
    } else if (found_rgn->end_incl == insert_rgn->end_incl) {
        PANIC("TODO: case when region ends at found region end");
    } else {
        // Before: [           found_rgn           ]
        // After:  [found_rgn][insert_rgn][leftover]
        LOG_FLOW("split found region into three parts");

        vmm_rgn_t *const leftover = prv_vmm_new_rgn();

        leftover->start = insert_rgn->end_incl + 1;
        leftover->end_incl = found_rgn->end_incl;

        found_rgn->end_incl = insert_rgn->start - 1;

        list_insert(&rgns->list, &found_rgn->node, &leftover->node);
        list_insert(&rgns->list, leftover->node.p_prev, &insert_rgn->node);

        LOG_FLOW("first part  0x%" PRIx0VA "..0x%" PRIx0VA, found_rgn->start,
                 found_rgn->end_incl);
        LOG_FLOW("inserted pt 0x%" PRIx0VA "..0x%" PRIx0VA, insert_rgn->start,
                 insert_rgn->end_incl);
        LOG_FLOW("third part  0x%" PRIx0VA "..0x%" PRIx0VA, leftover->start,
                 leftover->end_incl);

        return true;
    }
}

/**
 * Returns the region containing @a virt.
 *
 * @returns Pointer to the region struct if the region was found, never
 * `NULL`.
 */
static vmm_rgn_t *prv_vmm_find_rgn_by_addr(vmm_rgnlist_t *rgns, vaddr_t virt) {
    for (list_node_t *node = rgns->list.p_first_node; node != NULL;
         node = node->p_next) {
        vmm_rgn_t *const rgn = LIST_NODE_TO_STRUCT(node, vmm_rgn_t, node);
        if (rgn->start <= virt && virt <= rgn->end_incl) { return rgn; }
    }

    PANIC("no virtual memory region contains address 0x%" PRIx0VA, virt);
}

static void prv_vmm_rgnlist_dump(const vmm_rgnlist_t *rgns) {
    size_t idx = 0;
    for (list_node_t *node = rgns->list.p_first_node; node != NULL;
         node = node->p_next, idx++) {
        const vmm_rgn_t *const region =
            LIST_NODE_TO_STRUCT(node, const vmm_rgn_t, node);

        LOG_DEBUG("%2zu: [0x%" PRIx0VA "; 0x%" PRIx0VA ") type '%s'", idx,
                  region->start, region->end_incl,
                  prv_vmm_rgn_type_name(region->type));
    }
}

/**
 * Checks that the regions cover the whole AS and are sequential.
 */
static bool prv_vmm_rgnlist_check(const vmm_rgnlist_t *rgns) {
    const vmm_rgn_t *const first_rgn =
        LIST_NODE_TO_STRUCT(rgns->list.p_first_node, const vmm_rgn_t, node);
    if (first_rgn->start != 0) {
        LOG_ERROR("the first virtual memory region starts at 0x%" PRIx0VA
                  " instead of 0",
                  first_rgn->start);
        return false;
    }

    const vmm_rgn_t *prev_rgn = first_rgn;
    for (list_node_t *node = first_rgn->node.p_next; node != NULL;
         node = node->p_next) {
        const vmm_rgn_t *const rgn =
            LIST_NODE_TO_STRUCT(node, const vmm_rgn_t, node);

        if (!prv_vmm_check_rgn(rgn)) { return false; }

        if (rgn->start < prev_rgn->end_incl) {
            LOG_ERROR("region starts at 0x%" PRIx0VA
                      ", which is lower than previous region end 0x%" PRIx0VA,
                      rgn->start, prev_rgn->end_incl);
            return false;
        }
        if (rgn->start != prev_rgn->end_incl + 1) {
            LOG_ERROR("hole between region end 0x%" PRIx0VA
                      " and next region start 0x%" PRIx0VA,
                      prev_rgn->end_incl, rgn->start);
            return false;
        }

        prev_rgn = rgn;
    }

    return true;
}

static bool prv_vmm_check_rgn(const vmm_rgn_t *rgn) {
    if (rgn->start >= rgn->end_incl) {
        LOG_ERROR("invalid region range 0x%" PRIx0VA "..0x%" PRIx0VA,
                  rgn->start, rgn->end_incl);
        return false;
    }
    if (VMM_ADDR_PGOFF(rgn->start) != 0) {
        LOG_ERROR("region start 0x%" PRIx0VA " is not page-aligned",
                  rgn->start);
        return false;
    }
    if (VMM_ADDR_PGOFF(rgn->end_incl + 1) != 0) {
        LOG_ERROR("region end 0x%" PRIx0VA " is not page-aligned",
                  rgn->end_incl + 1);
        return false;
    }

    return true;
}

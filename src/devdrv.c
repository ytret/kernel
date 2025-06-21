/**
 * @file devdrv.c
 * Device enumeration and driver loading.
 */

#include "devdrv.h"
#include "disk/ahci.h"
#include "kprintf.h"
#include "panic.h"
#include "pci.h"

static void prv_devdrv_init_dev(const pci_dev_t *dev);

void devdrv_init(void) {
    pci_init();

    const size_t num_devs = pci_num_devs();
    for (size_t idx_dev = 0; idx_dev < num_devs; idx_dev++) {
        const pci_dev_t *dev = pci_get_dev_const(idx_dev);
        prv_devdrv_init_dev(dev);
    }
}

static void prv_devdrv_init_dev(const pci_dev_t *dev) {
    if (!dev) { panic("prv_devdrv_init_dev: dev = NULL"); }

    if (dev->header.common.base_class == PCI_BASE_CLASS_MASS_STORAGE) {
        if (dev->header.common.sub_class == PCI_MASS_STORAGE_SATA_DPA) {
            if (dev->header.common.interface == PCI_SATA_INTERFACE_AHCI) {
                kprintf(
                    "devdrv: pci %u-%u-%u: SATA DPA, AHBI HBA (major rev. 1)\n",
                    dev->bus_num, dev->dev_num, dev->fun_num);
                if (!ahci_init(dev)) {
                    kprintf("devdrv: pci %u-%u-%u: failed to initialize ahci "
                            "driver\n",
                            dev->bus_num, dev->dev_num, dev->fun_num);
                }
            } else {
                kprintf("devdrv: pci %u-%u-%u: unknown SATA DPA interface %u\n",
                        dev->bus_num, dev->dev_num, dev->fun_num,
                        dev->header.common.interface);
            }

        } else {
            kprintf("devdrv: pci %u-%u-%u: unknown mass storage subclass %u\n",
                    dev->bus_num, dev->dev_num, dev->fun_num,
                    dev->header.common.sub_class);
        }

    } else {
        kprintf("devdrv: pci %u-%u-%u: unknown base class %u\n", dev->bus_num,
                dev->dev_num, dev->fun_num, dev->header.common.base_class);
    }
}

#include "disk/ahci.h"
#include "disk/disk.h"
#include "kprintf.h"

bool disk_is_idle(devdrv_dev_t *dev) {
    switch (dev->driver_id) {
    case DEVDRV_DRIVER_AHCI_PORT:
        return ahci_port_is_idle(dev->driver_ctx);

    default:
        kprintf("disk_is_idle: unknown driver id (%u)\n", dev->driver_id);
        return false;
    }
}

bool disk_start_read(devdrv_dev_t *dev, uint64_t start_sector,
                     uint32_t num_sectors, void *p_buf) {
    switch (dev->driver_id) {
    case DEVDRV_DRIVER_AHCI_PORT:
        return ahci_port_start_read(dev->driver_ctx, start_sector, num_sectors,
                                    p_buf);
    default:
        kprintf("disk_start_read: unknown driver id (%u)\n", dev->driver_id);
        return false;
    }
}

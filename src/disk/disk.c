#include "disk/ahci.h"
#include "disk/disk.h"
#include "kprintf.h"

bool disk_read_sectors(devdrv_dev_t *dev, uint64_t start_sector,
                       uint32_t num_sectors, void *p_buf) {
    if (dev->dev_class != DEVDRV_CLASS_DISK) {
        kprintf("disk_read_sectors: invalid device class (%u)\n",
                dev->dev_class);
        return false;
    }
    switch (dev->driver_id) {
    case DEVDRV_DRIVER_AHCI_PORT:
        return ahci_port_read(dev->driver_ctx, start_sector, num_sectors,
                                 p_buf);
    default:
        kprintf("disk_read_sectors: unknown driver id (%u)\n", dev->driver_id);
        return false;
    }
}

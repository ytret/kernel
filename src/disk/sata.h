/**
 * @file sata.h
 * SATA data structure definitions.
 *
 * Refer to Serial ATA Revision 3.1.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * SATA device signature.
 * This value is read from the @ref reg_port_t.sig "Port Signature register"
 * after device reset.
 */
#define SATA_SIG_ATA 0x00000101

/**
 * @{
 * @name FIS Types
 */
#define SATA_FIS_REG_H2D   0x27 //!< Register - host to device.
#define SATA_FIS_REG_D2H   0x34 //!< Register - device to host.
#define SATA_FIS_DMA_ACT   0x39 //!< DMA Activate - device to host.
#define SATA_FIS_DMA_SETUP 0x41 //!< DMA Setup - bi-directional.
#define SATA_FIS_DATA      0x46 //!< Data - bi-directionial.
#define SATA_FIS_BIST_ACT  0x58 //!< BIST Activate - bi-directional.
#define SATA_FIS_PIO_SETUP 0x5F //!< Port IO Setup - device to host.
#define SATA_FIS_DEV_BITS  0xA1 //!< Set-Device-Bits - device to host.
/// @}

/**
 * @{
 * @name ATA Commands
 */
#define SATA_CMD_IDENTIFY_DEVICE 0xEC
#define SATA_CMD_READ_DMA_EXT    0x25
/// @}

#define SATA_SERIAL_STR_LEN 20

#define SATA_ERROR_ABORT (1 << 2)

/// Register FIS -- host to device.
typedef volatile struct [[gnu::packed]] {
    // 0x00
    /// Set to #SATA_FIS_REG_H2D.
    uint8_t fis_type;
    /// Endpoint address of the target device connected to a Port Multiplier.
    uint8_t pm_port : 4;
    uint8_t _reserved_1 : 3;
    /// Register FIS cause: Command register (1) or Device Control register (0).
    bool b_cmd : 1;
    /// Command register value.
    uint8_t command;
    uint8_t features_7_0;

    // 0x04
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    // 0x08
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t features_15_8;

    // 0x0C
    uint16_t count;
    uint8_t icc;
    /// Device control register value.
    uint8_t control;

    // 0x10
    uint16_t auxiliary;
    uint16_t _reserved_2;
} sata_fis_reg_h2d_t;

typedef volatile struct [[gnu::packed]] {
    // 0x00
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t _reserved_1 : 2;
    bool b_int : 1;
    uint8_t _reserved_2 : 1;
    uint8_t status;
    uint8_t error;

    // 0x04
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    // 0x08
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t _reserved_3;

    // 0x0C
    uint16_t count;
    uint16_t _reserved_4;

    // 0x10
    uint32_t _reserved;
} sata_fis_reg_d2h_t;

typedef volatile struct [[gnu::packed]] {
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint32_t _reserved_1 : 20;
} sata_fis_dma_act_t;

typedef volatile struct [[gnu::packed]] {
    // 0x00
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t _reserved_1 : 1;
    bool b_dir : 1;
    bool b_int : 1;
    bool b_auto_act : 1;
    uint16_t _reserved_2;

    uint32_t dma_buf_id_lo;      // 0x04
    uint32_t dma_buf_id_hi;      // 0x08
    uint32_t _reserved_3;        // 0x0C
    uint32_t dma_buf_offset;     // 0x10
    uint32_t dma_transfer_count; // 0x14
    uint32_t _reserved_4;        // 0x18
} sata_fis_dma_setup_t;

typedef volatile struct [[gnu::packed]] {
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint32_t _reserved : 20;

    uint32_t data[];
} sata_fis_data_t;

typedef volatile struct [[gnu::packed]] {
    // 0x00
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t _reserved_1 : 4;
    uint8_t pattern_def;
    uint8_t _reserved_2;

    uint32_t data_1; // 0x04
    uint32_t data_2; // 0x08
} sata_fis_bist_act_t;

typedef volatile struct [[gnu::packed]] {
    // 0x00
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t _reserved_1 : 1;
    bool b_dir : 1;
    bool b_int : 1;
    uint8_t _reserved_2 : 1;
    uint8_t status;
    uint8_t error;

    // 0x04
    uint8_t lba0;
    uint8_t lba1;
    uint8_t lba2;
    uint8_t device;

    // 0x08
    uint8_t lba3;
    uint8_t lba4;
    uint8_t lba5;
    uint8_t _reserved_3;

    // 0x0C
    uint16_t count;
    uint8_t _reserved_4;
    uint8_t e_status;

    // 0x10
    uint16_t transfer_count;
    uint16_t _reserved_5;
} sata_fis_pio_setup_t;

typedef volatile struct [[gnu::packed]] {
    // 0x00
    uint8_t fis_type;
    uint8_t pm_port : 4;
    uint8_t _reserved_1 : 2;
    bool b_int : 1;
    bool b_notif : 1;
    uint8_t status_2_0 : 3;
    uint8_t _reserved_2 : 1;
    uint8_t status_6_4 : 3;
    uint8_t _reserved_3 : 1;
    uint8_t error;

    // 0x04
    uint32_t prot_spec; // protocol specific
} sata_fis_dev_bits_t;

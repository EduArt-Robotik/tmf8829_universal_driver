/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 * Derived from ams-OSRAM TMF8829 reference drivers (MIT); adapted for portable multi-instance use.
 */

/**
 * @file tmf8829_regs.h
 * @brief Register map, command/status codes, interrupt masks, frame layout, and bootloader constants.
 *
 * Derived from the ams-OSRAM TMF8829 Arduino and Linux reference drivers (MIT-licensed).
 */

#ifndef TMF8829_REGS_H
#define TMF8829_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/**
 * @name Default bus addresses
 * @{
 */
/* ------------------------------------------------------------------ */

/** Default I2C 7-bit slave address. */
#define TMF8829_DEFAULT_I2C_ADDR 0x41u

/** SPI write command prefix byte. */
#define TMF8829_SPI_WR_CMD 0x02u
/** SPI read command prefix byte. */
#define TMF8829_SPI_RD_CMD 0x03u
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Registers in bootloader and application
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_REG_APP_ID   0x00u /**< Currently running application id */
#define TMF8829_REG_CMD_STAT 0x08u /**< Command / status register */
#define TMF8829_REG_PREV_CMD 0x09u /**< Previously executed command */

#define TMF8829_APP_ID__BOOTLOADER  0x80u /**< @ref TMF8829_REG_APP_ID value when bootloader is running */
#define TMF8829_APP_ID__APPLICATION 0x01u /**< @ref TMF8829_REG_APP_ID value when measurement app is running */
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Power, reset, identification
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_REG_I2C_DEVADDR 0xE0u /**< Re-programmable I2C slave address */
#define TMF8829_REG_INT_STATUS  0xE1u /**< Interrupt status (write-1-to-clear) */
#define TMF8829_REG_INT_ENAB    0xE2u /**< Interrupt enable mask */
#define TMF8829_REG_ID          0xE3u /**< Device id (read-only) */
#define TMF8829_REG_REVID       0xE4u /**< Device revision id (read-only) */
#define TMF8829_REG_INTERFACE   0xE9u /**< Active host interface status */
#define TMF8829_REG_GPIO01CFG   0xF1u /**< GPIO0/GPIO1 interface-function select */
#define TMF8829_REG_GPIO23CFG   0xF2u /**< GPIO2/GPIO3 interface-function select */
#define TMF8829_REG_GPIO45CFG   0xF3u /**< GPIO4/GPIO5 interface-function select */
#define TMF8829_REG_GPIO6CFG    0xF4u /**< GPIO6 function select (INT/GPIO6) */
#define TMF8829_REG_RESET       0xF7u /**< Reset register */
#define TMF8829_REG_ENABLE      0xF8u /**< Power / boot / status register */

/* EXPECTED chip id of the TMF8829 sensor */
#define TMF8829_CHIP_ID 0x9Eu /**< Expected value read from @ref TMF8829_REG_ID */

/* RESET register bit masks (write 1 to trigger). */
#define TMF8829_RESET_SOFT_MASK 0x40u /**< bit 6: soft reset */
#define TMF8829_RESET_HARD_MASK 0x80u /**< bit 7: hard reset */

/* ENABLE register bit masks (matches ams-OSRAM register map). */
#define TMF8829_ENABLE_STANDBY_MASK     0x01u /**< bit 0: standby mode */
#define TMF8829_ENABLE_TIMED_STBY_MASK  0x02u /**< bit 1: timed standby mode */
#define TMF8829_ENABLE_PON_MASK         0x04u /**< bit 2: power-on request (PON) */
#define TMF8829_ENABLE_POFF_MASK        0x08u /**< bit 3: power-off request */
#define TMF8829_ENABLE_POWERUP_SHIFT    4u    /**< bits 4-5: powerup_select */
#define TMF8829_ENABLE_POWERUP_MASK     (0x03u << TMF8829_ENABLE_POWERUP_SHIFT)
#define TMF8829_ENABLE_BOOT_NO_PLL_MASK 0x40u /**< bit 6: boot without PLL */
#define TMF8829_ENABLE_CPU_READY_MASK   0x80u /**< bit 7: 1 if CPU is up */

/* GPIOxCFG function-field offsets */
#define TMF8829_GPIO0_FUNC_OFFSET 0u /* TMF8829_REG_GPIO01CFG[3:0]  */
#define TMF8829_GPIO1_FUNC_OFFSET 4u /* TMF8829_REG_GPIO01CFG[7:4]  */
#define TMF8829_GPIO2_FUNC_OFFSET 0u /* TMF8829_REG_GPIO23CFG[3:0]  */
#define TMF8829_GPIO3_FUNC_OFFSET 4u /* TMF8829_REG_GPIO23CFG[7:4]  */
#define TMF8829_GPIO4_FUNC_OFFSET 0u /* TMF8829_REG_GPIO45CFG[3:0]  */
#define TMF8829_GPIO5_FUNC_OFFSET 4u /* TMF8829_REG_GPIO45CFG[7:4]  */
#define TMF8829_GPIO6_FUNC_OFFSET 0u /* TMF8829_REG_GPIO6CFG[3:0]   */

/* GPIOxCFG function values (before shift/mask into field) */
#define TMF8829_GPIO_FUNC_GPIO       0x00u
#define TMF8829_GPIO_FUNC_PERIPHERAL 0x01u

#define TMF8829_GPIO0_FUNC_SPI_MOSI TMF8829_GPIO_FUNC_PERIPHERAL
#define TMF8829_GPIO1_FUNC_SPI_CSN  TMF8829_GPIO_FUNC_PERIPHERAL
#define TMF8829_GPIO2_FUNC_SPI_SCLK TMF8829_GPIO_FUNC_PERIPHERAL
#define TMF8829_GPIO3_FUNC_SPI_MISO TMF8829_GPIO_FUNC_PERIPHERAL
#define TMF8829_GPIO4_FUNC_IxC_SCL  TMF8829_GPIO_FUNC_PERIPHERAL
#define TMF8829_GPIO5_FUNC_IxC_SDA  TMF8829_GPIO_FUNC_PERIPHERAL
#define TMF8829_GPIO6_FUNC_INT      TMF8829_GPIO_FUNC_PERIPHERAL

/* ENABLE.powerup_select values (placed at @ref TMF8829_ENABLE_POWERUP_SHIFT). */
#define TMF8829_POWERUP_NO_OVERRIDE   0u /**< Use fuses (default) */
#define TMF8829_POWERUP_FORCE_BOOTMON 1u /**< Stay in bootmonitor */
#define TMF8829_POWERUP_RAM           2u /**< Run RAM application after AORAM bootrecords */
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Application registers
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_REG_APP_VER_MAJOR   0x01u
#define TMF8829_REG_APP_VER_MINOR   0x02u
#define TMF8829_REG_APP_VER_PATCH   0x04u
#define TMF8829_REG_MEASURE_STATUS  0x05u
#define TMF8829_REG_GPIO_VALUE      0x10u
#define TMF8829_REG_LIVE_BEAT_0     0x1Au
#define TMF8829_REG_LIVE_BEAT_1     0x1Bu
#define TMF8829_REG_SERIAL_NUMBER_0 0x1Cu
#define TMF8829_REG_SERIAL_NUMBER_1 0x1Du
#define TMF8829_REG_SERIAL_NUMBER_2 0x1Eu
#define TMF8829_REG_SERIAL_NUMBER_3 0x1Fu
#define TMF8829_REG_CID_RID         0x20u
#define TMF8829_REG_PAYLOAD         0x21u

/* Configuration page (visible after CMD_LOAD_CONFIG_PAGE) */
#define TMF8829_REG_CFG_PERIOD_MS_LSB              0x22u
#define TMF8829_REG_CFG_PERIOD_MS_MSB              0x23u
#define TMF8829_REG_CFG_KILO_ITER_LSB              0x24u
#define TMF8829_REG_CFG_KILO_ITER_MSB              0x25u
#define TMF8829_REG_CFG_FP_MODE                    0x26u
#define TMF8829_REG_CFG_SPAD_SELECT                0x27u
#define TMF8829_REG_CFG_REF_SPAD_SELECT            0x28u
#define TMF8829_REG_CFG_SPAD_DEADTIME              0x29u
#define TMF8829_REG_CFG_RESULT_FORMAT              0x2Au
#define TMF8829_REG_CFG_DUMP_HISTOGRAMS            0x2Bu
#define TMF8829_REG_CFG_POWER_MODES                0x2Eu
#define TMF8829_REG_CFG_VCSEL_ON                   0x30u
#define TMF8829_REG_CFG_DITHER                     0x31u
#define TMF8829_REG_CFG_VCDRV                      0x32u
#define TMF8829_REG_CFG_VCDRV_2                    0x33u
#define TMF8829_REG_CFG_VCDRV_3                    0x34u
#define TMF8829_REG_CFG_VCSEL_PERIOD_LSB           0x36u
#define TMF8829_REG_CFG_VCSEL_PERIOD_MSB           0x37u
#define TMF8829_REG_CFG_VCDRV_OFFSET_LSB           0x38u
#define TMF8829_REG_CFG_VCDRV_OFFSET_MSB           0x39u
#define TMF8829_REG_CFG_VCDRV_CP                   0x3Au
#define TMF8829_REG_CFG_HISTOGRAM_BINS_LSB         0x40u
#define TMF8829_REG_CFG_HISTOGRAM_BINS_MSB         0x41u
#define TMF8829_REG_CFG_BIN_SHIFT                  0x42u
#define TMF8829_REG_CFG_REF_BIN_SHIFT              0x43u
#define TMF8829_REG_CFG_TDC_OFFSET_LSB             0x44u
#define TMF8829_REG_CFG_TDC_OFFSET_MSB             0x45u
#define TMF8829_REG_CFG_TDC_PRE_PERIODS_LSB        0x46u
#define TMF8829_REG_CFG_TDC_PRE_PERIODS_MSB        0x47u
#define TMF8829_REG_CFG_HV_CP                      0x48u
#define TMF8829_REG_CFG_HA_KILO_ITER_LSB           0x4Au
#define TMF8829_REG_CFG_HA_KILO_ITER_MSB           0x4Bu
#define TMF8829_REG_CFG_ENABLE_DUAL_MODE           0x4Cu
#define TMF8829_REG_CFG_HV_CP_OVERLOAD_DETECT      0x4Du
#define TMF8829_REG_CFG_ALG_PEAK_BINS              0x50u
#define TMF8829_REG_CFG_ALG_REF_PEAK_BINS          0x51u
#define TMF8829_REG_CFG_ALG_DISTANCE               0x52u
#define TMF8829_REG_CFG_ALG_CONFIDENCE_THRESHOLD   0x53u
#define TMF8829_REG_CFG_ALG_HW_PEAK_START          0x57u
#define TMF8829_REG_CFG_ALG_CALIBRATION            0x5Fu
#define TMF8829_REG_CFG_INT_ZONE_MASK_0            0x60u
#define TMF8829_REG_CFG_INT_ZONE_MASK_1            0x61u
#define TMF8829_REG_CFG_INT_ZONE_MASK_2            0x62u
#define TMF8829_REG_CFG_INT_ZONE_MASK_3            0x63u
#define TMF8829_REG_CFG_INT_ZONE_MASK_4            0x64u
#define TMF8829_REG_CFG_INT_ZONE_MASK_5            0x65u
#define TMF8829_REG_CFG_INT_ZONE_MASK_6            0x66u
#define TMF8829_REG_CFG_INT_ZONE_MASK_7            0x67u
#define TMF8829_REG_CFG_INT_LOW_LSB                0x68u
#define TMF8829_REG_CFG_INT_LOW_MSB                0x69u
#define TMF8829_REG_CFG_INT_HIGH_LSB               0x6Au
#define TMF8829_REG_CFG_INT_HIGH_MSB               0x6Bu
#define TMF8829_REG_CFG_INT_PERSISTENCE            0x6Cu
#define TMF8829_REG_CFG_POST_PROCESSING            0x6Du
#define TMF8829_REG_CFG_PROX_DISTANCE              0x6Eu
#define TMF8829_REG_CFG_CROP_TOP_X                 0x70u
#define TMF8829_REG_CFG_CROP_TOP_Y                 0x71u
#define TMF8829_REG_CFG_CROP_BOTTOM_X              0x72u
#define TMF8829_REG_CFG_CROP_BOTTOM_Y              0x73u
#define TMF8829_REG_CFG_INFO_FOV_CORR              0x78u
#define TMF8829_REG_CFG_GPIO_0                     0x80u
#define TMF8829_REG_CFG_GPIO_1                     0x81u
#define TMF8829_REG_CFG_GPIO_2                     0x82u
#define TMF8829_REG_CFG_GPIO_3                     0x83u
#define TMF8829_REG_CFG_GPIO_4                     0x84u
#define TMF8829_REG_CFG_GPIO_5                     0x85u
#define TMF8829_REG_CFG_GPIO_6                     0x86u
#define TMF8829_REG_CFG_GPIO                       0x87u
#define TMF8829_REG_CFG_I2C_ADDRESS                0x90u
#define TMF8829_REG_CFG_MOTION_DETECT_DISTANCE_LSB 0xB0u
#define TMF8829_REG_CFG_MOTION_DETECT_DISTANCE_MSB 0xB1u
#define TMF8829_REG_CFG_MOTION_DETECT_SNR          0xB2u
#define TMF8829_REG_CFG_MOTION_RELEASE_SNR         0xB3u
#define TMF8829_REG_CFG_MOTION_ADJACENT_PIXEL      0xB4u
#define TMF8829_REG_CFG_LAST_AVAILABLE             0xDFu

#define TMF8829_CFG_PAGE_SIZE ((unsigned)(TMF8829_REG_CFG_LAST_AVAILABLE - TMF8829_REG_CFG_PERIOD_MS_LSB + 1u))

/* GPIO output-control values for TMF8829_REG_CFG_GPIO_0 ... _GPIO_6 */
#define TMF8829_GPIO_CTRL_TRISTATE                  0u
#define TMF8829_GPIO_CTRL_INPUT_IR                  1u
#define TMF8829_GPIO_CTRL_INPUT_ACTIVE_HIGH         2u
#define TMF8829_GPIO_CTRL_INPUT_ACTIVE_LOW          3u
#define TMF8829_GPIO_CTRL_OUTPUT_LOW_VCSEL_PULSING  4u
#define TMF8829_GPIO_CTRL_OUTPUT_HIGH_VCSEL_PULSING 5u
#define TMF8829_GPIO_CTRL_OUTPUT_HIGH               6u
#define TMF8829_GPIO_CTRL_OUTPUT_LOW                7u

/* TMF8829_REG_CFG_GPIO pre-delay values */
#define TMF8829_GPIO_PRE_DELAY_NONE   0u
#define TMF8829_GPIO_PRE_DELAY_100_US 1u
#define TMF8829_GPIO_PRE_DELAY_200_US 2u

/* Result-page ancillary registers */
#define TMF8829_REG_FIFO_STATUS 0xFAu
#define TMF8829_REG_SYS_TICK_0  0xFBu
#define TMF8829_REG_SYS_TICK_1  0xFCu
#define TMF8829_REG_SYS_TICK_2  0xFDu
#define TMF8829_REG_SYS_TICK_3  0xFEu
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Application @ref TMF8829_REG_CMD_STAT opcodes and status
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_CMD_MEASURE                 0x10u
#define TMF8829_CMD_CLEAR_STATUS            0x11u
#define TMF8829_CMD_WRITE_PAGE              0x15u
#define TMF8829_CMD_LOAD_CONFIG_PAGE        0x16u
#define TMF8829_CMD_LOAD_CFG_8X8            0x40u
#define TMF8829_CMD_LOAD_CFG_8X8_LONG_RANGE 0x41u
#define TMF8829_CMD_LOAD_CFG_8X8_HIGH_ACC   0x42u
#define TMF8829_CMD_LOAD_CFG_16X16          0x43u
#define TMF8829_CMD_LOAD_CFG_16X16_HIGH_ACC 0x44u
#define TMF8829_CMD_LOAD_CFG_32X32          0x45u
#define TMF8829_CMD_LOAD_CFG_32X32_HIGH_ACC 0x46u
#define TMF8829_CMD_LOAD_CFG_48X32          0x47u
#define TMF8829_CMD_LOAD_CFG_48X32_HIGH_ACC 0x48u
#define TMF8829_CMD_STOP                    0xFFu

/* Application status (response in TMF8829_REG_CMD_STAT) */
#define TMF8829_STAT_OK       0x00u
#define TMF8829_STAT_ACCEPTED 0x01u
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Interrupt status / enable bits
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_INT_RESULTS    0x01u
#define TMF8829_INT_MOTION     0x02u
#define TMF8829_INT_PROXIMITY  0x04u
#define TMF8829_INT_HISTOGRAMS 0x08u

#define TMF8829_INT_ALL (TMF8829_INT_RESULTS | TMF8829_INT_MOTION | TMF8829_INT_PROXIMITY | TMF8829_INT_HISTOGRAMS)
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name FIFO frame layout and identifiers
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_PRE_HEADER_SIZE   5u
#define TMF8829_FRAME_HEADER_SIZE 16u
/** Bytes at start of frame not counted the same way as payload length field. */
#define TMF8829_FRAME_HEADER_OFFSET 4u
#define TMF8829_FRAME_FOOTER_SIZE   12u
#define TMF8829_FRAME_EOF_SIZE      2u

/** End-of-frame marker stored in the last two bytes of every frame. */
#define TMF8829_FRAME_EOF 0xE0F7u

#define TMF8829_FID_MASK       0xF0u
#define TMF8829_FID_RESULTS    0x10u
#define TMF8829_FID_HISTOGRAMS 0x20u

#define TMF8829_FPM_MASK 0x0Fu

/** Bitmask within the sub-id byte that marks histogram sub-frames. */
#define TMF8829_RESULT_FRAME_SUBIDX_MASK 0x40u

/* Frame identifier values delivered to the host */
#define TMF8829_RESULT_ID_MEAS_RES    0xAAu
#define TMF8829_RESULT_ID_MEAS_HIST   0xBBu
#define TMF8829_RESULT_ID_MEAS_HEADER 0xFDu
#define TMF8829_RESULT_ID_ERROR       0xFEu
#define TMF8829_RESULT_ID_NONE        0xFFu

#define TMF8829_RESULT_ERR_EOF     0x01u
#define TMF8829_RESULT_ERR_UNKNOWN 0x00u

/* Result-format mask bits (TMF8829_REG_CFG_RESULT_FORMAT) */
#define TMF8829_RESULT_FORMAT_NR_PEAKS_MASK     0x07u
#define TMF8829_RESULT_FORMAT_SIG_STRENGTH_MASK 0x08u
#define TMF8829_RESULT_FORMAT_NOISE_MASK        0x10u
#define TMF8829_RESULT_FORMAT_XTALK_MASK        0x20u
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Focal-plane resolution modes (@ref TMF8829_REG_CFG_FP_MODE)
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_FP_MODE_8X8A   0u
#define TMF8829_FP_MODE_8X8B   1u
#define TMF8829_FP_MODE_16X16  2u
#define TMF8829_FP_MODE_32X32  3u
#define TMF8829_FP_MODE_32X32S 4u
#define TMF8829_FP_MODE_48X32  5u
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Device limits and clock constant
 * @{
 */
/* ------------------------------------------------------------------ */

/** Device-side FIFO size in bytes. */
#define TMF8829_FIFO_SIZE 8192u

/** Device clock rate (kHz) used for clock-correction math. */
#define TMF8829_TICKS_PER_1000_US 125u
/** @} */

/* ------------------------------------------------------------------ */
/**
 * @name Bootloader / download
 * @{
 */
/* ------------------------------------------------------------------ */

#define TMF8829_REG_FIFO 0xFFu

/* Bootloader-only header registers (valid when TMF8829_REG_APP_ID == TMF8829_APP_ID__BOOTLOADER) */
#define TMF8829_REG_BL_PAYLOAD    0x09u
#define TMF8829_REG_BL_ADDRESS0   0x0Au
#define TMF8829_REG_BL_ADDRESS1   0x0Bu
#define TMF8829_REG_BL_ADDRESS2   0x0Cu
#define TMF8829_REG_BL_ADDRESS3   0x0Du
#define TMF8829_REG_BL_WORD_SIZE0 0x0Eu
#define TMF8829_REG_BL_WORD_SIZE1 0x0Fu

#define TMF8829_BL_CMD_STAT_SPI_OFF       0x20u
#define TMF8829_BL_CMD_STAT_I2C_OFF       0x22u
#define TMF8829_BL_CMD_STAT_ADDR_RAM      0x43u
#define TMF8829_BL_CMD_STAT_W_RAM_BOTH    0x42u
#define TMF8829_BL_CMD_STAT_FIFO_BOTH     0x45u
#define TMF8829_BL_CMD_STAT_START_RAM_APP 0x16u

#define TMF8829_BL_STAT_READY 0x00u

/** Bootloader write-RAM layout: [0]=cmd, [1]=payload_len, [2..]=data */
#define TMF8829_BL_WR_HEADER 2u
/** Max payload bytes per @c W_RAM_BOTH command (ams reference). */
#define TMF8829_BL_MAX_PAYLOAD 128u

/** Default RAM start address for downloadable application image. */
#define TMF8829_FW_IMAGE_LOAD_ADDR_DEFAULT 0x00010000u
/** @} */

/**
 * @name Poll timeouts (milliseconds), overridable at compile time
 * @{
 */

#ifndef TMF8829_BL_CMD_TIMEOUT_MS
#define TMF8829_BL_CMD_TIMEOUT_MS 3u
#endif
#ifndef TMF8829_BL_SET_ADDR_TIMEOUT_MS
#define TMF8829_BL_SET_ADDR_TIMEOUT_MS 3u
#endif
#ifndef TMF8829_BL_W_RAM_TIMEOUT_MS
#define TMF8829_BL_W_RAM_TIMEOUT_MS 3u
#endif
#ifndef TMF8829_BL_W_FIFO_TIMEOUT_MS
#define TMF8829_BL_W_FIFO_TIMEOUT_MS 3u
#endif
#ifndef TMF8829_BL_START_APP_TIMEOUT_MS
#define TMF8829_BL_START_APP_TIMEOUT_MS 3u
#endif

#ifndef TMF8829_APP_CMD_LOAD_CONFIG_TIMEOUT_MS
#define TMF8829_APP_CMD_LOAD_CONFIG_TIMEOUT_MS 3u
#endif
#ifndef TMF8829_APP_CMD_WRITE_CONFIG_TIMEOUT_MS
#define TMF8829_APP_CMD_WRITE_CONFIG_TIMEOUT_MS 3u
#endif
#ifndef TMF8829_APP_CMD_MEASURE_TIMEOUT_MS
#define TMF8829_APP_CMD_MEASURE_TIMEOUT_MS 5u
#endif
#ifndef TMF8829_APP_CMD_STOP_TIMEOUT_MS
#define TMF8829_APP_CMD_STOP_TIMEOUT_MS 25u
#endif
/** @} */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_REGS_H */

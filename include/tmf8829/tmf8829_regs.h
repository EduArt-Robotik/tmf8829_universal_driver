/*
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 tmf8829_universal_driver contributors
 *
 * Register addresses, command opcodes, and frame-layout constants for the
 * ams-OSRAM TMF8829. Values are taken from the public ams-OSRAM TMF8829
 * Arduino / Linux reference drivers (also MIT-licensed).
 */

#ifndef TMF8829_REGS_H
#define TMF8829_REGS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Default bus addresses                                              */
/* ------------------------------------------------------------------ */

/** Default I2C 7-bit slave address. */
#define TMF8829_DEFAULT_I2C_ADDR        0x41u

/** SPI write command prefix byte. */
#define TMF8829_SPI_WR_CMD              0x02u
/** SPI read command prefix byte. */
#define TMF8829_SPI_RD_CMD              0x03u

/* ------------------------------------------------------------------ */
/* Generic registers (visible in both bootloader and app)             */
/* ------------------------------------------------------------------ */

#define TMF8829_REG_APP_ID              0x00u  /**< Currently running application id */
#define TMF8829_REG_CMD_STAT            0x08u  /**< Command / status register */

#define TMF8829_APP_ID__BOOTLOADER      0x80u  /**< @ref TMF8829_REG_APP_ID value when bootloader is running */
#define TMF8829_APP_ID__APPLICATION     0x01u  /**< @ref TMF8829_REG_APP_ID value when measurement app is running */

/* ------------------------------------------------------------------ */
/* Application registers                                              */
/* ------------------------------------------------------------------ */

#define TMF8829_REG_APP_VER_MAJOR       0x01u
#define TMF8829_REG_APP_VER_MINOR       0x02u
#define TMF8829_REG_APP_VER_PATCH       0x04u
#define TMF8829_REG_MEASURE_STATUS      0x05u
#define TMF8829_REG_SERIAL_NUMBER_0     0x1Cu
#define TMF8829_REG_CID_RID             0x20u

/* Configuration page (visible after CMD_LOAD_CONFIG_PAGE) */
#define TMF8829_REG_CFG_PERIOD_MS_LSB   0x22u
#define TMF8829_REG_CFG_PERIOD_MS_MSB   0x23u
#define TMF8829_REG_CFG_KILO_ITER_LSB   0x24u
#define TMF8829_REG_CFG_KILO_ITER_MSB   0x25u
#define TMF8829_REG_CFG_FP_MODE         0x26u
#define TMF8829_REG_CFG_SPAD_DEADTIME   0x29u
#define TMF8829_REG_CFG_RESULT_FORMAT   0x2Au
#define TMF8829_REG_CFG_DUMP_HISTOGRAMS 0x2Bu
#define TMF8829_REG_CFG_VCSEL_ON        0x30u
#define TMF8829_REG_CFG_VCDRV_2         0x33u
#define TMF8829_REG_CFG_VCDRV_3         0x34u
#define TMF8829_REG_CFG_ALG_DISTANCE    0x52u
#define TMF8829_REG_CFG_INT_LOW_LSB     0x68u
#define TMF8829_REG_CFG_INT_LOW_MSB     0x69u
#define TMF8829_REG_CFG_INT_HIGH_LSB    0x6Au
#define TMF8829_REG_CFG_INT_HIGH_MSB    0x6Bu
#define TMF8829_REG_CFG_INT_PERSISTENCE 0x6Cu
#define TMF8829_REG_CFG_LAST_AVAILABLE  0xDFu

#define TMF8829_CFG_PAGE_SIZE \
    ((unsigned)(TMF8829_REG_CFG_LAST_AVAILABLE - TMF8829_REG_CFG_PERIOD_MS_LSB + 1u))

/* Result-page ancillary registers */
#define TMF8829_REG_FIFO_STATUS         0xFAu
#define TMF8829_REG_SYS_TICK_0          0xFBu

/* ------------------------------------------------------------------ */
/* Application command opcodes (written to TMF8829_REG_CMD_STAT)      */
/* ------------------------------------------------------------------ */

#define TMF8829_CMD_MEASURE                       0x10u
#define TMF8829_CMD_CLEAR_STATUS                  0x11u
#define TMF8829_CMD_WRITE_PAGE                    0x15u
#define TMF8829_CMD_LOAD_CONFIG_PAGE              0x16u
#define TMF8829_CMD_LOAD_CFG_8X8                  0x40u
#define TMF8829_CMD_LOAD_CFG_8X8_LONG_RANGE       0x41u
#define TMF8829_CMD_LOAD_CFG_8X8_HIGH_ACC         0x42u
#define TMF8829_CMD_LOAD_CFG_16X16                0x43u
#define TMF8829_CMD_LOAD_CFG_16X16_HIGH_ACC       0x44u
#define TMF8829_CMD_LOAD_CFG_32X32                0x45u
#define TMF8829_CMD_LOAD_CFG_32X32_HIGH_ACC       0x46u
#define TMF8829_CMD_LOAD_CFG_48X32                0x47u
#define TMF8829_CMD_LOAD_CFG_48X32_HIGH_ACC       0x48u
#define TMF8829_CMD_STOP                          0xFFu

/* Application status (response in TMF8829_REG_CMD_STAT) */
#define TMF8829_STAT_OK                           0x00u
#define TMF8829_STAT_ACCEPTED                     0x01u

/* ------------------------------------------------------------------ */
/* Interrupt mask bits                                                */
/* ------------------------------------------------------------------ */

#define TMF8829_INT_RESULTS                       0x01u
#define TMF8829_INT_MOTION                        0x02u
#define TMF8829_INT_PROXIMITY                     0x04u
#define TMF8829_INT_HISTOGRAMS                    0x08u

#define TMF8829_INT_ALL \
    (TMF8829_INT_RESULTS | TMF8829_INT_MOTION | \
     TMF8829_INT_PROXIMITY | TMF8829_INT_HISTOGRAMS)

/* ------------------------------------------------------------------ */
/* Result frame layout                                                */
/* ------------------------------------------------------------------ */

#define TMF8829_PRE_HEADER_SIZE                   5u
#define TMF8829_FRAME_HEADER_SIZE                 16u
#define TMF8829_FRAME_FOOTER_SIZE                 12u
#define TMF8829_FRAME_EOF_SIZE                    2u

/** End-of-frame marker stored in the last two bytes of every frame. */
#define TMF8829_FRAME_EOF                         0xE0F7u

#define TMF8829_FID_MASK                          0xF0u
#define TMF8829_FID_RESULTS                       0x10u
#define TMF8829_FID_HISTOGRAMS                    0x20u

#define TMF8829_FPM_MASK                          0x0Fu

/** Bitmask within the sub-id byte that marks histogram sub-frames. */
#define TMF8829_RESULT_FRAME_SUBIDX_MASK          0x40u

/* Frame identifier values delivered to the host */
#define TMF8829_RESULT_ID_MEAS_RES                0xAAu
#define TMF8829_RESULT_ID_MEAS_HIST               0xBBu
#define TMF8829_RESULT_ID_MEAS_HEADER             0xFDu
#define TMF8829_RESULT_ID_ERROR                   0xFEu
#define TMF8829_RESULT_ID_NONE                    0xFFu

#define TMF8829_RESULT_ERR_EOF                    0x01u
#define TMF8829_RESULT_ERR_UNKNOWN                0x00u

/* Result-format mask bits (TMF8829_REG_CFG_RESULT_FORMAT) */
#define TMF8829_RESULT_FORMAT_NR_PEAKS_MASK       0x07u
#define TMF8829_RESULT_FORMAT_SIG_STRENGTH_MASK   0x08u
#define TMF8829_RESULT_FORMAT_NOISE_MASK          0x10u
#define TMF8829_RESULT_FORMAT_XTALK_MASK          0x20u

/* ------------------------------------------------------------------ */
/* Focal-plane modes                                                  */
/* ------------------------------------------------------------------ */

#define TMF8829_FP_MODE_8X8A                      0u
#define TMF8829_FP_MODE_8X8B                      1u
#define TMF8829_FP_MODE_16X16                     2u
#define TMF8829_FP_MODE_32X32                     3u
#define TMF8829_FP_MODE_32X32S                    4u
#define TMF8829_FP_MODE_48X32                     5u

/* ------------------------------------------------------------------ */
/* Device characteristics                                             */
/* ------------------------------------------------------------------ */

/** Device-side FIFO size in bytes. */
#define TMF8829_FIFO_SIZE                         8192u

/** Device clock rate (kHz) used for clock-correction math. */
#define TMF8829_TICKS_PER_1000_US                 125u

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* TMF8829_REGS_H */

/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: tcc780x.h 21967 2009-07-19 17:31:56Z rob $
 *
 * Copyright (C) 2007 Rob Purchase
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef __TCC780X_H__
#define __TCC780X_H__

/* General-purpose IO */

#define PORTCFG0    (*(volatile unsigned long *)0xF005A000)
#define PORTCFG1    (*(volatile unsigned long *)0xF005A004)
#define PORTCFG2    (*(volatile unsigned long *)0xF005A008)
#define PORTCFG3    (*(volatile unsigned long *)0xF005A00C)

#define GPIOA       (*(volatile unsigned long *)0xF005A020)
#define GPIOB       (*(volatile unsigned long *)0xF005A040)
#define GPIOC       (*(volatile unsigned long *)0xF005A060)
#define GPIOD       (*(volatile unsigned long *)0xF005A080)
#define GPIOE       (*(volatile unsigned long *)0xF005A0A0)

#define GPIOA_DIR   (*(volatile unsigned long *)0xF005A024)
#define GPIOB_DIR   (*(volatile unsigned long *)0xF005A044)
#define GPIOC_DIR   (*(volatile unsigned long *)0xF005A064)
#define GPIOD_DIR   (*(volatile unsigned long *)0xF005A084)
#define GPIOE_DIR   (*(volatile unsigned long *)0xF005A0A4)

#define GPIOA_SET   (*(volatile unsigned long *)0xF005A028)
#define GPIOB_SET   (*(volatile unsigned long *)0xF005A048)
#define GPIOC_SET   (*(volatile unsigned long *)0xF005A068)
#define GPIOD_SET   (*(volatile unsigned long *)0xF005A088)
#define GPIOE_SET   (*(volatile unsigned long *)0xF005A0A8)

#define GPIOA_CLEAR (*(volatile unsigned long *)0xF005A02C)
#define GPIOB_CLEAR (*(volatile unsigned long *)0xF005A04C)
#define GPIOC_CLEAR (*(volatile unsigned long *)0xF005A06C)
#define GPIOD_CLEAR (*(volatile unsigned long *)0xF005A08C)
#define GPIOE_CLEAR (*(volatile unsigned long *)0xF005A0AC)

/* Clock Generator */

#define CLKCTRL    (*(volatile unsigned long *)0xF3000000)
#define PLL0CFG    (*(volatile unsigned long *)0xF3000004)
#define PLL1CFG    (*(volatile unsigned long *)0xF3000008)
#define CLKDIVC    (*(volatile unsigned long *)0xF300000C)
#define CLKDIVC1   (*(volatile unsigned long *)0xF3000010)
#define MODECTR    (*(volatile unsigned long *)0xF3000014)
#define BCLKCTR    (*(volatile unsigned long *)0xF3000018)
#define SWRESET    (*(volatile unsigned long *)0xF300001C)
#define PCLKCFG0   (*(volatile unsigned long *)0xF3000020)
#define PCLK_SDMMC (*(volatile unsigned long *)0xF3000024)
#define PCLKCFG2   (*(volatile unsigned long *)0xF3000028)
#define PCLKCFG3   (*(volatile unsigned long *)0xF300002C)
#define PCLK_LCD   (*(volatile unsigned long *)0xF3000030)
#define PCLKCFG5   (*(volatile unsigned long *)0xF3000034)
#define PCLKCFG6   (*(volatile unsigned long *)0xF3000038)
#define PCLKCFG7   (*(volatile unsigned long *)0xF300003C)
#define PCLKCFG8   (*(volatile unsigned long *)0xF3000040)
#define PCLK_TCT   (*(volatile unsigned long *)0xF3000044)
#define PCLKCFG10  (*(volatile unsigned long *)0xF3000048)
#define PCLKCFG11  (*(volatile unsigned long *)0xF300004C)
#define PCLK_ADC   (*(volatile unsigned long *)0xF3000050)
#define PCLK_DAI   (*(volatile unsigned long *)0xF3000054)
#define PCLKCFG14  (*(volatile unsigned long *)0xF3000058)
#define PCLK_RFREQ (*(volatile unsigned long *)0xF300005C)
#define PCLKCFG16  (*(volatile unsigned long *)0xF3000060)
#define PCLKCFG17  (*(volatile unsigned long *)0xF3000064)

#define PCK_EN (1<<28)

#define CKSEL_PLL0 0
#define CKSEL_PLL1 1
#define CKSEL_XIN  4

/* Device bits for SWRESET & BCLKCTR */

#define DEV_USBD  (1<<1)
#define DEV_LCDC  (1<<2)
#define DEV_SDMMC (1<<6)
#define DEV_NAND  (1<<9)
#define DEV_DAI   (1<<14)
#define DEV_ECC   (1<<16)
#define DEV_RTC   (1<<21)
#define DEV_SDRAM (1<<22)
#define DEV_COP   (1<<23)
#define DEV_ADC   (1<<24)
#define DEV_TIMER (1<<26)
#define DEV_CPU   (1<<27)
#define DEV_IRQ   (1<<28)
#define DEV_MAIN  (1<<31)

/* IRQ Controller */

#define IEN     (*(volatile unsigned long *)0xF3001000)
#define CREQ    (*(volatile unsigned long *)0xF3001004)
#define IRQSEL  (*(volatile unsigned long *)0xF300100C)
#define MREQ    (*(volatile unsigned long *)0xF3001014)
#define POL     (*(volatile unsigned long *)0xF300101C)
#define MIRQ    (*(volatile unsigned long *)0xF3001028)
#define MFIQ    (*(volatile unsigned long *)0xF300102C)
#define TMODE   (*(volatile unsigned long *)0xF3001030)
#define TMODEA  (*(volatile unsigned long *)0xF300103C)
#define ALLMASK (*(volatile unsigned long *)0xF3001044)
#define VAIRQ   (*(volatile unsigned long *)0xF3001080)
#define VAFIQ   (*(volatile unsigned long *)0xF3001084)
#define VNIRQ   (*(volatile unsigned long *)0xF3001088)
#define VNFIQ   (*(volatile unsigned long *)0xF300108C)
#define VCTRL   (*(volatile unsigned long *)0xF3001090)

#define IRQ_PRIORITY_TABLE ((volatile unsigned int *)0xF30010A0)

#define EXT0_IRQ_MASK   (1<<0)
#define EXT3_IRQ_MASK   (1<<3)
#define TIMER0_IRQ_MASK (1<<6)
#define DAI_RX_IRQ_MASK (1<<14)
#define DAI_TX_IRQ_MASK (1<<15)
#define USBD_IRQ_MASK   (1<<21)
#define ADC_IRQ_MASK    (1<<30)

/* Timer / Counters */

/* Note: Timers 0-3 have a 16 bit counter, 4-5 have 20 bits */
#define TCFG0 (*(volatile unsigned int *)(0xF3003000))
#define TCNT0 (*(volatile unsigned int *)(0xF3003004))
#define TREF0 (*(volatile unsigned int *)(0xF3003008))
#define TCFG1 (*(volatile unsigned int *)(0xF3003010))
#define TCNT1 (*(volatile unsigned int *)(0xF3003014))
#define TREF1 (*(volatile unsigned int *)(0xF3003018))
#define TCFG2 (*(volatile unsigned int *)(0xF3003020))
#define TCNT2 (*(volatile unsigned int *)(0xF3003024))
#define TREF2 (*(volatile unsigned int *)(0xF3003028))
#define TCFG3 (*(volatile unsigned int *)(0xF3003030))
#define TCNT3 (*(volatile unsigned int *)(0xF3003034))
#define TREF3 (*(volatile unsigned int *)(0xF3003038))
#define TCFG4 (*(volatile unsigned int *)(0xF3003040))
#define TCNT4 (*(volatile unsigned int *)(0xF3003044))
#define TREF4 (*(volatile unsigned int *)(0xF3003048))
#define TCFG5 (*(volatile unsigned int *)(0xF3003050))
#define TCNT5 (*(volatile unsigned int *)(0xF3003054))
#define TREF5 (*(volatile unsigned int *)(0xF3003058))

#define TIREQ     (*(volatile unsigned long *)0xF3003060)

/* TCFG flags */
#define TCFG_EN    (1<<0)   /* enable timer */
#define TCFG_CONT  (1<<1)   /* continue from zero once TREF is reached */
#define TCFG_PWM   (1<<2)   /* PWM mode */
#define TCFG_IEN   (1<<3)   /* IRQ enable */
#define TCFG_SEL   (1<<4)   /* clock source & divider */
#define TCFG_POL   (1<<7)   /* polarity */
#define TCFG_CLEAR (1<<8)   /* reset TCNT to zero */
#define TCFG_STOP  (1<<9)   /* stop counting once TREF reached */

/* TIREQ flags */
#define TIREQ_TI0  (1<<0)   /* Timer N IRQ flag */
#define TIREQ_TI1  (1<<1)
#define TIREQ_TI2  (1<<2)
#define TIREQ_TI3  (1<<3)
#define TIREQ_TI4  (1<<4)
#define TIREQ_TI5  (1<<5)

#define TIREQ_TF0  (1<<8)   /* Timer N reference value reached */
#define TIREQ_TF1  (1<<9)
#define TIREQ_TF2  (1<<10)
#define TIREQ_TF3  (1<<11)
#define TIREQ_TF4  (1<<12)
#define TIREQ_TF5  (1<<13)

#define TC32EN     (*(volatile unsigned long *)0xF3003080)
#define TC32LDV    (*(volatile unsigned long *)0xF3003084)
#define TC32MCNT   (*(volatile unsigned long *)0xF3003094)
#define TC32IRQ    (*(volatile unsigned long *)0xF3003098)

/* ADC */

#define ADCCON     (*(volatile unsigned long *)0xF3004000)
#define ADCDATA    (*(volatile unsigned long *)0xF3004004)
#define ADCCONA    (*(volatile unsigned long *)0xF3004080)
#define ADCSTATUS  (*(volatile unsigned long *)0xF3004084)
#define ADCCFG     (*(volatile unsigned long *)0xF3004088)

/* Memory Controller */

#define SDCFG      (*(volatile unsigned long *)0xF1000000)
#define SDFSM      (*(volatile unsigned long *)0xF1000004)
#define MCFG       (*(volatile unsigned long *)0xF1000008)
#define CSCFG0     (*(volatile unsigned long *)0xF1000010)
#define CSCFG1     (*(volatile unsigned long *)0xF1000014)
#define CSCFG2     (*(volatile unsigned long *)0xF1000018)
#define CSCFG3     (*(volatile unsigned long *)0xF100001C)
#define CLKCFG     (*(volatile unsigned long *)0xF1000020)
#define SDCMD      (*(volatile unsigned long *)0xF1000024)

#define SDCFG1     (*(volatile unsigned long *)0xF1001000)
#define MCFG1      (*(volatile unsigned long *)0xF1001008)

/* DAI */

#define DADO_L0     (*(volatile unsigned long *)0xF0059020)
#define DADO_R0     (*(volatile unsigned long *)0xF0059024)
#define DADO_L1     (*(volatile unsigned long *)0xF0059028)
#define DADO_R1     (*(volatile unsigned long *)0xF005902c)
#define DADO_L2     (*(volatile unsigned long *)0xF0059030)
#define DADO_R2     (*(volatile unsigned long *)0xF0059034)
#define DADO_L3     (*(volatile unsigned long *)0xF0059038)
#define DADO_R3     (*(volatile unsigned long *)0xF005903c)
#define DADO_L(_x_) (*(volatile unsigned int *)(0xF0059020+8*(_x_)))
#define DADO_R(_x_) (*(volatile unsigned int *)(0xF0059024+8*(_x_)))
#define DAMR        (*(volatile unsigned long *)0xF0059040)
#define DAVC        (*(volatile unsigned long *)0xF0059044)

/* Misc */

#define ECFG0      (*(volatile unsigned long *)0xF300500C)
#define MBCFG      (*(volatile unsigned long *)0xF3005020)

#define TCC780_VER (*(volatile unsigned long *)0xE0001FFC)

/* NAND Flash Controller */

#define NFC_CMD    (*(volatile unsigned long *)0xF0053000)
#define NFC_SADDR  (*(volatile unsigned long *)0xF005300C)
#define NFC_SDATA  (*(volatile unsigned long *)0xF0053040)
#define NFC_WDATA  (*(volatile unsigned long *)0xF0053010)
#define NFC_CTRL   (*(volatile unsigned long *)0xF0053050)
    #define NFC_16BIT (1<<26)
    #define NFC_CS0   (1<<23)
    #define NFC_CS1   (1<<22)
    #define NFC_READY (1<<20)
#define NFC_DMA_WR (*(volatile unsigned long *)0xF0053054)
#define NFC_DMA_RD (*(volatile unsigned long *)0xF0053058)
#define NFC_DMA_SZ (*(volatile unsigned long *)0xF005305C)
#define NFC_IREQ   (*(volatile unsigned long *)0xF0053060)
#define NFC_RST    (*(volatile unsigned long *)0xF0053064)

/* ECC Controller */

#define ECC_CTRL       (*(volatile unsigned long *)0xF005B000)
    #define ECC_ENC       (1<<27)
    #define ECC_READY     (1<<26)
    #define ECC_M4EN      (1<<6)
#define ECC_BASE       (*(volatile unsigned long *)0xF005B004)
#define ECC_CLR        (*(volatile unsigned long *)0xF005B00C)
#define MLC_ECC0W      (*(volatile unsigned long *)0xF005B030)
#define MLC_ECC1W      (*(volatile unsigned long *)0xF005B034)
#define MLC_ECC2W      (*(volatile unsigned long *)0xF005B038)
#define MLC_ECC0R      (*(volatile unsigned long *)0xF005B040)
#define MLC_ECC1R      (*(volatile unsigned long *)0xF005B044)
#define MLC_ECC2R      (*(volatile unsigned long *)0xF005B048)
#define ECC_ERRADDR(x) (*(volatile unsigned long *)(0xF005B050+4*(x)))
#define ECC_ERRDATA(x) (*(volatile unsigned long *)(0xF005B060+4*(x)))
#define ECC_ERR_NUM    (*(volatile unsigned long *)0xF005B070)

/* SD/MMC Controller */

#define SDICLK      (*(volatile unsigned long *)0xF0058004)
#define SDIARGU     (*(volatile unsigned long *)0xF0058008)
#define SDICMD      (*(volatile unsigned long *)0xF005800C)
#define SDIRSPCMD   (*(volatile unsigned long *)0xF0058010)
#define SDIRSPARGU0 (*(volatile unsigned long *)0xF0058014)
#define SDIRSPARGU1 (*(volatile unsigned long *)0xF0058018)
#define SDIRSPARGU2 (*(volatile unsigned long *)0xF005801C)
#define SDIRSPARGU3 (*(volatile unsigned long *)0xF0058020)
#define SDIDTIMER   (*(volatile unsigned long *)0xF0058024)
#define SDIDCTRL2   (*(volatile unsigned long *)0xF0058028)
#define SDIDCTRL    (*(volatile unsigned long *)0xF005802C)
#define SDISTATUS   (*(volatile unsigned long *)0xF0058030)
#define SDIIFLAG    (*(volatile unsigned long *)0xF0058034)
#define SDIWDATA    (*(volatile unsigned long *)0xF0058038)
#define SDIRDATA    (*(volatile unsigned long *)0xF005803C)
#define SDIIENABLE  (*(volatile unsigned long *)0xF0058040)

#define SDICMD_RES_TYPE1  1
#define SDICMD_RES_TYPE1b 2
#define SDICMD_RES_TYPE2  3
#define SDICMD_RES_TYPE3  4
#define SDICMD_RES_TYPE4  5
#define SDICMD_RES_TYPE5  6
#define SDICMD_RES_TYPE6  7

#define SDISTATUS_RESP_RCVD      (1<<7)
#define SDISTATUS_FIFO_LOAD_REQ  (1<<17)
#define SDISTATUS_FIFO_FETCH_REQ (1<<18)
#define SDISTATUS_CMD_PATH_RDY   (1<<21)
#define SDISTATUS_MULTIBLOCK_END (1<<25)

/* Timer frequency */
/* Timer is based on PCK_TCT (set to 2Mhz in system.c) */
#define TIMER_FREQ (2000000)

#endif

#define LCDC_CTRL    (*(volatile unsigned long *)0xF0000000)
#define LCDC_CLKDIV  (*(volatile unsigned long *)0xF0000008)
#define LCDC_HTIME1  (*(volatile unsigned long *)0xF000000C)
#define LCDC_HTIME2  (*(volatile unsigned long *)0xF0000010)
#define LCDC_VTIME1  (*(volatile unsigned long *)0xF0000014)
#define LCDC_VTIME2  (*(volatile unsigned long *)0xF0000018)
#define LCDC_VTIME3  (*(volatile unsigned long *)0xF000001C)
#define LCDC_VTIME4  (*(volatile unsigned long *)0xF0000020)
#define LCDC_DS      (*(volatile unsigned long *)0xF000005C)
#define LCDC_I1CTRL  (*(volatile unsigned long *)0xF000008C)
#define LCDC_I1POS   (*(volatile unsigned long *)0xF0000090)
#define LCDC_I1SIZE  (*(volatile unsigned long *)0xF0000094)
#define LCDC_I1BASE  (*(volatile unsigned long *)0xF0000098)
#define LCDC_I1OFF   (*(volatile unsigned long *)0xF00000A8)
#define LCDC_I1SCALE (*(volatile unsigned long *)0xF00000AC)
#define MMU_REG0     (*(volatile unsigned long *)0xf7000000)
#define MMU_REG1     (*(volatile unsigned long *)0xf7000004)
#define MMU_REG2     (*(volatile unsigned long *)0xf7000008)
#define MMU_REG3     (*(volatile unsigned long *)0xf700000C)
#define MMU_REG4     (*(volatile unsigned long *)0xf7000010)
#define MMU_REG5     (*(volatile unsigned long *)0xf7000014)
#define MMU_REG6     (*(volatile unsigned long *)0xf7000018)
#define MMU_REG7     (*(volatile unsigned long *)0xf700001C)
#define MMU_REG8     (*(volatile unsigned long *)0xf7000020)
#define IRQ_PRIO1      ((volatile unsigned int *)0xF30010A4)
#define IRQ_PRIO2      ((volatile unsigned int *)0xF30010A8)
#define IRQ_PRIO3      ((volatile unsigned int *)0xF30010AC)
#define IRQ_PRIO4      ((volatile unsigned int *)0xF30010B0)
#define IRQ_PRIO5      ((volatile unsigned int *)0xF30010B4)
#define IRQ_PRIO6      ((volatile unsigned int *)0xF30010B8)
#define IRQ_PRIO7      ((volatile unsigned int *)0xF30010BC)

#define USB_NUM_ENDPOINTS  3
#define USB_DEVBSS_ATTR    IBSS_ATTR
/* USB 2.0 device registers */
#define TCC7xx_USB_PHY_CFG       ((volatile unsigned short*)0xf00100c4)
    #define TCC7xx_USB_PHY_CFG_XSEL  (1<<13) /* FS/HS Transceiver enable */
    #define TCC7xx_USB_PHY_CFG_DWS   (1<<6)  /* Host mode */
    #define TCC7xx_USB_PHY_XO        (1<<5)  /* Enable XO_OUT */
    #define TCC7xx_USB_PHY_CKSEL_12  0
    #define TCC7xx_USB_PHY_CKSEL_24  1
    #define TCC7xx_USB_PHY_CKSEL_48  2
/* USB 2.0 device registers */
#define TCC7xx_USB_INDEX    ((volatile unsigned short*)0xf0010000) /* Endpoint Index register */
#define TCC7xx_USB_EPIF     ((volatile unsigned short*)0xf0010004) /* Endpoint interrupt flag register */
#define TCC7xx_USB_EPIE     ((volatile unsigned short*)0xf0010008) /* Endpoint interrupt enable register */
#define TCC7xx_USB_FUNC     ((volatile unsigned short*)0xf001000c) /* Function address register */
#define TCC7xx_USB_EP_DIR   ((volatile unsigned short*)0xf0010014) /* Endpoint direction register */
#define TCC7xx_USB_TST      ((volatile unsigned short*)0xf0010014) /* Test registerregister */
#define TCC7xx_USB_SYS_STAT ((volatile unsigned short*)0xf001001c) /* System status register */
    #define TCC7xx_USB_SYS_STAT_RESET   (1<<0) /* Host forced reced */
    #define TCC7xx_USB_SYS_STAT_SUSPEND (1<<1) /* Host forced suspend */
    #define TCC7xx_USB_SYS_STAT_RESUME  (1<<2) /* Host forced resume */
    #define TCC7xx_USB_SYS_STAT_HIGH    (1<<4) /* High speed */
    #define TCC7xx_USB_SYS_STAT_SPD_END (1<<6) /* Speed detection end */
    #define TCC7xx_USB_SYS_STAT_VBON    (1<<8)
    #define TCC7xx_USB_SYS_STAT_VBOF    (1<<9)
    #define TCC7xx_USB_SYS_STAT_EOERR   (1<<10) /* overrun error */
    #define TCC7xx_USB_SYS_STAT_DCERR   (1<<11) /* Data CRC error */
    #define TCC7xx_USB_SYS_STAT_TCERR   (1<<12) /* Token CRC error */
    #define TCC7xx_USB_SYS_STAT_BSERR   (1<<13) /* Bit-stuff error */
    #define TCC7xx_USB_SYS_STAT_TMERR   (1<<14) /* Timeout error */
    #define TCC7xx_USB_SYS_STAT_BAERR   (1<<15) /* Byte align error */
#define TCC7xx_USB_SYS_STAT_ERRORS (TCC7xx_USB_SYS_STAT_EOERR | \
                                    TCC7xx_USB_SYS_STAT_DCERR | \
                                    TCC7xx_USB_SYS_STAT_TCERR | \
                                    TCC7xx_USB_SYS_STAT_BSERR | \
                                    TCC7xx_USB_SYS_STAT_TMERR | \
                                    TCC7xx_USB_SYS_STAT_BAERR)
#define TCC7xx_USB_SYS_CTRL ((volatile unsigned short*)0xf0010020) /* System control register */
    #define TCC7xx_USB_SYS_CTRL_RESET   (1<<0) /* Reset enable */
    #define TCC7xx_USB_SYS_CTRL_SUSPEND (1<<1) /* Suspend enable */
    #define TCC7xx_USB_SYS_CTRL_RESUME  (1<<2) /* Resume enable */
    #define TCC7xx_USB_SYS_CTRL_IPS     (1<<4) /* Interrupt polarity */
    #define TCC7xx_USB_SYS_CTRL_RFRE    (1<<5) /* Reverse read data enable */
    #define TCC7xx_USB_SYS_CTRL_SPDEN   (1<<6) /* Speed detection interrupt enable */
    #define TCC7xx_USB_SYS_CTRL_BUS16   (1<<7) /* Select bus width 8/16 */
    #define TCC7xx_USB_SYS_CTRL_EIEN    (1<<8) /* Error interrupt enable */
    #define TCC7xx_USB_SYS_CTRL_RWDE    (1<<9) /* Reverse write data enable */
    #define TCC7xx_USB_SYS_CTRL_VBONE   (1<<10) /* VBus On enable */
    #define TCC7xx_USB_SYS_CTRL_VBOFE   (1<<11) /* VBus Off enable */
    #define TCC7xx_USB_SYS_CTRL_DUAL    (1<<12) /* Dual interrupt enable*/
    #define TCC7xx_USB_SYS_CTRL_DMAZ    (1<<14) /* DMA total count zero int */
#define TCC7xx_USB_EP0_STAT ((volatile unsigned short*)0xf0010024) /* EP0 status register */
#define TCC7xx_USB_EP0_CTRL ((volatile unsigned short*)0xf0010028) /* EP0 control register */
#define TCC7xx_USB_EP0_BUF  ((volatile unsigned short*)0xf0010060) /* EP0 buffer register */
#define TCC7xx_USB_EP1_BUF  ((volatile unsigned short*)0xf0010064) /* EP1 buffer register */
#define TCC7xx_USB_EP2_BUF  ((volatile unsigned short*)0xf0010068) /* EP2 buffer register */
#define TCC7xx_USB_EP3_BUF  ((volatile unsigned short*)0xf001006c) /* EP3 buffer register */
/* Indexed registers,  write endpoint number to TCC7xx_USB_INDEX */
#define TCC7xx_USB_EP_STAT  ((volatile unsigned short*)0xf001002c) /* EP status register */
    #define TCC7xx_USP_EP_STAT_RPS   (1 << 0) /* Packet received */
    #define TCC7xx_USP_EP_STAT_TPS   (1 << 1) /* Packet transmited */
    #define TCC7xx_USP_EP_STAT_LWO   (1 << 4) /* Last word odd */
#define TCC7xx_USB_EP_CTRL  ((volatile unsigned short*)0xf0010030) /* EP control register */
    #define TCC7xx_USB_EP_CTRL_TZLS  (1 << 0)  /* TX Zero Length Set */
    #define TCC7xx_USB_EP_CTRL_ESS   (1 << 1)  /* Endpoint Stall Set */
    #define TCC7xx_USB_EP_CTRL_CDP   (1 << 2)  /* Clear Data PID */
    #define TCC7xx_USB_EP_CTRL_TTE   (1 << 5)  /* TX Toggle Enable */
    #define TCC7xx_USB_EP_CTRL_FLUSH (1 << 6)  /* Flush FIFO */
    #define TCC7xx_USB_EP_CTRL_DUEN  (1 << 7)  /* Dual FIFO Mode */
    #define TCC7xx_USB_EP_CTRL_IME   (1 << 8)  /* ISO Mode */
    #define TCC7xx_USB_EP_CTRL_OUTHD (1 << 11) /* OUT Packet Hold */
    #define TCC7xx_USB_EP_CTRL_INHLD (1 << 12) /* IN Packet Hold */
#define TCC7xx_USB_EP_BRCR        ((volatile unsigned short*)0xf0010034) /* EP byte read count register */
#define TCC7xx_USB_EP_BWCR        ((volatile unsigned short*)0xf0010038) /* EP byte write count register */
#define TCC7xx_USB_EP_MAXP        ((volatile unsigned short*)0xf001003c) /* EP max packet register */
#define TCC7xx_USB_EP_DMA_CTRL    ((volatile unsigned short*)0xf0010040) /* EP DMA control register */
#define TCC7xx_USB_EP_DMA_TCNTR   ((volatile unsigned short*)0xf0010044) /* EP DMA transfer counter register */
#define TCC7xx_USB_EP_DMA_FCNTR   ((volatile unsigned short*)0xf0010048) /* EP DMA fifo counter register */
#define TCC7xx_USB_EP_DMA_TTCNTR1 ((volatile unsigned short*)0xf001004c) /* EP DMA total trasfer counter1 register */
#define TCC7xx_USB_EP_DMA_TTCNTR2 ((volatile unsigned short*)0xf0010050) /* EP DMA total trasfer counter2 register */
#define TCC7xx_USB_EP_DMA_ADDR1   ((volatile unsigned short*)0xf00100a0) /* EP DMA MCU addr1 register */
#define TCC7xx_USB_EP_DMA_ADDR2   ((volatile unsigned short*)0xf00100a4) /* EP DMA MCU addr2 register */
#define TCC7xx_USB_EP_DMA_STAT    ((volatile unsigned short*)0xf00100c0) /* EP DMA Transfer Status register */
#define TCC7xx_USB_DELAY_CTRL     ((volatile unsigned short*)0xf0010080)	/* Delay control register */

#define DMA_SRC   ((volatile unsigned int*)0xf0040200)
#define DMA_SINC  ((volatile unsigned int*)0xf0040204)
#define DMA_DES   ((volatile unsigned int*)0xf0040210)
#define DMA_DINC  ((volatile unsigned int*)0xf0040214)
#define DMA_REG8  ((volatile unsigned int*)0xf0040220)
#define DMA_REG9  ((volatile unsigned int*)0xf0040224)

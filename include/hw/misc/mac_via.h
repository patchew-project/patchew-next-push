/*
 *
 * Copyright (c) 2011-2018 Laurent Vivier
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 */

#ifndef HW_MISC_MAC_VIA_H
#define HW_MISC_MAC_VIA_H
#define TYPE_MAC_VIA "mac_via"
#define MAC_VIA(obj)   OBJECT_CHECK(MacVIAState, (obj), TYPE_MAC_VIA)

/* VIA1 */

#define VIA1_IRQ_ONE_SECOND_BIT 0
#define VIA1_IRQ_VBLANK_BIT     1
#define VIA1_IRQ_ADB_READY_BIT  2
#define VIA1_IRQ_ADB_DATA_BIT   3
#define VIA1_IRQ_ADB_CLOCK_BIT  4

#define VIA1_IRQ_NB             8

#define VIA1_IRQ_ONE_SECOND (1 << VIA1_IRQ_ONE_SECOND_BIT)
#define VIA1_IRQ_VBLANK     (1 << VIA1_IRQ_VBLANK_BIT)
#define VIA1_IRQ_ADB_READY  (1 << VIA1_IRQ_ADB_READY_BIT)
#define VIA1_IRQ_ADB_DATA   (1 << VIA1_IRQ_ADB_DATA_BIT)
#define VIA1_IRQ_ADB_CLOCK  (1 << VIA1_IRQ_ADB_CLOCK_BIT)

/* VIA2 */

#define VIA2_IRQ_SCSI_DATA_BIT  (VIA1_IRQ_NB + 0)
#define VIA2_IRQ_SLOT_BIT       (VIA1_IRQ_NB + 1)
#define VIA2_IRQ_UNUSED_BIT     (VIA1_IRQ_NB + 2)
#define VIA2_IRQ_SCSI_BIT       (VIA1_IRQ_NB + 3)
#define VIA2_IRQ_ASC_BIT        (VIA1_IRQ_NB + 4)

#define VIA2_IRQ_NB             8

#define VIA2_IRQ_SCSI_DATA  (1 << VIA2_IRQ_SCSI_DATA_BIT)
#define VIA2_IRQ_SLOT       (1 << VIA2_IRQ_SLOT_BIT)
#define VIA2_IRQ_UNUSED     (1 << VIA2_IRQ_SCSI_BIT)
#define VIA2_IRQ_SCSI       (1 << VIA2_IRQ_UNUSED_BIT)
#define VIA2_IRQ_ASC        (1 << VIA2_IRQ_ASC_BIT)
#endif

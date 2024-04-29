#include <kern/pci.h>
#include <inc/assert.h>

#ifndef JOS_KERN_E1000_H
#define JOS_KERN_E1000_H



/* TX Descriptor Registers */
#define E1000_TDBAL    0x03800  /* TX Descriptor Ring Base Address Low - RW */
#define E1000_TDBAH    0x03804  /* TX Descriptor Ring Base Address High - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descripotr Tail - RW */

/* TX Descriptor Misc */
#define TD_MAX         16       /* Maximum number of TX Descriptor */
#define TD_SIZE        16       /* Each TD has 16 bytes */

/* Transmit Control Registers Address */
#define E1000_TCTL     0x00400  /* TX Control - RW */
#define E1000_TCTL_EXT 0x00404  /* Extended TX Control - RW */
#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

/* Transmit Control BITS*/
#define E1000_TCTL_RST    0x00000001    /* software reset */
#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_BCE    0x00000004    /* busy check enable */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */
#define E1000_TCTL_SWXOFF 0x00400000    /* SW Xoff transmission */
#define E1000_TCTL_PBE    0x00800000    /* Packet Burst Enable */
#define E1000_TCTL_RTLC   0x01000000    /* Re-transmit on late collision */
#define E1000_TCTL_NRTU   0x02000000    /* No Re-transmit on underrun */
#define E1000_TCTL_MULR   0x10000000    /* Multiple request support */

/* E1000 Virtual Memory Mapping */
#define E1000_TDBASE  0x9000000000
#define E1000_PACKET  0x9000001000

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

int transmit_packet(void* buffer, int length);

#endif	// JOS_KERN_E1000_H

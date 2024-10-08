#include <inc/x86.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <kern/pci.h>
#include <kern/pcireg.h>
#include <kern/pmap.h>
#include <kern/e1000.h>

// Flag to do "lspci" at bootup
static int pci_show_devs = 0;
static int pci_show_addrs = 0;

// PCI "configuration mechanism one"
static uint32_t pci_conf1_addr_ioport = 0x0cf8;
static uint32_t pci_conf1_data_ioport = 0x0cfc;

// Forward declarations
static int pci_bridge_attach(struct pci_func *pcif);
static int pci_e1000_attach(struct pci_func *pcif);

//
volatile uint32_t*  e1000_viraddr;
volatile physaddr_t ring_buffers[TD_MAX];
volatile physaddr_t ring_buffers_receive[RD_MAX];

// PCI driver table
struct pci_driver {
	uint32_t key1, key2;
	int (*attachfn) (struct pci_func *pcif);
};

// pci_attach_class matches the class and subclass of a PCI device
struct pci_driver pci_attach_class[] = {
	{ PCI_CLASS_BRIDGE, PCI_SUBCLASS_BRIDGE_PCI, &pci_bridge_attach },
	{ 0, 0, 0 },
};

// pci_attach_vendor matches the vendor ID and device ID of a PCI device
struct pci_driver pci_attach_vendor[] = { {0x8086, 0x100E, &pci_e1000_attach},
	{ 0, 0, 0 },
};

static void
pci_conf1_set_addr(uint32_t bus,
		   uint32_t dev,
		   uint32_t func,
		   uint32_t offset)
{
	assert(bus < 256);
	assert(dev < 32);
	assert(func < 8);
	assert(offset < 256);
	assert((offset & 0x3) == 0);

	uint32_t v = (1 << 31) |		// config-space
		(bus << 16) | (dev << 11) | (func << 8) | (offset);
	outl(pci_conf1_addr_ioport, v);
}

static uint32_t
pci_conf_read(struct pci_func *f, uint32_t off)
{
	pci_conf1_set_addr(f->bus->busno, f->dev, f->func, off);
	return inl(pci_conf1_data_ioport);
}

static void
pci_conf_write(struct pci_func *f, uint32_t off, uint32_t v)
{
	pci_conf1_set_addr(f->bus->busno, f->dev, f->func, off);
	outl(pci_conf1_data_ioport, v);
}

static int __attribute__((warn_unused_result))
pci_attach_match(uint32_t key1, uint32_t key2,
		 struct pci_driver *list, struct pci_func *pcif)
{
	uint32_t i;

	for (i = 0; list[i].attachfn; i++) {
		if (list[i].key1 == key1 && list[i].key2 == key2) {
			int r = list[i].attachfn(pcif);
			if (r > 0)
				return r;
			if (r < 0)
				cprintf("pci_attach_match: attaching "
					"%x.%x (%p): e\n",
					key1, key2, list[i].attachfn, r);
		}
	}
	return 0;
}

static int
pci_attach(struct pci_func *f)
{
	return
		pci_attach_match(PCI_CLASS(f->dev_class),
				 PCI_SUBCLASS(f->dev_class),
				 &pci_attach_class[0], f) ||
		pci_attach_match(PCI_VENDOR(f->dev_id),
				 PCI_PRODUCT(f->dev_id),
				 &pci_attach_vendor[0], f);
}

static const char *pci_class[] =
{
	[0x0] = "Unknown",
	[0x1] = "Storage controller",
	[0x2] = "Network controller",
	[0x3] = "Display controller",
	[0x4] = "Multimedia device",
	[0x5] = "Memory controller",
	[0x6] = "Bridge device",
};

static void
pci_print_func(struct pci_func *f)
{
	const char *class = pci_class[0];
	if (PCI_CLASS(f->dev_class) < sizeof(pci_class) / sizeof(pci_class[0]))
		class = pci_class[PCI_CLASS(f->dev_class)];

	cprintf("PCI: %02x:%02x.%d: %04x:%04x: class: %x.%x (%s) irq: %d\n",
		f->bus->busno, f->dev, f->func,
		PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id),
		PCI_CLASS(f->dev_class), PCI_SUBCLASS(f->dev_class), class,
		f->irq_line);
}

static int
pci_scan_bus(struct pci_bus *bus)
{
	int totaldev = 0;
	struct pci_func df;
	memset(&df, 0, sizeof(df));
	df.bus = bus;

	for (df.dev = 0; df.dev < 32; df.dev++) {
		uint32_t bhlc = pci_conf_read(&df, PCI_BHLC_REG);
		if (PCI_HDRTYPE_TYPE(bhlc) > 1)	    // Unsupported or no device
			continue;

		totaldev++;

		struct pci_func f = df;
		for (f.func = 0; f.func < (PCI_HDRTYPE_MULTIFN(bhlc) ? 8 : 1);
		     f.func++) {
			struct pci_func af = f;

			af.dev_id = pci_conf_read(&f, PCI_ID_REG);
			if (PCI_VENDOR(af.dev_id) == 0xffff)
				continue;

			uint32_t intr = pci_conf_read(&af, PCI_INTERRUPT_REG);
			af.irq_line = PCI_INTERRUPT_LINE(intr);

			af.dev_class = pci_conf_read(&af, PCI_CLASS_REG);
			if (pci_show_devs)
				pci_print_func(&af);
			pci_attach(&af);
		}
	}

	return totaldev;
}

static int
pci_bridge_attach(struct pci_func *pcif)
{
	uint32_t ioreg  = pci_conf_read(pcif, PCI_BRIDGE_STATIO_REG);
	uint32_t busreg = pci_conf_read(pcif, PCI_BRIDGE_BUS_REG);

	if (PCI_BRIDGE_IO_32BITS(ioreg)) {
		cprintf("PCI: %02x:%02x.%d: 32-bit bridge IO not supported.\n",
			pcif->bus->busno, pcif->dev, pcif->func);
		return 0;
	}

	struct pci_bus nbus;
	memset(&nbus, 0, sizeof(nbus));
	nbus.parent_bridge = pcif;
	nbus.busno = (busreg >> PCI_BRIDGE_BUS_SECONDARY_SHIFT) & 0xff;

	if (pci_show_devs)
		cprintf("PCI: %02x:%02x.%d: bridge to PCI bus %d--%d\n",
			pcif->bus->busno, pcif->dev, pcif->func,
			nbus.busno,
			(busreg >> PCI_BRIDGE_BUS_SUBORDINATE_SHIFT) & 0xff);

	pci_scan_bus(&nbus);
	return 1;
}

static int
pci_e1000_attach(struct pci_func *pcif)
{
	// Enables the device; set pcif->reg_base[0], pcif->reg_size[0] physical address. 
	pci_func_enable(pcif);
	// Memory Map the physical address into MMIO
	e1000_viraddr = mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	// testing device status register
	if(*(volatile int*)((int64_t)e1000_viraddr + 0x8) != 0x80080783) {
		panic("mmio error, something got fucked\n");
	}
	// Transmit Initialization, see 14.5 in Intel's manual
	// Allocate Memory; its reference count is not updated.
	// If more than one page, MAKE SURE THEY ARE CONTIGUOUS!
	struct PageInfo* tdring_page = page_alloc(1);
	physaddr_t phyaddr = page2pa(tdring_page);
	page_insert(boot_pml4e, tdring_page, (void*) E1000_TDBASE_TRANSMIT, PTE_P|PTE_W); // Only kernel should have access
	// Map into virtual Address
	for(int i = 0; i < TD_MAX; i++){
		struct PageInfo* new_packet_page = page_alloc(1);
		ring_buffers[i] = page2pa(new_packet_page);
		page_insert(boot_pml4e, new_packet_page, (void*) (E1000_PACKET + i*4096), PTE_P|PTE_W); // Only kernel should have access
	}
	// PACKET TRANSMISSION INITIALIZATION
	// We support a maximum of 64 Transmit Descriptors as required by Auto Grader.
	*(volatile int64_t*)((int64_t)e1000_viraddr + E1000_TDBAL) = phyaddr;
	*(volatile int*)((int64_t)e1000_viraddr + E1000_TDLEN) = TD_MAX * TD_SIZE;
	*(volatile int*)((int64_t)e1000_viraddr + E1000_TDH) = 0;
	*(volatile int*)((int64_t)e1000_viraddr + E1000_TDT) = 0;
	// Set up TCTL; TCTL register is at 0x400 from Base
	*(volatile int*)((int64_t)e1000_viraddr + E1000_TCTL) = 
		*(volatile int*)((int64_t)e1000_viraddr + E1000_TCTL) 
		| E1000_TCTL_EN  // set bit to 1 according to manual
		| E1000_TCTL_PSP // Set bit to 1 according to manual 
		| 0x00000100     // E1000_TCTL_CT  0x00000ff0  collision threshold , ff means that 8 bits are for this configuration, set to 10 by standard
		| 0x00200000;    // E1000_TCTL_COLD, assume full duplex.

	*(volatile int*)((int64_t)e1000_viraddr + E1000_TIPG) =  
		(6 << 20)    // IPGR2 is set to 6 by 802.3 Standard
		+ (4 << 10)  // IPGR1 is set to 2/3 of IPGR2, which is 4
		+ (10);      // IPGT is set to 10


	// PACKET RECEIVE INITIALIZATION
	struct PageInfo* tdring_page_receive = page_alloc(1);
	physaddr_t phyaddr_receive = page2pa(tdring_page_receive);
	page_insert(boot_pml4e, tdring_page_receive, (void*) E1000_TDBASE_RECEIVE, PTE_P|PTE_W); // Only kernel should have access
	struct rx_desc* rds = (struct rx_desc*) E1000_TDBASE_RECEIVE;

	for(int i = 0; i < RD_MAX; i++){
		struct PageInfo* new_receive_page = page_alloc(1);
		rds[i].addr = page2pa(new_receive_page);
		page_insert(boot_pml4e, new_receive_page, (void*) (E1000_PACKET_RECEIVE + i*4096), PTE_P|PTE_W); // Only kernel should have access
	}

	*(volatile int64_t*)((int64_t)e1000_viraddr + E1000_RA) = 
	(0x52ll) 
	| (0x54ll << 8)
	| (0x00ll << 16)
	| (0x12ll << 24)
	| (0x34ll << 32)
	| (0x56ll << 40)
	| ((int64_t)E1000_RAH_AV << 32);
	*(volatile int64_t*)((int64_t)e1000_viraddr + E1000_RDBAL) = phyaddr_receive;
	*(volatile int*)((int64_t)e1000_viraddr + E1000_RDLEN) = RD_MAX * RD_SIZE;
	*(volatile int*)((int64_t)e1000_viraddr + E1000_RDH) = 5;
	// If H == T, the whole thing just shut down.
	*(volatile int*)((int64_t)e1000_viraddr + E1000_RDT) = 4;
	*(volatile int*)((int64_t)e1000_viraddr + E1000_MTA) = 0;
	// *(volatile int*)((int64_t)e1000_viraddr + E1000_IMS) = 
	// 	*(volatile int*)((int64_t)e1000_viraddr + E1000_IMS) 
	// 	| E1000_IMS_RXT0 
	// 	| E1000_IMS_RXO 
	// 	| E1000_IMS_RXDMT0  
	// 	| E1000_IMS_RXSEQ 
	// 	| E1000_IMS_LSC;    

	// Put this to last, can only be enabled after receive ring is initialized and ready
	*(volatile int*)((int64_t)e1000_viraddr + E1000_RCTL) = 
		(*(volatile int*)((int64_t)e1000_viraddr + E1000_RCTL) 
		| E1000_RCTL_EN    // set bit to 1 according to manual
		| E1000_RCTL_BAM    // allow broadcast
		| E1000_RCTL_BSEX
		| E1000_RCTL_SZ_4096
		| E1000_RCTL_SECRC) 
		& (~0x00000C00)
		& (~E1000_RCTL_LPE);
	// cprintf("%llx %llx", E1000_RCTL, *(volatile int*)((int64_t)e1000_viraddr + E1000_RCTL));
	// panic("");

	
	return 1;
}

// External PCI subsystem interface

void
pci_func_enable(struct pci_func *f)
{
	pci_conf_write(f, PCI_COMMAND_STATUS_REG,
		       PCI_COMMAND_IO_ENABLE |
		       PCI_COMMAND_MEM_ENABLE |
		       PCI_COMMAND_MASTER_ENABLE);

	uint32_t bar_width;
	uint32_t bar;
	for (bar = PCI_MAPREG_START; bar < PCI_MAPREG_END;
	     bar += bar_width)
	{
		uint32_t oldv = pci_conf_read(f, bar);

		bar_width = 4;
		pci_conf_write(f, bar, 0xffffffff);
		uint32_t rv = pci_conf_read(f, bar);

		if (rv == 0)
			continue;

		int regnum = PCI_MAPREG_NUM(bar);
		uint32_t base, size;
		if (PCI_MAPREG_TYPE(rv) == PCI_MAPREG_TYPE_MEM) {
			if (PCI_MAPREG_MEM_TYPE(rv) == PCI_MAPREG_MEM_TYPE_64BIT)
				bar_width = 8;

			size = PCI_MAPREG_MEM_SIZE(rv);
			base = PCI_MAPREG_MEM_ADDR(oldv);
			if (pci_show_addrs)
				cprintf("  mem region %d: %d bytes at 0x%x\n",
					regnum, size, base);
		} else {
			size = PCI_MAPREG_IO_SIZE(rv);
			base = PCI_MAPREG_IO_ADDR(oldv);
			if (pci_show_addrs)
				cprintf("  io region %d: %d bytes at 0x%x\n",
					regnum, size, base);
		}

		pci_conf_write(f, bar, oldv);
		f->reg_base[regnum] = base;
		f->reg_size[regnum] = size;

		if (size && !base)
			cprintf("PCI device %02x:%02x.%d (%04x:%04x) "
				"may be misconfigured: "
				"region %d: base 0x%x, size %d\n",
				f->bus->busno, f->dev, f->func,
				PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id),
				regnum, base, size);
	}

	cprintf("PCI function %02x:%02x.%d (%04x:%04x) enabled\n",
		f->bus->busno, f->dev, f->func,
		PCI_VENDOR(f->dev_id), PCI_PRODUCT(f->dev_id));
}

int
pci_init(void)
{
	static struct pci_bus root_bus;
	memset(&root_bus, 0, sizeof(root_bus));

	return pci_scan_bus(&root_bus);
}

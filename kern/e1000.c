#include <kern/e1000.h>
#include <kern/pci.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <kern/pmap.h>
#include <inc/memlayout.h>

// LAB 6: Your driver code here
int transmit_packet(void* buffer, int length) {
    int tail = *(volatile int*)(e1000_viraddr + E1000_TDT);
    volatile struct tx_desc* tdbase = ((volatile struct tx_desc*)(E1000_TDBASE));
    struct tx_desc td = tdbase[tail];
    
    if(td.cmd & (1 << 3)) {
        if(td.status & (1 << 0)) {
            td.status &= (~(1 << 0));
            td.addr = E1000_PACKET + tail * PGSIZE;
            td.length = length;
            memcpy((void*)td.addr, buffer, PGSIZE);
        } else {
            return -1; // -1 means buffer full
        } 
    } else {
        td.cmd |= (1 << 3); // set RS bit
    }
    tdbase[tail] = td;
    *(volatile int*)(e1000_viraddr + E1000_TDT) = tail + 1;
    return 0;
}
#include <kern/e1000.h>
#include <kern/pci.h>
#include <inc/assert.h>
#include <inc/string.h>
#include <kern/pmap.h>
#include <inc/memlayout.h>
#include <kern/sched.h>

// LAB 6: Your driver code here
int transmit_packet(void* buffer, int length) {
    int tail = *(volatile int*)((int64_t)e1000_viraddr + E1000_TDT);
    cprintf("tail is %llx\n", tail);
    volatile struct tx_desc* tdbase = ((volatile struct tx_desc*)(E1000_TDBASE_TRANSMIT));
    struct tx_desc td = tdbase[tail];
    cprintf("buffer contains %llx\n", *(int64_t*)(buffer));
    while(true){
        if(td.cmd & (1 << 3)) {
            if(td.status & (1 << 0)) {
                td.status &= (~(1 << 0));
                td.addr = ring_buffers[tail];
                td.length = length;
                memmove((void*)(E1000_PACKET + tail * PGSIZE), buffer, length);
                break;
            } else {
                sched_yield();
                continue;
            } 
        } else {
            td.cmd |= (1 << 3) | (1 << 0); // set RS bit
            td.status &= (~(1 << 0));
            td.addr = ring_buffers[tail];
            td.length = length;
            memmove((void*)(E1000_PACKET + tail * PGSIZE), buffer, length);
            break;
        }
    }

    tdbase[tail] = td;
    if((tail + 1) == TD_MAX)
        *(volatile int*)((int64_t)e1000_viraddr + E1000_TDT) = 0;
    else
        *(volatile int*)((int64_t)e1000_viraddr + E1000_TDT) = tail + 1;

    return 0;
}

int receive_packet(void* buffer) {
    int tail = *(volatile int*)((int64_t)e1000_viraddr + E1000_RDT);
    int curr = (tail + 1) % RD_MAX;
    // cprintf("tail is %llx\n", tail);
    volatile struct rx_desc* rdbase = ((volatile struct rx_desc*)(E1000_TDBASE_RECEIVE));
    struct rx_desc rd = rdbase[curr];
    // cprintf("addr of buffer is %llx\n", buffer);
    // cprintf("addr of receive side is %llx\n", rd.addr);

    // if the memory is owned by hardware, software CANNOT access it
    if((rd.status & (1 << 0)) ) {
        memcpy(buffer, (void*)(E1000_PACKET_RECEIVE + curr * PGSIZE), rd.length);
        rd.status &= (~(3 << 0));
    } else {
        return -99;
    } 
    

    rdbase[curr] = rd;
    if((tail + 1) == RD_MAX)
        *(volatile int*)((int64_t)e1000_viraddr + E1000_RDT) = 0;
    else
        *(volatile int*)((int64_t)e1000_viraddr + E1000_RDT) = tail + 1;
    if(rd.length < 0) {
        panic("");
    }

    return rd.length;
}
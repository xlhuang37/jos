#include "ns.h"

extern union Nsipc nsipcbuf;

    void
input(envid_t ns_envid)
{
    binaryname = "ns_input";

    // LAB 6: Your code here:
    // 	- read a packet from the device driver
    //	- send it to the network server
    // Hint: When you IPC a page to the network server, it will be
    // reading from it for a while, so don't immediately receive
    // another packet in to the same physical page.

    // must use nsipc here
    // Also union type doing weird shit
    int r;
    sys_page_alloc(thisenv->env_id, (void*)(0x408000), PTE_P|PTE_W|PTE_U);
    while(true) {
        while(true) {
            r = sys_receive_packet((void*)(0x408000));
            if(r == -99){
                sys_yield();
                continue;
            } else if(r >= 0) {
                break;
            } else {
                panic("");
            }      
        }

        sys_page_alloc(thisenv->env_id, &nsipcbuf, PTE_P|PTE_W|PTE_U);

        memmove(nsipcbuf.pkt.jp_data, (void*)(0x408000), r);
        nsipcbuf.pkt.jp_len = r;

        ipc_send(ns_envid, NSREQ_INPUT, &nsipcbuf, PTE_P|PTE_W|PTE_U);

        // probably maybe i don't need the following; new allocation is on different physical page everytime?
        // but the returned page is always first on the list..
        // for better robustness, maybe use sys_time
        for(int i = 0; i < 10; i++){
            sys_yield();
        }
    }  
}

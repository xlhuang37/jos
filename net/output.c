#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
    binaryname = "ns_output";

    // LAB 6: Your code here:
    // 	- read a packet from the network server
    //	- send the packet to the device driver
    while(true){ 
        sys_ipc_recv(&nsipcbuf);
        sys_send_packet((void*)nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len);
    }
    
}

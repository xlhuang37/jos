## JOS Implementation
This is my implementation of JOS, which was done during UNC's COMP630 - OS Implementation. I believe that JOS was originally used at MIT, which was taken and modified by my professor, Don Porter. While the bulk of our JOS is the same as MIT's, a notable difference is that professor Porter's version uses a 4-way page table that supports 64bit memory address. Other than that, a few labs have slightly different exercises.

### Lab 1: Booting and a simple stack backtracker
We are tasked to write an interesting stack backtrace function that complies with DWARF2 debugging format. Other than that, it was basically a walkthrough of JOS booting process. 

### Lab 2: Virtual Memory
Setting up a 64bit 4-layer page table that conforms with x86-64's specification. Specifically, I initialized page out of physical memory, and then enabled virtual memory by setting up page table. I implemented page table walk functions and mechanisms for allocating/de-allocating pages in our virtual address space. Finally, we usesd lcr3 instruction to load the top level page directory into our CPU.

### Lab 3: Processes and Interrupt
In the first part of the lab, we deal with processes. We set up address space for each process, allocated memory, and supported loading binary into our process. The second part of the lab introduces interrupt handling. Essentially we just populate Interrupt Descriptor Table with pointers to some assembly code that we wrote. The assembly code stores the current register state and then redirects the execution to trap handling function, where we decipher the type of interrupt and handle accordingly. We are specifically implemented the handling of system call, achieving user space and kernel separation for the first time. 

### Lab 4: Context Switching and fork
Building atop the interrupt handling code from lab 3, I implemented the handling of context-switching interrupt, which allows our OS to run multiple processes! With context switching enabled, I went on to build fork and a rudimentary IPC mechanism. 

### Lab 5: File System 
The bulk of the code was provided, and my job was just to make sense of the starter code and fill in some tiny blanks. Firstly, we implemented some necessary data structure for the specified file system abstraction. Then, we build up a process that serves the file system functionalities, which will be interacted with through the IPC mechanism we built from the previous lab. Finally, we enabled interrupt for keyboard!

### Final lab: Network Driver
I opted to do the network driver for final project. First, we find and configure the E1000 network card using PCI interface. Then, we allocate virtual address for the physical memory mapping that the network card acquired. After that, we pretty much that fill in bits while reading the manual to configure transmit and receive. 

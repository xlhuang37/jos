## JOS Implementation
This is my implementation of JOS, which was done during UNC's COMP630 - OS Implementation. I believe that JOS was originally used at MIT, which was taken and modified by my professor, Don Porter. While the bulk of our JOS is the same as MIT's, a notable difference is that professor Porter's version uses a 4-way page table that supports 64bit memory address. Other than that, a few labs have slightly different exercises.

### Lab 1
We are tasked to write an interesting stack backtrace function that complies with DWARF2 debugging format. Other than that, it was basically a walkthrough of JOS booting process. 

### Lab 2
Setting up a 64bit 4-layer page table that conforms with x86-64's specification. Specifically, I initialized page out of physical memory, and then enabled virtual memory by setting up page table. I implemented page table walk functions and mechanisms for allocating/de-allocating pages in our virtual address space. Finally, we usesd lcr3 instruction to load the top level page directory into our CPU.

### Lab 3
In the first part of the lab, we deal with processes. We set up address space for each process, allocated memory, and supported loading binary into our process. The second part of the lab introduces interrupt handling. Essentially we just populate Interrupt Descriptor Table with pointers to some assembly code that we wrote. The assembly code stores the current register state and then redirects the execution to trap handling function, where we decipher the type of interrupt and handle accordingly. We are specifically implemented the handling of system call, achieving user space and kernel separation for the first time. 

### Lab 4

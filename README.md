Omamori
=======

A hobbyist OS that I'm developing.

What works right now:
   *  Interrupts (HW and SW)
   *  Physical / Virtual memory allocation
   *  Paging
   *  Kernel heap allocation
   *  Task switching (right now, it only runs tasks in kmode)
   *  In-kernel IPC (message based, basically consists of passing around pointers to kernel memory)
   *  The PS/2 controller and keyboard drivers
   *  VGA console driver
   *  In-kernel libc
   *  In-kernel lua integration
   
What I'm planning on implementing (at some point, and in a rough order):
   *  ATA drivers
   *  Filesystems
   *  User-mode programs
   *  Graphics / a GUI
   *  Networking
   *  Extensive documentation
   
   
I'm making most of the design up as I go along, and mostly just implementing what seems like the easiest choice.
Of course, I'm also looking for challenges as well, so...
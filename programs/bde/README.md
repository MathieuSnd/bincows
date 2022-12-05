# Bincows Desktop Environment

this program is called by /bin/init 
and drives the Bincows Desktop mode.

It mmaps the framebuffer to its virtual memory, 
and writes to it.

It shares memory with every GUI application on /mem/
describing virtual frame buffers.
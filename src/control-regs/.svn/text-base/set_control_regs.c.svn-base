#include <linux/module.h>
#include <linux/kernel.h>

__inline__
void read_cr124 () {
  u32 cr0, cr2, cr3, cr4;
  #ifdef __x86_64__
  __asm__ __volatile__ (
            "mov %%cr0, %%rax\n\t"
	            "mov %%eax, %0\n\t"
	            "mov %%cr2, %%rax\n\t"
	            "mov %%eax, %1\n\t"
	            "mov %%cr4, %%rax\n\t"
	            "mov %%eax, %2\n\t"
	    : "=m" (cr0), "=m" (cr2), "=m" (cr4)
	    : /* no input */
	    : "%rax"
    );
#elif defined(__i386__)
  __asm__ __volatile__ (
            "mov %%cr0, %%eax\n\t"
	            "mov %%eax, %0\n\t"
	            "mov %%cr2, %%eax\n\t"
	            "mov %%eax, %1\n\t"
	            "mov %%cr3, %%eax\n\t"
	            "mov %%eax, %2\n\t"
	    : "=m" (cr0), "=m" (cr2), "=m" (cr4)
	    : /* no input */
	    : "%eax"
    );
#endif


  printk(KERN_INFO "cr0 = 0x%8.8X\n", cr0);
  printk(KERN_INFO "cr2 = 0x%8.8X\n", cr2);
  printk(KERN_INFO "cr4 = 0x%8.8X\n", cr4);
  int pce_bit = (cr4 & (1<<8))>>8;
  printk(KERN_INFO "CR4.PCE (8)bit = %u\n", pce_bit);
}

int init_module(void)
{
  read_cr124();  
  
  printk(KERN_ALERT "Setting CR4.PCE(0x100) bit to 1\n");
  __asm__("push   %rax\n\t"
	  "mov    %cr4,%rax;\n\t"
	  "or     $(1 << 8),%rax;\n\t"
	  "mov    %rax,%cr4;\n\t"
	  "wbinvd\n\t"
	  "pop    %rax"
    );
  
  read_cr124();
  
  return 0;
}

void cleanup_module(void)
{
}


#pragma once

#include <stdint.h>

struct cpuid_regs {
    uint32_t eax, ebx, ecx, edx;
};

void cpuid(uint32_t eax, struct cpuid_regs* out_regs);


// doc from 

/**
 * 
Basic CPUID Information
Initial EAX Value	Register	Information Provided about the Processor
0H	EAX	Maximum Input Value for Basic CPUID Information (see second table)
-	EBX	"Genu"
-	ECX	"ntel"
-	EDX	"ineI"
01H	EAX	Version Information: Type, Family, Model, and Stepping ID
-	EBX	Bits 7-0: Brand Index
-	-	Bits 15-8: CLFLUSH line size (Value . 8 = cache line size in bytes)
-	-	Bits 23-16: Number of logical processors per physical processor; two for the Pentium 4
                 processor supporting Hyper-Threading Technology
-	-	Bits 31-24: Local APIC ID
-	ECX	Extended Feature Information (see fourth table)
-	EDX	Feature Information (see fifth table)
02H	EAX	Cache and TLB Information (see sixth table)
-	EBX	Cache and TLB Information
-	ECX	Cache and TLB Information
-	EDX	Cache and TLB Information
03H	ECX	Bits 00-31 of 96 bit processor serial number. (Available in Pentium III processor only;
                 otherwise, the value in this register is reserved.)
-	EDX	Bits 32-63 of 96 bit processor serial number. (Available in Pentium III processor only;
                 otherwise, the value in this register is reserved.)
-	-	NOTE: Processor serial number (PSN) is not supported in the Pentium 4 processor or later.
             On all models, use the PSN flag (returned using CPUID) to check for PSN support before
             accessing the feature. See AP-485, Intel Processor Identification and the CPUID Instruction 
             (Order Number 241618) for more information on PSN.
04H	EAX	Bits 4-0: Cache Type**
-	-	Bits 7-5: Cache Level (starts at 1)
-	-	Bits 8: Self Initializing cache level (does not need SW initialization)
-	-	Bits 9: Fully Associative cache
-	-	Bits 13-10: Reserved
-	-	Bits 25-14: Number of threads sharing this cache*
-	-	Bits 31-26: Number of processor cores on this die (Multicore)*
-	EBX	Bits 11-00: L = System Coherency Line Size*
-	-	Bits 21-12: P = Physical Line partitions*
-	-	Bits 31-22: W = Ways of associativity*
-	ECX	Bits 31-00: S = Number of Sets*
-	EDX	Reserved = 0
-	-	0 = Null - No more caches
-	-	1 = Data Cache
-	-	2 = Instruction Cache
-	-	3 = Unified Cache
-	-	4-31 = Reserved
-	-	NOTE: CPUID leaves > 3 < 80000000 are only visible when IA32_CR_MISC_ENABLES.BOOT_NT4 (bit 
                22) is clear (Default)
5H	EAX	Bits 15-00: Smallest monitor-line size in bytes (default is processor's monitor granularity)
-	EBX	Bits 15-00: Largest monitor-line size in bytes (default is processor's monitor granularity)

*Add one to the value in the register file to get the number. For example, the number of processor cores is EAX[31:26]+1.
** Cache Types fields



Extended Function CPUID Information
Initial EAX Value	Register	Information Provided about the Processor
80000000H	EAX	Maximum Input Value for Extended Function CPUID Information (see second table).
-	        EBX	Reserved
-	        ECX	Reserved
-	        EDX	Reserved
80000001H	EAX	Extended Processor Signature and Extended Feature Bits. (Currently reserved)
-	        EBX	Reserved
-	        ECX	Reserved
-	        EDX	Reserved
80000002H	EAX	Processor Brand String
-	        EBX	Processor Brand String Continued
-	        ECX	Processor Brand String Continued
-	        EDX	Processor Brand String Continued
80000003H	EAX	Processor Brand String Continued
-	        EBX	Processor Brand String Continued
-	        ECX	Processor Brand String Continued
-	        EDX	Processor Brand String Continued
80000004H	EAX	Processor Brand String Continued
-	        EBX	Processor Brand String Continued
-	        ECX	Processor Brand String Continued
-	        EDX	Processor Brand String Continued
80000005H	EAX	Reserved = 0
-	        EBX	Reserved = 0
-	        ECX	Reserved = 0
-	        EDX	Reserved = 0
80000006H	EAX	Reserved = 0
-	        EBX	Reserved = 0
-	        ECX	Bits 0-7: Cache Line Size
-	        -	Bits 15-12: L2 Associativity
-	        -	Bits 31-16: Cache size in 1K units
-	        EDX	Reserved = 0
-	80000007H EAX	Reserved = 0
-	        EBX	Reserved = 0
-	        ECX	Reserved = 0
-	        EDX	Reserved = 0
80000008H	EAX	Reserved = 0
-	        EBX	Reserved = 0
-	        ECX	Reserved = 0
-	        EDX	Reserved = 0
*/

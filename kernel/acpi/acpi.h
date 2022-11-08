#pragma once


// scan acpi tables
// retrive APIC, PCIE and HPET information
// it should be called early to print
// potential early panics
void acpi_early_scan(const void* rsdp_location);

// this function should be called when 
// paging and malloc are initialized
// and acpi_scan_clean should be called 
// afterwards
// This function needs paging and 
void acpi_init(void);
void acpi_scan_clean(void);


// this function maps HPET and ACPI register spaces
// according to vmap.h
int acpi_map_mmio(void);


// Returns the (virtual) address of the c-th table that 
// has the given signature,or NULL when no such table was found.
//
// this function must be called after acpi_scan(...)
// and before acpi_scan_clean()
const void* acpi_get_table(const char * signature, size_t c);


void map_acpi_mmios(void);



#pragma once


// list acpi tables
// this function should be called when 
// paging and malloc are initialized
// and acpi_scan_clean should be called 
// afterwards
// It also parse PCIE and HPET tables
void acpi_scan(const void* rsdp_location);
void acpi_scan_clean(void);


// Returns the (virtual) address of the c-th table that 
// has the given signature,or NULL when no such table was found.
//
// this function must be called after acpi_scan(...)
// and before acpi_scan_clean()
const void* acpi_get_table(const char * signature, size_t c);


void map_acpi_mmios(void);



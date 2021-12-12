#pragma once

/*

    read RSDP, XSDP, MADT, FADT
*/
void read_acpi_tables(const void* rsdp_location);
const struct XSDT* get_xsdt_location(void);

void map_acpi_mmios(void);
#pragma once

/*

    read RSDP, XSDP, MADT, FADT
*/
void read_acpi_tables(const void* rsdp_location);

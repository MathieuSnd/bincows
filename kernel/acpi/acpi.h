#pragma once

/*

    read RSDP, XSDP, MADT, FADT
*/
void read_acpi_tables(void* rsdp_location);

#pragma once


/**
 * panic_string can be NULL
 * 
 * doesn't return: freezes
 * dumps the registers and print debug infos
 */
__attribute__((noreturn)) void panic(const char* panic_string);
#ifndef CTYPES_H
#define CTYPES_H

#ifdef __cplusplus
extern "C" {
#endif


int isalnum(int c);

// This function checks whether the passed character is alphabetic.
int isalpha(int c);
// This function checks whether the passed character is control character.
int iscntrl(int c);

// This function checks whether the passed character is decimal digit.
int isdigit(int c);

// This function checks whether the passed character has graphical representation using locale.
int isgraph(int c);

// This function checks whether the passed character is lowercase letter.
int islower(int c);

// This function checks whether the passed character is printable.
int isprint(int c);

// This function checks whether the passed character is a punctuation character.
int ispunct(int c);

// This function checks whether the passed character is white-space.
int isspace(int c);

// This function checks whether the passed character is an uppercase letter.
int isupper(int c);

// This function checks whether the passed character is a hexadecimal digit.
int isxdigit(int c);

int tolower(int c);

int toupper(int c);

int isascii(int c);


#ifdef __cplusplus
}
#endif

#endif // CTYPES_H
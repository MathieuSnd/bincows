#ifndef CTYPES_H
#define CTYPES_H


extern int isalnum(int c);

// This function checks whether the passed character is alphabetic.
extern int isalpha(int c);
// This function checks whether the passed character is control character.
extern int iscntrl(int c);

// This function checks whether the passed character is decimal digit.
extern int isdigit(int c);

// This function checks whether the passed character has graphical representation using locale.
extern int isgraph(int c);

// This function checks whether the passed character is lowercase letter.
extern int islower(int c);

// This function checks whether the passed character is printable.
extern int isprint(int c);

// This function checks whether the passed character is a punctuation character.
extern int ispunct(int c);

// This function checks whether the passed character is white-space.
extern int isspace(int c);

// This function checks whether the passed character is an uppercase letter.
extern int isupper(int c);

// This function checks whether the passed character is a hexadecimal digit.
extern int isxdigit(int c);

extern int tolower(int c);

extern int toupper(int c);


#endif // CTYPES_H
#include <string.h>
#include "sprintf.h"


#define SIGN(X) X > 0 ? 1 : -1
#define ARG(TYPE) (TYPE)va_arg(ap, TYPE)

static char* utohex(char* str, uint64_t x, int min_digits) {
    size_t digits = 1;

    uint64_t _x = x;

    // _x < 0xf
    while(_x & ~0xf) {
        _x >>= 4;
        digits++;
    }
    
    if((int)digits < min_digits)
        digits = min_digits;

    char* end = str+digits;

    char* ptr = end;
    
    for(int i = digits;i>0;i--) {
        int d = x & 0x0f;
        if(d < 0xa)
            d += '0';
        else
            d += 'a'-10;
        *(--ptr) = d;

        x >>= 4;
    }


    *end = '\0';
    return end;
}

// unsigned to string
// return a ptr to the end of the string
static char* utos(char* str, uint64_t x, int min_digits) {
    size_t digits = 1;

    uint64_t _x = x;
    while(_x > 9) {
        _x /= 10;
        digits++;
    }

    if((int)digits < min_digits)
        digits = min_digits;

    char* end = str+digits;
    
    char* ptr = end;
    for(int i = digits;i>0;i--) {
        *(--ptr) = '0' + x%10;

        x /= 10;
    }
    *end = '\0';


    return end;
}

// signed to string
static char* itos(char* str, int64_t x, int min_digits) {
    if(x < 0) {
        *(str++) = '-';
        x *= -1;
    }

    return utos(str, x, min_digits);
}


int vsprintf(char *str, const char *format, va_list ap) {
    char cf;
    int args_put = 0;
    int persent = 0;
    int digits = -1;

// bool value: '%l..' 
    int _long = 0;

    while(1) {
        cf = *(format++);
        if(cf == '\0')
            break;
        
        if(persent) {
            if(cf == '.')
                continue;
            else if(cf == 'l') {
                _long = 1;
                continue;
            }

            int i = cf - '0';
            if(i >= 0 && i <= 9) {
                if(digits < 0)
                    digits = 0;
                digits = 10 * digits + i;
            }
            else {
                switch(cf) {
                    default:
                    // invalid
                        break;
                    case 'u':
                        if(!_long)
                            str = utos(str, ARG(uint32_t), digits);
                        else
                            str = utos(str, ARG(uint64_t), digits);
                        break;
                    case 'd':
                    case 'i':
                        if(!_long)
                            str = itos(str, ARG(int32_t), digits);
                        else
                            str = itos(str, ARG(int64_t), digits);
                        break;
                    case 'h':
                    case 'x':
                        if(!_long)
                            str = utohex(str, ARG(uint32_t), digits);
                        else
                            str = utohex(str, ARG(uint64_t), digits);
                        break;
                    case 's':
                        {
                            const char* arg = ARG(const char*);
                            if(digits >= 0) {
                                strncpy(str, arg, digits);
                                if((int) strlen(arg) < digits)
                                    str += strlen(arg);
                                else
                                    str += digits;
                            }
                            else {
                                strcpy(str, arg);
                                str += strlen(arg);
                            }
                        }
                        break;
                    case 'c':
                        *(str++) = (char)ARG(int);
                        break;
                }
                digits = -1;
                persent = 0;
                _long = 0;
                args_put++;
            }
        }

        else if(cf == '%') {
            if(persent) {
                // allow %% in fmt to print '%'
                persent = 0;
                *(str++) = '%';
            }
            persent = 1;
        }
        else {
            *(str++) = cf;
        }
    }

    *str = '\0';
    return args_put;
}


int sprintf(char* str, const char* format, ...) {
    va_list ap;
    int res;

    va_start(ap, format);
    res = vsprintf(str,format,ap);
    va_end(ap);

    return res;
}

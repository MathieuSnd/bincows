#include <sprintf.h>
#include <string.h>

#define SIGN(X) X > 0 ? 1 : -1
#define ARG(TYPE) (TYPE)va_arg(ap, TYPE)
#define MIN(A,B) (((A) < (B)) ? (A) : (B))

static char* utohex(char* str, uint64_t x, int min_digits, int max_digits) {
    size_t digits = 1;

    uint64_t _x = x;

    // _x < 0xf
    while(_x & ~0xf) {
        _x >>= 4;
        digits++;
    }

    
    if(digits > max_digits) {
        for(unsigned i = digits; i > max_digits; i--)
            x /= 16;

        digits = max_digits;
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
static char* utos(char* str, uint64_t x, int min_digits, size_t max_digits) {
    size_t digits = 1;

    uint64_t _x = x;
    while(_x > 9) {
        _x /= 10;
        digits++;
    }


    if(digits > max_digits) {
        for(unsigned i = digits; i > max_digits; i--)
            x /= 10;

        digits = max_digits;
    }
    if((int)digits < min_digits)
        digits = min_digits;

    //digits = 5;

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
static char* itos(char* str, int64_t x, int min_digits, size_t max_digits) {
    if(x < 0) {
        *(str++) = '-';
        x *= -1;
    }

    return utos(str, x, min_digits, max_digits - 1);
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, ~0llu, format, ap);
}



int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    char cf;
    int persent = 0;
    int digits = -1;

    // save the beginning to count output size
    char* strbegin = str;

// bool value: '%l..' 
    int _long = 0;

    while(size > 0) {
        
        cf = *(format++);
        if(cf == '\0')
            break;
        size--;


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
                char* end = str;
                switch(cf) {
                    default:
                    // invalid
                        break;
                    case 'u':
                        if(!_long)
                            end = utos(str, ARG(uint32_t), digits, size);
                        else
                            end = utos(str, ARG(uint64_t), digits, size);
                        break;
                    case 'd':
                    case 'i':
                        if(!_long)
                            end = itos(str, ARG(int32_t), digits, size);
                        else
                            end = itos(str, ARG(int64_t), digits, size);
                        break;
                    case 'h':
                    case 'x':
                        if(!_long)
                            end = utohex(str, ARG(uint32_t), digits, size);
                        else
                            end = utohex(str, ARG(uint64_t), digits, size);
                        break;
                    case 's':
                        {
                            const char* arg = ARG(const char*);
                            if(digits >= 0) {

                                int len = MIN(digits, (int)size);
                                strncpy(str, arg, len);
                                if((int) strlen(arg) < len)
                                    end += strlen(arg);
                                else
                                    end += len;
                            }
                            else {
                                strncpy(str, arg, size);
                                end += strlen(arg);
                            }
                        }
                        break;
                    case 'c':
                        *(end++) = (char)ARG(int);
                        break;
                }
                digits = -1;
                persent = 0;
                _long = 0;

                size -= end-str;

                str = end;
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
    return str - strbegin;
}


int sprintf(char* str, const char* format, ...) {
    va_list ap;
    int res;

    va_start(ap, format);
    res = vsprintf(str,format,ap);
    va_end(ap);

    return res;
}

void default_backend(const char* string, size_t len) {
    (void)(string + len);
}

static void (* backend_fun)(const char *, size_t) = default_backend;


void set_backend_print_fun(void (*fun)(const char *string, size_t length)) {
    backend_fun = fun;
}


int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

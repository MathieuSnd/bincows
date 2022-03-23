

int x = 8;
const char* rodata = " zefgrbefvzegr ";

int g = 0;

int fibo(int v) {
    if(v < 2)
        return v;
    else
        return fibo(v - 1);
}

int _start(int argc, char** argv) {
    return 2;
}
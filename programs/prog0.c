

int x = 8;

int fibo(int v) {
    if(v < 2)
        return v;
    else
        return fibo(v - 1);
}

int main(int argc, char** argv) {
    return fibo(x);
}
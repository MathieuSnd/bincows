#include "../kernel/lib/string.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>


static int is_absolute(const char *path)
{
    return path[0] == '/';
}

static void simplify_path(char* dst, const char* src) {
    
    char *buf = malloc(strlen(src)+1);
    

    *dst = 0; 

    strcpy(buf, src);

    char *sub = strtok(buf, "/");

    // each loop concatenates
    // "/str"
    // so the final output path is in format:
    // "/foo/bar/foobar"
    while (sub != NULL)
    {

        if(!strcmp(sub, "."))
            ;
        else if(!strcmp(sub, "..")) {
            char* ptr = (char*)strrchr(dst, '/');

            if(ptr != NULL)
                *ptr = '\0';
        }
        else {
            strcat(dst, "/");
            strcat(dst, sub);
        }

        sub = strtok(NULL, "/");
    }

    free(buf);

    if(dst[0] == '\0') {
        *dst++ = '/';
        *dst = '\0';
    }
}



uint16_t path_hash(const char* path) {
    unsigned len = strlen(path);
    uint16_t res = 0;
    const uint8_t* dwp = path;


    for(unsigned i = 0; i < len; i += 1)
    {
        uint32_t entropy_bits = dwp[2*i] ^ (dwp[2*i+1] << 4);

        entropy_bits ^= (entropy_bits << 4);

        res = (res ^ (entropy_bits+i)) & 8191;
    }
    return res;
}

#define CACHE_SIZE 8192

int main(void) {
    /*
    char* path[] = {
        "//..//../././..///./home//mathieu//",
        "",
        "/../",
        "/..///..//../././..///./home//mathieu////..//../././..///./home//mathieu////..//../././..///./home//mathieu//",
        "/aaa/bbb/ccc/../cc/../jeu/../../../oui",
        "/aaa/bbb/ccc/../cc/../jeu/./oui/",
        "/oui/non",
    };

    for(int i = 0; i  < sizeof(path)/sizeof(char*); i++) {
        char* buf = malloc(strlen(path[i])+1);
        simplify_path(buf, path[i]);
        printf("'%s' -> '%s' %d\n", path[i], buf, sizeof(path));

        free(buf);
    }*/

    FILE* file = fopen("file_list.txt", "r");
    char path[4097];

    uint32_t cache[CACHE_SIZE];
    uint64_t n_paths = 0;

    int avlen = 0;


    memset(cache, 0, CACHE_SIZE * sizeof(uint32_t));

    while(fgets(path, 4096, file) != NULL) {
        assert(strchr(path, '\n'));
        *(char*)strchr(path, '\n') = '\0';

        uint16_t hash = path_hash(path) & (CACHE_SIZE-1);

        
        if(hash >= CACHE_SIZE) {
            printf("ayao %u\n", hash);
            return 1;
        }

        int len = strlen(path)+1;

        if(len & ~31)
            len = (len & ~31) + 32;

        avlen += len;

        n_paths++;


        cache[hash]++;
    }
    fclose(file);

    int n_empty = 0;

    unsigned min = 0xffffffff, max = 0;
    double var = 0;
    double e = n_paths / CACHE_SIZE;

    
    for(int i = 0; i < CACHE_SIZE; i++) { 
        int x = cache[i];
        if(!x)
            n_empty++;

        if(x < min)
            min = x;
        else if(x > max)
            max = x;
        
        
        var += (x - e) * (x - e);
    }

    int collisions = n_empty + n_paths - CACHE_SIZE;

    var = sqrt(var / n_paths);

    printf("min = %u, max = %u, V = %lf, total = %u, empty: %u "
            "; path average len = %u, collision rate: %.2lf %%\n", 
min,max,var,n_paths, n_empty, avlen / n_paths, collisions / (double)n_paths * 100.0);
}
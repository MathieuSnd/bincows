// for initializing std streams
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>


int x = 8;
const char* rodata = " zefgrbefvzegr ";

int g = 0;

int fibo(int v) {
    if(v < 2)
        return v;
    else
        return fibo(v - 1) + fibo(v - 2);
}

int f(int x) {
    return x+1;
}


#define VERSION "0.1"

const char* version_string = 
        "Welcome in the Bincows Shell!\n"
        "Version: " VERSION "\n"
//        "Copyright (c) 2020-2021 \n"
        "see https://github.com/MathieuSnd/bincows\n"
        "type help to list commands\n\n"
;

static char* cwd = NULL;

/*
int print_input_end() {
    
    return 0;
}
*/

void init_stream(void) {
    int __stdin  = open("/dev/ps2kb", 0,0);
    int __stdout = open("/dev/term", 0,0);
    int __stderr = open("/dev/term", 0,0);

    if(__stdin  != STDIN_FILENO)
        *(int*)0 = 0;
    if(__stdout != STDOUT_FILENO)
        *(int*)2 = 0;
    if(__stderr != STDERR_FILENO)
        *(int*)3 = 0;
}


void print_cow(void) {
    int file = open("/boot/boot_message.txt", 0,0);
    if(file < 0) {
        printf("/boot/cow not found\n");
        return;
    }

    
    size_t file_size = lseek(file, 0, SEEK_END);

    lseek(file, 0, SEEK_SET);

    char* buf = malloc(file_size+1);

    if(!buf) {
        printf("malloc failed\n");
        return;
    }

    size_t n = read(file, buf, file_size);

    buf[n] = '\0';

    printf("%s", buf);

    free(buf);

    close(file);
}

static void cd(const char* path) {
    int r = chdir(path);

    if(r != 0)
        printf("cd: no such file or directory\n");

    free(cwd);
    cwd = getcwd(NULL, 0);
}

static void export(const char** argv) {
    if(argv[1] == NULL) {
        printf("export: missing argument\n");
        return;
    }

    char* dup = strdup(argv[1]);


    putenv(dup);
}

/**
 * execute the builtin command
 * if it is one,
 * else return 0
 */
static int builtin_cmd(const char** argv) {

    if(strcmp(argv[0], "cd") == 0) {
        if(!argv[1]) {
            printf("cd: missing argument\n");
            return 1;
        }

        cd(argv[1]);
    }
    else if(strcmp(argv[0], "export") == 0) {
        export(argv);
    }
    
    else if(strcmp(argv[0], "help") == 0)
        printf("\n"
               "help\n"
               "\tlist all commands\n"
               "\n"
               "version\n"
               "\tprint version\n"
               "\n"
               "exit\n"
               "\tquit the shell\n"
               "\n"
               "cow\n"
               "\tprint a cow\n"
               "\n"
               "\n");
    else if(strcmp(argv[0], "version") == 0)
        printf(version_string);
    else if(strcmp(argv[0], "cow") == 0)
        print_cow();
    else if(strcmp(argv[0], "exit") == 0)
        exit(0);
    else
        return 0;        
    return 1;
}


static char** convert_cmdline(char* cmdline) {   
    char** argv = malloc(sizeof(char*));
    argv[0] = NULL;

    char* cmd = strtok(cmdline, " ");

    int i = 0;
    while(cmd) {
        argv = realloc(argv, sizeof(char*) * (i + 2));
        argv[i++] = cmd;
        argv[i] = NULL;
        cmd = strtok(NULL, " ");
    }

    return argv;
}


static void execute(char* cmd) {
    // convert to argv
    char** argv = convert_cmdline(cmd);

    if(!argv[0])
        return;
    
    if(builtin_cmd(argv))
        return;


    // execute
    int ret = forkexec(argv);


    if(ret) 
    {
        // couldn't execute
        
        if(access(argv[0], F_OK)) {
            // the file exists, but we can't execute it

            printf("can't execute '%s'\n", argv[0]);
        }
        else
            printf("command not found: %s\n", argv[0]);
    }

    free(argv);
}



void print_prompt(void) {
    printf("%s > _\b", cwd);
}

int main(int argc, char** argv) {
    init_stream();

    print_cow();


    printf("%s\n", version_string);


    // current working directory
    cwd = getcwd(NULL, 0);

    export((const char*[]){NULL,"PATH=/bin",0});


    for(int i = 0; i < 500; i++)
        execute("write");

    print_prompt();


    int cur = 0;
    char line[1024];


    while(1) {
        char ch;

        if(read(STDIN_FILENO, &ch, 1) <= 0)
            break;

        switch(ch) {
            default:
                line[cur++] = ch;
                printf("%c_\b", ch);
                break;
            case '\b':
                if(cur > 0) {
                    printf(" \b\b_\b");
                    cur--;
                }
                break;
            case '\n':
                printf("\n");
                line[cur] = 0;
                execute(line);
                print_prompt();
                cur = 0;
                break;
        }
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    free(cwd);

}

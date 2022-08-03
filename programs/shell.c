// for initializing std streams
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <bc_extsc.h>
#include <signal.h>


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


static int execute(char* cmd);

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

    printf("%s\n", buf);

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


extern char** __environ;

static void list_env(void) {
    char** ptr = __environ;

    while(*ptr) {
        printf("%s\n", *ptr);
        ptr++;
    }
} 

static void export(const char** argv) {
    if(argv[1] == NULL) {
        list_env();        
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


char* env_end(char* begin) {
    char* end = begin;
    while(*end) {
        
        if(end == begin && *end >= '0' && *end <= '9')
            return end;


        if((*end >= 'a' && *end <= 'z')
            || (*end >= 'A' && *end <= 'Z')
            || (*end >= '0' && *end <= '9')
            || (*end == '_')
        )
            end++;
        else
            return end;
    }
    return end;
}

char* eval_env(char* cmd) {
    // replace $VAR with the value of VAR

    char* ptr = cmd;
    char* var;

    char* dest = malloc(strlen(cmd) + 1);

    char* dptr = dest;

    int dest_sz = strlen(cmd) + 1;


    while(1) {
        var = (char*)strchr(ptr, '$');

        if(var) {
            memcpy(dptr, ptr, var - ptr);
            dptr += var - ptr;


            char* end = env_end(var + 1);



            int len = end - var;
        

            char* var_name = malloc(len);

            memcpy(var_name, var + 1, len - 1);

            var_name[len - 1] = '\0';

            // get the value of the variable
            char* var_value = getenv(var_name);


            if(var_value) {
                dest_sz += strlen(var_value);

                int i = dptr - dest;
                dest = realloc(dest, dest_sz);

                dptr = dest + i;

                strcpy(dptr, var_value);

                
                dptr += strlen(var_value);
            }

            ptr = end;
        }
        else {
            strcpy(dptr, ptr);
            break;
        }
    }

    return dest;
}



void type_character(char ch) {


    static int cur = 0;
    static char line[1024];

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
            cur = 0;

            if(execute(line) == -1) {
                print_prompt();
            }
            break;
    }
}



// is set everytime a SIGCHLD signal occurs
static volatile int sigchld_flag = 0;

void sigchld_handler(int s) {
    if(s != SIGCHLD) {
        printf("error: sigchld_handler received signal %d != %d", 
                        s, SIGCHLD);
        return;
    }

    sigchld_flag = 1;
    print_prompt();
}


int tolower(int c) {
    if(c >= 'A' && c <= 'Z')
        return c + 32;
    return c;
}

int toupper(int c) {
    if(c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}


static int execute(char* cmd) {
    // convert to argv

    char* final = eval_env(cmd);

    char** argv = convert_cmdline(final);

    free(final);

    if(!argv[0])
        return -1;

    if(builtin_cmd((const char**) argv))
        return -1;



    // 1 - stdin
    // 2 - stdout
    // 3 - stderr
    // all the others are to be masked
    const fd_mask_t fdmask = ~7llu;


    // setup pipe
    int pipe_ends[2];
    int r = pipe(pipe_ends);

    if(r) {
        printf("couldn't create a pipe\n");
        free(argv);
        return -1;
    }

    const int term_in = 31;

    dup2(0, term_in);

    dup2(pipe_ends[0], 0);


    sigchld_flag = 0;

    // execute
    int pid = forkexec((const char* const*)argv, fdmask);


    dup2(term_in, 0);
    close(term_in);
    close(pipe_ends[0]);


    if(pid == -1) 
    {
        // couldn't execute
        
        if(access(argv[0], F_OK)) {
            // the file exists, but we can't execute it

            printf("can't execute '%s'\n", argv[0]);
        }
        else
            printf("command not found: %s\n", argv[0]);

    }
    else {
        
        while(1) {
            int r = fgetc(stdin);

            if(r <= 0) {
                printf("broken terminal %lu\n", r);
                break;
            }


            if(r == 0xff) {
                // control character
                r = fgetc(stdin);

                r = toupper(r);

                printf("^%c\n", r);

                switch(r) {
                    case 'D': // CTRL-D
                    // eof: close pipe
                        close(pipe_ends[1]);
                        if(sigchld_flag)
                            pause();
                        free(argv);
                        return pid;
                    case 'C': // CTRL-C
                        // kill the child process
                        kill(pid, SIGINT);
                        break;
                }
            } else {
                int w = write(pipe_ends[1], &r, 1);

                if(w <= 0) {
                    type_character(r);
                    break;
                }
                else
                    fputc(r, stdout);
            }
        }

        close(pipe_ends[1]);
    }

    free(argv);

    return pid;
}



void print_prompt(void) {
    printf("%s > _\b", cwd);
}


void script_exec(char* line) {

    int isspace(int c) {
        return c == ' ' || c == '\t';
    }

    // remove spaces
    while(isspace(*line))
        line++;

    if(*line == '#') // comment
        return;

    execute(line);
}

void exec_script(const char* path) {
    FILE* file = fopen(path, "r");
    if(!file) {
        printf("couldn't open file '%s'\n", path);
        return;
    }


    char line[2048];

    const int len = sizeof(line);


    while(fgets(line, len, file) != NULL) {
        line[len-1] = '\0';
        script_exec(line);
    }

    fclose(file);
}

int main(int argc, char** argv) {
    init_stream();
    
    signal(SIGCHLD, sigchld_handler);

    exec_script("/home/.shrc");

    printf("%s\n", version_string);


    // current working directory
    cwd = getcwd(NULL, 0);


    print_prompt();



    while(1) {
        char ch;

        if(read(STDIN_FILENO, &ch, 1) <= 0)
            break; // eof

        type_character(ch);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    free(cwd);

}

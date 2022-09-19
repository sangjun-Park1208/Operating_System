#include "types.h"
#include "syscall.h"
#include "user.h"

int main(int argc, char* argv[]){
    int mask = atoi(argv[1]);
    int pid, wpid;

    pid = fork();

    if(pid == 0){
        trace(mask);
        exec(argv[2], argv+2);
        exit();
    }
    if(pid > 0){
        while((wpid=wait()) >= 0 && wpid != pid)
            printf(1, "zombie!\n");
    }
    exit();
    return 0;
    
}
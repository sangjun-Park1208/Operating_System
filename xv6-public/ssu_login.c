#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char userID[16][32]; // list.txt
char pwdID[16][32]; // list.txt
char buf[32];
char idBuf[16];
char pwBuf[16];

char *argv[] = { "sh", 0 };

void get_user_list(){
    int fd, n, i, k;
    if((fd = open("list.txt", O_RDONLY)) < 0){
        printf(1, "open error in %s\n", "list.txt");
        exit();
    }

    i=0; k=0;
    while((n = read(fd, buf, sizeof(buf))) > 0){
        while(buf[i] != ' '){
            userID[i] = buf[i];
            i++;
        }
        i++; // 공백 제거
        while( k<16 && i<32 && buf[i] != '\n')
            pwdID[k++] = buf[i++];

    }

    close(fd);
}

void get_input(){
    printf(1, "Useranme: ");
    read(0, idBuf, sizeof(idBuf));

    printf(1, "Password: ");
    read(0, pwBuf, sizeof(pwBuf));
}

int check_idpw(){
    for(int i=0; i<16; i++){
        if(!strcmp(userID[i], idBuf) && !strcmp(pwdID[i], pwBuf))
            return 1;
    }
    return -1;
}

int main(int argc, char* argv[]){
    int chk, pid, wpid;

    get_user_list();
    get_input();

    while((chk = check_idpw()) < 0){
        printf(1, "--Invalid ID/PW. Try again.--\n");
        get_input();
    }

    pid = fork();
    if(pid < 0){
        printf(1, "fork failed.\n");
        exit();
    }
    if(pid == 0){
        exec("sh", argv);
        exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
        printf(1, "zombie!\n");

    return 0;    
}
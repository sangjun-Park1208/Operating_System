#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char userID[16][32]; // list.txt
char pwdID[16][32]; // list.txt
char buf[1];
char idBuf[16];
char pwBuf[16];

char *argv_[] = { "sh", 0 };

void get_user_list(){
    int fd, i, k, j, n;
    if((fd = open("list.txt", O_RDONLY)) < 0){
        printf(1, "open error in %s\n", "list.txt");
        exit();
    }

    i=0; k=0; j=0;
    int isID, isPW;
    isID = 1; isPW = 0;
    while((n = read(fd, buf, sizeof(buf))) > 0){
        printf(1, "buf[0] : %s\n", buf);
        if(buf[0] == ' '){
            isID = 0; isPW = 1; 
            continue;
        }
        if(buf[0] == '\n'){
            // while(buf[0] == '\n') read(fd, buf, sizeof(buf));
            i++; j=k=0;
            isID = 1; isPW = 0;
            continue;
        }

        if(isID){
            userID[i][j++] = buf[0];
            continue;
        }
        if(isPW){
            pwdID[i][k++] = buf[0];
            continue;
        }

    }



    // while((n = read(fd, buf, sizeof(buf))) > 0){

    //     printf(1, "buf : %s\n", buf);
    //     while(buf[j] != ' '){
    //         userID[i][s++] = buf[j++];
    //     }
    //     userID[i][s] = '\0';
    //     printf(1, "userID : %s\n", userID[i]);
    //     j++;

    //     while(buf[j] != '\n'){
    //         pwdID[i][k++] = buf[j++];
    //     }
    //     pwdID[i][k] = '\0';
    //     printf(1, "pwdID : %s\n", pwdID[i]);
    //     while(buf[j] == '\n') j++;
    //     j=s=k=0;
    //     i++;
    // }
    for(int i=0; i<2; i++){
        printf(1, "ID : %s, PW : %s.\n", userID[i], pwdID[i]);
    }

    close(fd);
}

void get_input(){
    printf(1, "Username: ");
    read(0, idBuf, sizeof(idBuf));
    idBuf[strlen(idBuf)-1] = '\0';

    printf(1, "Password: ");
    read(0, pwBuf, sizeof(pwBuf));
    pwBuf[strlen(pwBuf)-1] = '\0';

}

int check_idpw(){
    for(int i=0; i<16; i++){
        if(!strcmp(userID[i], idBuf) && !strcmp(pwdID[i], pwBuf))
            return 1;
    }
    return -1;
}

int main(int argc, char* argv[]){
    printf(1, "DEBUG\n");
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
        exec("sh", argv_);
        exit();
    }
    while((wpid=wait()) >= 0 && wpid != pid)
        printf(1, "zombie!\n");

    return 0;    
}
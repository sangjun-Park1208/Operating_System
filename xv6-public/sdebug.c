// 필요한 Header file
#include "types.h"
#include "user.h"

#define PNUM 5
#define PRINT_CYCLE 10000000
#define TOTAL_COUNTER 500000000

void sdebug_func(void){
    int n, pid;
    int weight_ = 1;
    printf(1, "start sdebug command\n");

    for(n=0; n<PNUM; n++){
        pid = fork();
        if(pid < 0)
            break;
        if(pid == 0){
            // 힌트 : counter, print_counter 변수 초기화, first 변수 초기화, end_ticks
            if(weightset(weight_)<0){
                printf(1,"weight error\n");
                exit();
            }
            int counter = 0;
            int print_counter = PRINT_CYCLE;
            int first = 1;
            int start_ticks = uptime();
            int end_ticks;
            while(counter <= TOTAL_COUNTER){
                counter++;
                if(print_counter == 0){
                    if(first){
                        end_ticks = uptime();
                        printf(1, "PID: %d, WEIGHT: %d, ", getpid(), weight_);
                        printf(1, "TIMES: %d ms\n", (end_ticks - start_ticks)*10);
                        first = 0;
                    }
                    print_counter = PRINT_CYCLE;
                }
                print_counter--;
            }
            printf(1, "PID: %d terminated\n", getpid());
            exit();
        }
        weight_++;
    }

    for(; n > 0; n--){
       if(wait() < 0){
        printf(1, "wait stopped early\n");
        exit();
       }
    }

  if(wait() != -1){
    printf(1, "wait got too many\n");
    exit();
  }

    printf(1, "end of sdebug command\n");
}

int main(void){
    sdebug_func();
    exit();
}
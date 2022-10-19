#include "types.h"
#include "user.h"

#define PNUM 5
#define PRINT_CYCLE 10000000
#define TOTAL_COUNTER 500000000

void sdebug_func(void){
    int n, pid;
    int weight_ = 1;
    printf(1, "start sdebug command\n");

    for(n=0; n<PNUM; n++, weight_++){
        pid = fork();
        if(pid < 0) // fork 예외처리
            break;
        if(pid == 0){ // 자식 프로세스
            if(weightset(weight_) < 0){ // weightset 시스템콜에 인자로 가중치 1부터 증가시키면서 프로세스별로 할당
                printf(1,"weight error\n");
                exit();
            }
            int counter = 0;
            int print_counter = PRINT_CYCLE;
            int first = 1;
            int start_ticks = uptime();
            int end_ticks;
            while(counter <= TOTAL_COUNTER){ 
                counter++; // 종료 조건 : counter값이 5억보다 커지는 경우
                if(print_counter == 0){
                    if(first){
                        end_ticks = uptime(); // 한 번만 출력하기 위해 first변수로 처음인지 체크. 
                        printf(1, "PID: %d, WEIGHT: %d, TIMES: %d ms\n", getpid(), weight_, (end_ticks - start_ticks)*10);
                        // printf(1, "TIMES: %d ms\n", (end_ticks - start_ticks)*10);
                        first = 0;
                    }
                    print_counter = PRINT_CYCLE; // 실행 중인 프로세스 정보를 출력하는 주기(1천만)가 다 된 경우, 새로 할당
                }
                print_counter--;
            }
            printf(1, "PID: %d terminated\n", getpid()); // 프로세스 종료
            exit();
        }
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
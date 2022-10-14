// // 필요한 Header file

// #define PNUM 5
// #define PRINT_CYCLE 10000000
// #define TOTAL_COUNTER 500000000

// void sdebug_func(void){
//     int n, pid;
//     printf(1, "start sdebug command\n");

//     for(n=0; n<PNUM; n++){
//         pid = fork();
//         if(pid < 0)
//             break;
//         if(pid == 0){
//             // 힌트 : counter, print_counter 변수 초기화, first 변수 초기화, end_ticks
//             int first = 1;
//             while(counter <= TOTAL_COUNTER){
//                 // 힌트
//                 if(print_counter == 0){
//                     if(first){
//                         end_ticks = uptime();
//                         printf(1, "PID: %d, WEIGHT: %d, ", );
//                         printf(1, "TIMES: %d ms\n", );
//                         first = 0;
//                     }
//                     print_counter = PRINT_CYCLE;
//                 }
//             }
//             printf(1, "PID: %d terminated\n", getpid());
//         }
//     }

//     // 힌트

//     printf(1, "end of sdebug command\n");
// }

// int main(void){
//     sdebug_func();
//     exit();
// }
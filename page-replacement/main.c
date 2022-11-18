#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

char* algorithm_type[9] = {"", "Optimal", "FIFO", "LIFO", "LRU", "LFU", "SC", "ESC", "ALL"};

int* input1;
int input1_cnt;
int input2;
int input3;

int PAGEFRAME_NUM;
int HIT_CNT;
int MISS_CNT;
int PAGE_STREAM_NUM;

int* RANDOM_STREAM;

typedef struct Node {
    struct Node* next;
    struct Node* prev;
    int page;
    int ref_cnt; // reference count : 
    int ref_bit;
    int read_bit;
    int write_bit;
} Node;

typedef struct Pageframe {
    Node* head;
    Node* tail;
    Node* cur;
    Node* tmp;
    int size;
} Pageframe;

Pageframe* pageframe;

void start_simulator();
void get_input1();
void get_input2();
void get_input3();

void set_random_stream();

/* string manipulate functions */
int split(char*, char*, char*[]);

/* node manipulate functions */
Node* create_node(int);
void append_node(int);
int find_node(int);

/* Algorithms */
void FIFO();

/* pageframe manipulate function */
void initiate_Pageframe();

/* print functions */
void print_algorithm_type(int);
void print_column();
void print_result();
void print_pageframe(int, char*);

int main(void){
    start_simulator();
    print_column();
    print_result();
}


void start_simulator(){
    get_input1();
    get_input2();
    get_input3();

    FIFO();

    printf("Exit Program..\n");
}

void get_input1(){
    char input[20];
    char* argv[5];

    while(1){
        printf("Select Page Replacement algorithm simulator. (Maximum three)\n");
        printf("(1)Optimal (2)FIFO (3)LIFO (4)LRU (5)LFU (6)SC (7)ESC (8)ALL\n");
        fgets(input, sizeof(input), stdin);
        input[strlen(input) - 1] = '\0';
        input1_cnt = split(input, " ", argv);
        printf("argc : %d\n", input1_cnt);

        if (input1_cnt < 1 || input1_cnt > 3) {
            printf("Usage: input number count (1~3)\n");
            continue;
        }

        input1 = (int*)malloc(sizeof(int)*input1_cnt);
        for(int i=0; i<input1_cnt; i++){
            input1[i] = atoi(argv[i]);
            if(input1[i] < 1 || input1[i] > 8) {
                printf("Usage: input number (1~8)\n");
                printf("Exit Program..\n");
                continue;
            }
        }
        break;
    }


}
void get_input2(){
    while(1){
        printf("Set page frame number. (3~10)\n >> ");
        scanf("%d", &input2);
        if (input2 < 3 || input2 > 10){
            printf("Usage: page frame number must have 3~10 range\n");
            continue;
        }
        PAGEFRAME_NUM = input2;
        break;
    }
}
void get_input3(){
    while(1){
        printf("Select data input style. (1~2)\n");
        printf("(1)Random  (2)User file open\n >> ");
        scanf("%d", &input3);
        if(input3 < 1 || input3 > 2){
            printf("Usage: input number should be '1' or '2'. Try again.\n");
            continue;
        }

        printf("Input Page stream count (bigger than 500).\n >> ");
        scanf("%d", &PAGE_STREAM_NUM);
        // if(PAGE_STREAM_NUM < 500){
        //     printf("Usage: Page stream count must be bigger than 500. Try again.\n");
        //     continue;
        // }

        set_random_stream();
        if(input3 == 2){
            int fd;
            if((fd = open("input.txt", O_RDWR | O_CREAT | O_TRUNC)) < 0){
                printf("open error in input.txt\n");
                exit(1);
            }

            for(int i=0; i<PAGE_STREAM_NUM; i++){
                // write(fd, RANDOM_STREAM, sizeof(int)*PAGE_STREAM_NUM);

            }

        }
        break;
    }
}

void set_random_stream() {
    RANDOM_STREAM = (int*)malloc(sizeof(int)*PAGE_STREAM_NUM);

    srand((unsigned int)time(NULL));
    printf("<STREAM>\n");
    for(int i=0; i<PAGE_STREAM_NUM; i++){
        RANDOM_STREAM[i] = rand()%30 + 1;
        printf("%d ", RANDOM_STREAM[i]);
    }
    printf("\n");
}

int split(char* string, char* seperator, char* argv[]){
    int argc = 0;
    char* ptr = NULL;

    ptr = strtok(string, seperator);
    while(ptr != NULL){
        argv[argc++] = ptr;
        ptr = strtok(NULL, " ");
    }
    return argc;
}


void print_algorithm_type(int type_num) {
    printf("[%s] Simulator\n", algorithm_type[type_num]);
}
void print_column() {
    printf("================================================================================================================================================================\n");
    printf("Stream\t\t");
    printf("Result\t\t");
    for(int i=0; i<PAGEFRAME_NUM; i++){
        printf("Pageframe[%d]\t\t\t", i);
    }
    printf("\n----------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
}
void print_pageframe(int stream, char* result) {
    // printf("in print_pageframe\n");
    printf("%d\t\t%s\t\t", stream, result);

    pageframe->tmp = pageframe->head;
    while (pageframe->tmp->page != pageframe->tail->page){
        printf("%d\t\t\t", pageframe->tmp->page);
        pageframe->tmp = pageframe->tmp->next;
    }
    printf("\n");
}
void print_result() {
    printf("\n\n*******RESULT*******\n");
    printf(" [HIT] :  %d\n", HIT_CNT);
    printf("[MISS] :  %d\n", MISS_CNT);
    printf("================================================================================================================================================================\n");
}
void print_page_stream() {

}



Node* create_node(int page) {
    // printf("in create_node\n");
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->page = page;

    return newNode;
}
void append_node(int page) { // pageframe->size 가 입력받은 pageframe 크기보다 작을 때는 무조건 append
    // printf("in append_node\n");
    MISS_CNT++;
    if(pageframe->head == NULL){ // 없으면 초기화
        pageframe->head = pageframe->tail = pageframe->cur = create_node(page);
        pageframe->size++;
        return;
    }
    Node* newNode = create_node(page);
    pageframe->tail->next = newNode;
    newNode->prev = pageframe->tail;
    pageframe->tail = newNode;
    pageframe->size++;
    return;
}
int find_node(int page) { 
    // printf("in find_node\n");
    pageframe->tmp = pageframe->head;
    while (pageframe->tmp->page != pageframe->tail->page){
        if (pageframe->tmp->page == page)
            return 1;
        pageframe->tmp = pageframe->tmp->next;
    }
    return 0; // Not Found
}
void initiate_Pageframe() {
    // printf("in initiate_Pageframe\n");
    pageframe = (Pageframe*)malloc(sizeof(Pageframe));
    pageframe->head = pageframe->tail = pageframe->cur = NULL;
}




// find_node() 함수도 각 알고리즘 별로 만들어야 함. (중요)



void FIFO() {
    // printf("in FIFO\n");
    int isFound;
    int pageframecnt=0;
    initiate_Pageframe();
    Node* tmp;
    for(int i=0; i < PAGE_STREAM_NUM; i++){ // 500회 수행
        if(i<PAGEFRAME_NUM){
            // printf("in first if block\n");
            pageframecnt++;
            append_node(RANDOM_STREAM[i]);
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }
        
        if(find_node(RANDOM_STREAM[i])){
            // printf("in second if block\n");
            print_page_stream(RANDOM_STREAM[i], "HIT");
            continue;
        }
        /* head(first in) node 삭제 */
        tmp = pageframe->head->next;
        tmp->prev = NULL;
        pageframe->head->next = NULL;
        pageframe->head = tmp;

        /* tail(last in) node 추가 */
        append_node(RANDOM_STREAM[i]);
        // printf("not in if block\n");
        print_pageframe(RANDOM_STREAM[i], "MISS");
    }

    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
    printf("cnt : %d\n", pageframecnt);
}
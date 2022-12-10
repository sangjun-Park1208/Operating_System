#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define BIGNUM 9999999
#define BUF_SIZE 5

// 파일 입출력을 위한 전처리문
#define PRINT_BUF_SIZE 1024


char print_buf[PRINT_BUF_SIZE];
char* algorithm_type[9] = {"", "Optimal", "FIFO", "LIFO", "LRU", "LFU", "SC", "ESC", "ALL"};
char* algorithm_type_filename[9] = {"", "Optimal.txt", "FIFO.txt", "LIFO.txt", "LRU.txt", "LFU.txt", "SC.txt", "ESC.txt", ""};

/* 입력 전역 변수*/
int* input1; // 첫 번째 입력의 경우, 최대 3개까지 받을 수 있게 구현
int input1_cnt;
int input2;
int input3;

int PAGEFRAME_NUM;
int HIT_CNT;
int MISS_CNT;
int PAGE_STREAM_NUM;

int* RANDOM_STREAM;

int reference_cnt[31]; // For LFU Algorithm : 참조 횟수

int fd; // 파일 입출력 File descriptor

/* 링크드 리스트 구현을 위한 Node, Pageframe 구조체 */
typedef struct Node { 
    struct Node* next;
    struct Node* prev;
    int page;
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
/* 입력 함수 */
void get_input1();
void get_input2();
void get_input3();

/* 스트림 랜덤 생성 함수 */
void set_random_stream();
void set_random_stream_to_file(int);

/* string manipulate functions */
int split(char*, char*, char*[]);

/* node manipulate functions */
Node* create_node(int);
void append_node(int);
void append_node_circular(int);
Node* find_node(int);

/* Algorithms */
void Optimal();
void FIFO();
void LIFO();
void LRU();
void LFU();
void SC();
void ALL();

/* pageframe manipulate function */
void initiate_Pageframe();

/* print functions */
void print_stream();
void print_algorithm_type(int);
void print_column();
void print_result();
void print_pageframe(int, char*);
 
int main(void){
    start_simulator();
    return 0;
}

int hasOptimal() {
    for(int i=0; i<input1_cnt; i++){
        if(input1[i] == 1) return 1;
    }
    return 0;
}

void start_simulator(){
    get_input1();
    get_input2();
    get_input3();

    if(!hasOptimal()) { // Optimal()은 따로 불리지 않아도 항상 호출되어야 함.
        if((fd = open(algorithm_type_filename[1], O_RDWR | O_CREAT | O_TRUNC)) < 0){
            printf("open error in %s\n", algorithm_type_filename[1]);
            exit(1);
        }
        print_algorithm_type(1);
        print_stream();
        print_column();
        Optimal();
        print_result();
        close(fd);
    }

    for(int i=0; i<input1_cnt; i++){
        print_algorithm_type(input1[i]);

        if(input1[i]!=8 && ((fd = open(algorithm_type_filename[input1[i]], O_RDWR | O_CREAT | O_TRUNC)) < 0)){
            printf("open error in %s\n", algorithm_type_filename[input1[i]]);
            exit(1);
        }
        if(input1[i] != 8){
            print_stream();
            print_column();
        }
        switch (input1[i]){ // 첫 번째 입력에서 받은 다중 값들에 대한 알고리즘 함수 실행
            case 1:
                Optimal();
                break;
            case 2:
                FIFO();
                break;
            case 3:
                LIFO();
                break;
            case 4:
                LRU();
                break;
            case 5:
                LFU();
                break;
            case 6:
                SC();
                break;
            case 8:
                close(fd);
                ALL();
                return;
            default:
                break;
        }
        print_result();
        close(fd);
    }

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
        printf("(1)Random  (2)User File open\n");
        scanf("%d", &input3);
        if(input3 < 1 || input3 > 2){
            printf("Usage: input number should be '1' or '2'. Try again.\n");
            continue;
        }

        printf("Input Page stream count (bigger than 500).\n");
        scanf("%d", &PAGE_STREAM_NUM);
        if(PAGE_STREAM_NUM < 500){
            printf("Usage: Page stream count must be bigger than 500. Try again.\n");
            continue;
        }


        if(input3 == 1) set_random_stream();
        if(input3 == 2){
            int fd_;
            if((fd_ = open("input.txt", O_RDWR | O_CREAT | O_TRUNC)) < 0){
                printf("open error in input.txt\n");
                exit(1);
            }
            set_random_stream_to_file(fd_);
            close(fd_);
        }
        break;
    }
}

void set_random_stream() {
    RANDOM_STREAM = (int*)malloc(sizeof(int)*PAGE_STREAM_NUM);

    srand((unsigned int)time(NULL));
    for(int i=0; i<PAGE_STREAM_NUM; i++){
        RANDOM_STREAM[i] = rand()%30 + 1;
    }
    printf("\n");
}

void set_random_stream_to_file(int fd_) {
    RANDOM_STREAM = (int*)malloc(sizeof(int)*PAGE_STREAM_NUM);
    char RANDOM_STREAM_STR[3];
    char buf[BUF_SIZE] = {'\0', };
    char c[1];
    int k=0;

    srand((unsigned int)time(NULL));
    int s;
    for(int i=0; i<PAGE_STREAM_NUM; i++){ // 랜덤으로 스트림 생성
        s = rand()%30 + 1;
        sprintf(RANDOM_STREAM_STR, "%d ", s);
        if(s<10) write(fd_, RANDOM_STREAM_STR, 2); // 1바이트씩 읽기 때문에, 숫자가 두 자리인 경우는 따로 처리
        if(s>=10) write(fd_, RANDOM_STREAM_STR, 3);
        memset(RANDOM_STREAM_STR, '\0', 3);
    }
    printf("\n");

    lseek(fd_, 0, SEEK_SET);
    while(read(fd_, c, 1) > 0){
        if(strcmp(c, " ")) { // 공백이 아니면
            strcat(buf, c); // 뒤에 붙임
            continue;
        }

        // 공백이면
        RANDOM_STREAM[k] = atoi(buf); // 앞 숫자가 끝났으므로, 불러와서 저장
        memset(buf, '\0', BUF_SIZE);
        k++;
    }
    printf("\n");
}

int split(char* string, char* seperator, char* argv[]){ // seperator를 delimeter로 사용
    int argc = 0;
    char* ptr = NULL;

    ptr = strtok(string, seperator);
    while(ptr != NULL){
        argv[argc++] = ptr;
        ptr = strtok(NULL, " ");
    }
    return argc;
}


void print_algorithm_type(int type_num) { // 수행할 알고리즘의 명칭 출력
    printf("[%s] Simulator\n", algorithm_type[type_num]);
}
void print_column() {  // 스트림과 그에 따른 결과 칼럼 출력
    printf("Stream\t\t");
    printf("Result\t\t");
    sprintf(print_buf, "Stream\t\tResult\t\t");
    write(fd, print_buf, PRINT_BUF_SIZE);
    memset(print_buf, '\0', PRINT_BUF_SIZE);

    for(int i=0; i<PAGEFRAME_NUM; i++){
        printf("Pageframe[%d]\t\t", i);
        sprintf(print_buf, "Pageframe[%d]\t\t", i);
        write(fd, print_buf, PRINT_BUF_SIZE);
        memset(print_buf, '\0', PRINT_BUF_SIZE);
    }
    printf("\n");
    write(fd, "\n", 2);
}
void print_pageframe(int stream, char* result) { // 각 알고리즘 함수 내에서 호출, page stream 별로 한 번씩 출력됨
    printf("%d\t\t%s\t\t", stream, result);
    sprintf(print_buf, "%d\t\t%s\t\t", stream, result);
    write(fd, print_buf, PRINT_BUF_SIZE);
    memset(print_buf, '\0', PRINT_BUF_SIZE);

    pageframe->tmp = pageframe->head;
    while(1){
        printf("%d\t\t\t", pageframe->tmp->page);
        sprintf(print_buf, "%d\t\t\t", pageframe->tmp->page);
        write(fd, print_buf, PRINT_BUF_SIZE);
        memset(print_buf, '\0', PRINT_BUF_SIZE);

        if(pageframe->tmp == pageframe->tail) break;
        pageframe->tmp = pageframe->tmp->next;
    }
    printf("\n");
    write(fd, "\n", 2);

}
void print_result() {
    printf("\n*******RESULT*******\n");
    printf(" [HIT] :  %d\n", HIT_CNT);
    printf("[MISS] :  %d\n", MISS_CNT);
    printf("********************\n\n\n");
    sprintf(print_buf, "\n*******RESULT*******\n [HIT] :  %d\n[MISS] :  %d\n********************\n", HIT_CNT, MISS_CNT);

    write(fd, print_buf, PRINT_BUF_SIZE);
    memset(print_buf, '\0', PRINT_BUF_SIZE);
}
void print_stream() {
    printf("stream : ");
    write(fd, "stream: ", 9);
    for(int i=0; i<PAGE_STREAM_NUM; i++){
        printf("%d ", RANDOM_STREAM[i]);
        sprintf(print_buf, "%d ", RANDOM_STREAM[i]);
        write(fd, print_buf, PRINT_BUF_SIZE);
        memset(print_buf, '\0', PRINT_BUF_SIZE);
    }
    printf("\n");
    write(fd, "\n", 2);
}

void print_Allpageframe() {
    if(pageframe->head == NULL) { return;}
    pageframe->tmp = pageframe->head;
    while (1){
        printf("%d", pageframe->tmp->page);
        if(pageframe->tmp == pageframe->tail) break;
        pageframe->tmp = pageframe->tmp->next;
        printf("->");
    }
    // printf("/ head: %d, tail: %d\n", pageframe->head->page, pageframe->tail->page);
    
}


Node* create_node(int page) { // 노드 생성
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->page = page;
    newNode->next = NULL;
    newNode->prev = NULL;
    newNode->ref_bit = 1;
    return newNode;
}
void append_node(int page) { // 노드 추가 (tail)
    MISS_CNT++;
    if(pageframe->head == NULL){ // 없으면 초기화
        pageframe->head = pageframe->tail = create_node(page);
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
void append_node_circular(int page) { // 노드 추가(SC 구현 시 사용 : 원형 연결 리스트)
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
    newNode->next = pageframe->head;
    pageframe->head->prev = newNode;
    pageframe->cur = pageframe->tail;  
    pageframe->size++;
    return;
}
Node* find_node(int page) { // head 부터 tail까지 순회하며 원하는 페이지가 있는지(hit) 여부 체크
    if(pageframe->head == NULL) {
        return NULL;
    }
    pageframe->tmp = pageframe->head;
    while(1){
        if(pageframe->tmp->page == page) return pageframe->tmp;
        if(pageframe->tmp == pageframe->tail) {break;}
        pageframe->tmp = pageframe->tmp->next;
    }
    return NULL; // Not Found
}
void initiate_Pageframe() { // 반복적으로 알고리즘을 실행하기 위해, page frame 초기화
    pageframe = (Pageframe*)malloc(sizeof(Pageframe));
    pageframe->head = pageframe->tail = NULL;
}

void Optimal() {
    HIT_CNT = MISS_CNT = 0;
    initiate_Pageframe();
    Node* tmp, *replaceNode, *newNode;
    int j, max, out;
    for(int i=0; i < PAGE_STREAM_NUM; i++){
        replaceNode = NULL;
        if (find_node(RANDOM_STREAM[i])) { // HIT 구간
            print_pageframe(RANDOM_STREAM[i], "HIT");
            continue;
        }
        if(pageframe->size < PAGEFRAME_NUM){ // append 구간 (프레임 개수보다 들어 온 페이지 개수가 적을 때)
            append_node(RANDOM_STREAM[i]);
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }

        tmp = pageframe->head;
        max = 0;
        out = 0;
        while(out == 0){
            // 교체 대상 찾기
            for(j = i; j < PAGE_STREAM_NUM; j++){
                if(tmp->page == RANDOM_STREAM[j]){ // 현재 page frame에 있는 page가 뒤에 또 등장하는 경우
                    if(max < j){ // 등장한 page가 현재까지 갱신된 max값보다 더 큰 경우
                        max = j; // 갱신하고
                        replaceNode = tmp; // 대체 노드를 현재 커서가 가리키는 노드로 설정함
                    }
                    break; // break를 통해 아래의 경우(마지막에 도달했고 && 갱신한 경우)를 따로 처리함.
                }
                if(j == PAGE_STREAM_NUM-1){ // 마지막에 도달했지만 해당 page가 없는 경우 : 가장 우선순위가 높음
                    replaceNode = tmp;
                    out = 1;
                    break; // 이번 loop에서는 더 이상 처리할 필요가 없기 때문에 탈출(out 조건)
                }
            }

            if(tmp == pageframe->tail) break;
            tmp = tmp->next;
        }

        // replaceNode가 정해지지 않은 경우 == 뒤에 같은 숫자가 안 나오는 경우
        if(replaceNode == NULL) replaceNode = pageframe->head;

    // replaceNode <-> newNode
        newNode = create_node(RANDOM_STREAM[i]);
        if(replaceNode == pageframe->head){ // head일 때 예외 처리
            newNode->next = pageframe->head->next;
            pageframe->head->next->prev = newNode;
            free(pageframe->head);
            pageframe->head = newNode;
            MISS_CNT++;
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }
        if(replaceNode == pageframe->tail){ // tail일 때 예외 처리
            newNode->prev = pageframe->tail->prev;
            pageframe->tail->prev->next = newNode;
            free(pageframe->tail);
            pageframe->tail = newNode;
            MISS_CNT++;
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }
        newNode->prev = replaceNode->prev;
        replaceNode->prev->next = newNode;
        newNode->next = replaceNode->next;
        replaceNode->next->prev = newNode;
        free(replaceNode);
        MISS_CNT++;
        print_pageframe(RANDOM_STREAM[i], "MISS");
        
    }

    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
}


void FIFO() {
    HIT_CNT = MISS_CNT = 0;
    initiate_Pageframe();
    Node* tmp;
    for(int i=0; i < PAGE_STREAM_NUM; i++){

        if (find_node(RANDOM_STREAM[i])) { // HIT 구간
            print_pageframe(RANDOM_STREAM[i], "HIT");
            continue;
        }
        if(pageframe->size < PAGEFRAME_NUM){ // append 구간 (프레임 개수보다 들어 온 페이지 개수가 적을 때)
            append_node(RANDOM_STREAM[i]);
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }
        
        /* head 없애고 */
        tmp = pageframe->head->next;
        pageframe->head->next = NULL;
        free(pageframe->head);
        pageframe->head = tmp;

        /* tail(last in) node 추가 */
        append_node(RANDOM_STREAM[i]);
        print_pageframe(RANDOM_STREAM[i], "MISS");

    }

    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
}

void LIFO() {
    HIT_CNT = MISS_CNT = 0;
    initiate_Pageframe();
    Node *tmp, *newNode;
    for (int i = 0; i < PAGE_STREAM_NUM; i++){

        if (find_node(RANDOM_STREAM[i])){ // HIT 구간
            print_pageframe(RANDOM_STREAM[i], "HIT");
            continue;
        }
        if (pageframe->size < PAGEFRAME_NUM){ // append 구간 (프레임 개수보다 들어 온 페이지 개수가 적을 때)
            append_node(RANDOM_STREAM[i]);
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }

        /* 맨 뒤 노드 replace */
        newNode = create_node(RANDOM_STREAM[i]);
        tmp = pageframe->tail->prev;
        tmp->next = newNode;
        newNode->prev = tmp;
        free(pageframe->tail);
        pageframe->tail = newNode;
        MISS_CNT++;
        print_pageframe(RANDOM_STREAM[i], "MISS");
    }

    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
}

void LRU() {
    HIT_CNT = MISS_CNT = 0;
    initiate_Pageframe();
    Node *hitNode, *newNode, *tmp;
    for (int i = 0; i < PAGE_STREAM_NUM; i++){

        if (hitNode = find_node(RANDOM_STREAM[i])){ // HIT 구간
            /* change_sequence */
            if(pageframe->size == 1) {
                print_pageframe(RANDOM_STREAM[i], "HIT");
                continue;
            }
            if(pageframe->size > 1) {
                if(hitNode == pageframe->tail) { print_pageframe(RANDOM_STREAM[i], "HIT"); continue;}
                if(hitNode == pageframe->head) {
                    tmp = pageframe->head;
                    pageframe->head = pageframe->head->next;
                    tmp->next = NULL;
                    pageframe->head->prev = NULL;
                    pageframe->tail->next = tmp;
                    tmp->prev = pageframe->tail;
                    pageframe->tail = tmp;
                    print_pageframe(RANDOM_STREAM[i], "HIT");
                    continue;
                }
                hitNode->prev->next = hitNode->next;
                hitNode->next->prev = hitNode->prev;
                pageframe->tail->next = hitNode;
                hitNode->prev = pageframe->tail;
                hitNode->next = NULL;
                pageframe->tail = hitNode;
                print_pageframe(RANDOM_STREAM[i], "HIT");
                continue;
            }
        }

        if (pageframe->size < PAGEFRAME_NUM){ // append 구간 (프레임 개수보다 들어 온 페이지 개수가 적을 때)
            append_node(RANDOM_STREAM[i]);
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }

        // 1) newNode를 맨 뒤에 붙임
        newNode = create_node(RANDOM_STREAM[i]);
        pageframe->tail->next = newNode;
        newNode->prev = pageframe->tail;
        // 2) tail = newnode 
        pageframe->tail = newNode;
        // 3) head = head->next
        tmp = pageframe->head->next;
        tmp->prev = NULL;
        free(pageframe->head);
        pageframe->head = tmp;
        MISS_CNT++;
        print_pageframe(RANDOM_STREAM[i], "MISS");
    }
    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
}

void LFU() {
    HIT_CNT = MISS_CNT = 0;
    initiate_Pageframe();
    Node *tmp, *minNode, *newNode;
    int min;
    for (int i = 0; i < PAGE_STREAM_NUM; i++){

        if (find_node(RANDOM_STREAM[i])) { // HIT 구간
            reference_cnt[RANDOM_STREAM[i]]++;
            print_pageframe(RANDOM_STREAM[i], "HIT");
            continue;
        }
        if (pageframe->size < PAGEFRAME_NUM) { // append 구간 (프레임 개수보다 들어 온 페이지 개수가 적을 때)
            append_node(RANDOM_STREAM[i]);
            reference_cnt[RANDOM_STREAM[i]]++;
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }

        tmp = pageframe->head;
        min = BIGNUM;
        while(1){ // 현재 page frame에서 가장 빈도수가 낮은 page 찾는 과정
            if(min > reference_cnt[tmp->page]){
                min = reference_cnt[tmp->page];
                minNode = tmp;
            }
            if(tmp == pageframe->tail) break;

            tmp = tmp->next;
        }
        // minNode : 최소 참조 횟수를 가진 노드 == 교체 대상

        newNode = create_node(RANDOM_STREAM[i]);
        reference_cnt[RANDOM_STREAM[i]]++;
        // newNode : minNode와 교체할 새로운 노드

        if(minNode == pageframe->head){ // 교체 노드가 head인 경우
            newNode->next = pageframe->head->next;
            pageframe->head->next->prev = newNode;
            free(pageframe->head);
            pageframe->head = newNode;
            MISS_CNT++;
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }
        if(minNode == pageframe->tail){ // 교체 노드가 tail인 경우
            pageframe->tail->prev->next = newNode;
            newNode->prev = pageframe->tail->prev;
            free(pageframe->tail);
            pageframe->tail = newNode;
            MISS_CNT++;
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }
        minNode->prev->next = newNode;
        newNode->prev = minNode->prev;
        minNode->next->prev = newNode;
        newNode->next = minNode->next;
        free(minNode);
        MISS_CNT++;
        print_pageframe(RANDOM_STREAM[i], "MISS");
    }

    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
}

void SC() {
    HIT_CNT = MISS_CNT = 0;
    initiate_Pageframe();
    Node *tmp, *newNode;

    for (int i = 0; i < PAGE_STREAM_NUM; i++){

        if (find_node(RANDOM_STREAM[i])) { // HIT 구간
            print_pageframe(RANDOM_STREAM[i], "HIT");
            continue;
        }
        if (pageframe->size < PAGEFRAME_NUM) { // append 구간 (프레임 개수보다 들어 온 페이지 개수가 적을 때)
            append_node_circular(RANDOM_STREAM[i]); // 원형 연결 리스트
            print_pageframe(RANDOM_STREAM[i], "MISS");
            continue;
        }

        tmp = pageframe->cur;
        newNode = create_node(RANDOM_STREAM[i]);
        while(1){
            if(tmp->ref_bit == 1) { // 참조 비트가 1인 경우 : 0으로 바꾸고 다음으로 커서 이동
                tmp->ref_bit = 0;
                tmp = tmp->next;
                continue;
            }
            if(tmp->ref_bit == 0) { // 참조 비트가 0인 경우 : 교체 대상
                if(tmp == pageframe->head){ // 교체 대상이 head인 경우
                    newNode->next = pageframe->head->next;
                    pageframe->head->next->prev = newNode;
                    newNode->prev = pageframe->tail;
                    pageframe->tail->next = newNode;
                    free(pageframe->head);
                    pageframe->head = newNode;
                    MISS_CNT++;
                    print_pageframe(RANDOM_STREAM[i], "MISS");
                    pageframe->cur = newNode->next;
                    break;
                }
                if(tmp == pageframe->tail){ // 교체 대상이 tail인 경우
                    newNode->next = pageframe->head;
                    pageframe->head->prev = newNode;
                    newNode->prev = pageframe->tail->prev;
                    pageframe->tail->prev->next = newNode;
                    free(pageframe->tail);
                    pageframe->tail = newNode;
                    MISS_CNT++;
                    print_pageframe(RANDOM_STREAM[i], "MISS");
                    pageframe->cur = newNode->next;
                    break;
                }
                tmp->prev->next = newNode;
                newNode->prev = tmp->prev;
                tmp->next->prev = newNode;
                newNode->next = tmp->next;
                free(tmp);
                MISS_CNT++;
                print_pageframe(RANDOM_STREAM[i], "MISS");
                pageframe->cur = newNode->next;
                break;
            }
        }
    }

    HIT_CNT = PAGE_STREAM_NUM - MISS_CNT;
}

void ALL() {
    for(int i=1; i<7; i++){
        print_algorithm_type(i);
        if((fd = open(algorithm_type_filename[i], O_RDWR | O_CREAT | O_TRUNC)) < 0){
            printf("open error in %s\n", algorithm_type_filename[i]);
            exit(1);
        }
        print_stream();
        print_column();
        switch (i){
            case 1:
                Optimal();
                break;
            case 2:
                FIFO();
                break;
            case 3:
                LIFO();
                break;
            case 4:
                LRU();
                break;
            case 5:
                LFU();
                break;
            case 6:
                SC();
                break;
        }
        print_result();
        close(fd);
    }
}
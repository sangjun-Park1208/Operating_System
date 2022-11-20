#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>


typedef struct Node {
    struct Node* next;
    struct Node* prev;
    int page;
    int ref_cnt; 
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

Node* create_node(int page) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    newNode->page = page;

    return newNode;
}
void append_node(int page) { // pageframe->size 가 입력받은 pageframe 크기보다 작을 때는 무조건 append
    if(pageframe->head == NULL){ // 없으면 초기화
        printf("초기화 상태에서 추가됨\n");
        pageframe->head = pageframe->tail = pageframe->cur = create_node(page);
        pageframe->size++;
        return;
    }
    printf("뒤에 추가됨\n");
    Node* newNode = create_node(page);
    pageframe->tail->next = newNode;
    newNode->prev = pageframe->tail;
    pageframe->tail = newNode;
    pageframe->size++;
    return;
}
int find_node(int page) { 
    pageframe->tmp = pageframe->head;

    while(1){
        if(pageframe->tmp->page == page) return 1;
        if(pageframe->tmp->page == pageframe->tail->page) return 0;
        pageframe->tmp = pageframe->tmp->next;
    }



    // while (pageframe->tmp->page != pageframe->tail->page){
    //     if (pageframe->tmp->page == page)
    //         return 1;
    //     pageframe->tmp = pageframe->tmp->next;
    // }
    return 0; // Not Found
}

void initiate_Pageframe() {
    pageframe = (Pageframe*)malloc(sizeof(Pageframe));
    pageframe->head = pageframe->tail = pageframe->cur = NULL;
}

void print_pageframe() {
    pageframe->tmp = pageframe->head;
    while(1){
        printf("%d", pageframe->tmp->page);
        if(pageframe->tmp == pageframe->tail) {
            printf("끝에 도달함");
            break;
        }
        printf(" -> ");
        pageframe->tmp = pageframe->tmp->next;
    }
    printf("\n");
}

int main() {
    initiate_Pageframe();
    append_node(0);
    append_node(1);
    append_node(2);
    print_pageframe();
}
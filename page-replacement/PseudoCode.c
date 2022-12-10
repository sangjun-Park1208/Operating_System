Optimal () {
    HIT_CNT = MISS_CNT = 0;
    Pageframe 초기화;

    for(int i=0; i < pageframe 개수; i++){
        if(hit 한 경우){
            표준 출력;
            continue;
        }

        if(현재 pageframe 내의 페이지 개수 < pageframe 최대 개수){ 
            // 개수가 더 작으므로, 무조건 append 되는 구간
            appendNode(page);
            표준 출력;
            continue;
        }

        while (교체 대상을 찾을 때까지){
            for (int j = i; j < pageframe 개수; j++) {
                if (현재 커서가 가리키는 페이지가 나중에 등장하는 경우){
                    if (그 페이지의 인덱스가 더 멀리 떨어진 경우) {
                        max = j;
                        replaceNode = 현재 커서;
                    } // 갱신 작업
                    break;
                }
                if (현재 커서가 tail에 도달한 경우){
                    // 끝까지 다시 사용되지 않음을 확인함.
                    // 가장 높은 우선순위를 부여하기 때문에, 반복문 탈출
                    replaceNode = 현재 커서;
                    break;
                }
            }
            if (현재 커서 == tail) break;
            현재 커서 = 다음 커서;
        }

        if (교체 대상이 head인 경우){
            교체 코드;
            continue;
        }
        if (교체 대상이 tail인 경우){
            교체 코드;
            continue;
        }
        교체 코드; // head도, tail도 아닌 가운데 있는 경우
    }

}

FIFO() {
    HIT_CNT = MISS_CNT = 0;
    Page frame 초기화;
    for(int i=0; i < pageframe 개수; i++){
        if(hit 한 경우){
            표준 출력;
            continue;
        }

        if(현재 pageframe 내의 페이지 개수 < pageframe 최대 개수){ 
            // 개수가 더 작으므로, 무조건 append 되는 구간
            appendNode(page);
            표준 출력;
            continue;
        }

        head 삭제;
        tail 뒤에 append();
    }
}

LIFO() {
    HIT_CNT = MISS_CNT = 0;
    Page frame 초기화;
    for(int i=0; i < pageframe 개수; i++){
        if(hit 한 경우){
            표준 출력;
            continue;
        }

        if(현재 pageframe 내의 페이지 개수 < pageframe 최대 개수){ 
            // 개수가 더 작으므로, 무조건 append 되는 구간
            appendNode(page);
            표준 출력;
            continue;
        }

        
        tail 뒤에 replace();
    }
}

LRU() {
    HIT_CNT = MISS_CNT = 0;
    Page frame 초기화;
    for(int i=0; i < pageframe 개수; i++){
        if(hit 한 경우){
            if(page frame에 head 하나만 있는 경우){
                표준 출력;
                continue;
            }
            if(여러 개 있는 경우){
                if(hitNode가 tail인 경우) { 표준 출력; continue; }
                if(hitNode가 head인 경우) {
                    hitNode 위치를 tail 뒤로 옮김;
                    표준 출력;
                    continue;
                }
                // hitNode가 가운데 있는 경우
                next, prev포인터 조정을 통해 위치 replace();
                표준 출력;
                continue;
            }
        }

        if(현재 pageframe 내의 페이지 개수 < pageframe 최대 개수){ 
            // 개수가 더 작으므로, 무조건 append 되는 구간
            appendNode(page);
            표준 출력;
            continue;
        }

        newNode를 tail 뒤에 append;
        tail = newNode;
        head = head->next;

        
    }
}

LFU() {
    HIT_CNT = MISS_CNT = 0;
    Page frame 초기화;
     for(int i=0; i < pageframe 개수; i++){
        if(hit 한 경우){
            표준 출력;
            continue;
        }

        if(현재 pageframe 내의 페이지 개수 < pageframe 최대 개수){ 
            // 개수가 더 작으므로, 무조건 append 되는 구간
            appendNode(page);
            표준 출력;
            continue;
        }

        min = 최댓값(999999);
        while(1){
            if(min > 페이지 별 참조 배열[현재 커서의 page 번호]){
                min 값 갱신;
                minNode = 현재 커서;
            }
            if(현재 커서 == tail) break;
            현재 커서 = 다음 커서;
        }
        // minNode : 최소 참조 횟수를 가진 노드 == 교체 대상

        newNode = createNode(page);

        if(minNode == head){
            replace();
            표준 출력;
            continue;
        }
        if(minNode == tail){
            replace();
            표준 출력;
            continue;
        }
        // minNode가 가운데 있는 경우
        replace();
        표준 출력;   
    }
}

SC () {
    HIT_CNT = MISS_CNT = 0;
    Page frame 초기화;
     for(int i=0; i < pageframe 개수; i++){
        if(hit 한 경우){
            표준 출력;
            continue;
        }

        if(현재 pageframe 내의 페이지 개수 < pageframe 최대 개수){ 
            // 개수가 더 작으므로, 무조건 append 되는 구간
            appendNode(page);
            표준 출력;
            continue;
        }

        while(1){
            if(현재 커서의 참조비트 == 1){
                참조비트 = 0;
                현재 커서 = 다음 커서;
                continue;
            }
            if(현재 커서의 참조비트 == 0){
                if(현재 커서 == head){
                    replace();
                    표준 출력;
                    break;
                }
                if(현재 커서 == tail){
                    replace();
                    표준 출력;
                    break;
                }
                // 가운데 노드인 경우                    
                replace();
                표준 출력;
                break;
            }
        }
    }
}
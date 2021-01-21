/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

// mdriver 구동을 위한 team정보 struct 설정
team_t team = {
    /* Team name */
    "jungle",
    /* First member's full name */
    "Saihim Cho",
    /* First member's email address */
    "saihimcho@kaist.ac.kr",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// *** Variables *** //
#define WSIZE 4 // word and header footer 사이즈를 byte로.
#define DSIZE 8 // double word size를 byte로
#define CHUNKSIZE (1 << 12)

static char *heap_listp = 0;
static char *start_nextfit = 0;

//******MACROs*************//
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// size를 pack하고 개별 word 안의 bit를 할당 (size와 alloc을 비트연산)
#define PACK(size, alloc) ((size) | (alloc))

/* address p위치에 words를 read와 write를 한다. */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

// address p위치로부터 size를 읽고 field를 할당
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* given block ptr bp, 그것의 header와 footer의 주소를 계산*/
#define HDRP(bp) ((char *)(bp)-WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* GIVEN block ptr bp, 이전 블록과 다음 블록의 주소를 계산*/
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE(((char *)(bp)-DSIZE)))

//*******Functions*********//

/* 우성 : 아주 멋진 프로토타입 정리네요. 매우 C스럽고 깔끔합니다! 저희도 차용하겠습니다 */
int mm_init(void);
static void *extend_heap(size_t words);
void *mm_malloc(size_t size);
void mm_free(void *ptr);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  /* create 초기 빈 heap*/
  if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
  {
    return -1;
  }
  PUT(heap_listp, 0);
  PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
  heap_listp += (2 * WSIZE);
  /* 우성 : 훌륭한 타이밍 입니다. */
  start_nextfit = heap_listp;

  if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    return -1;
  return 0;
}

static void *extend_heap(size_t words)
{ // 새 가용 블록으로 힙 확장
  char *bp;
  size_t size;
  /* alignment 유지를 위해 짝수 개수의 words를 Allocate */
  size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
  if ((long)(bp = mem_sbrk(size)) == -1)
  {
    return NULL;
  }
  /* free block 헤더와 푸터를 init하고 epilogue 헤더를 init*/
  PUT(HDRP(bp), PACK(size, 0));         // free block header
  PUT(FTRP(bp), PACK(size, 0));         // free block footer
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // new epilogue header 추가

  /* 만약 prev block이 free였다면, coalesce해라.*/
  return coalesce(bp);
}

static void *coalesce(void *bp)
{
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc)
  { // case 1 - 이전과 다음 블록이 모두 할당 되어있는 경우, 현재 블록의 상태는 할당에서 가용으로 변경
    return bp;
  }
  else if (prev_alloc && !next_alloc)
  {                                        // case2 - 이전 블록은 할당 상태, 다음 블록은 가용상태. 현재 블록은 다음 블록과 통합 됨.
    size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더만큼 사이즈 추가?
    PUT(HDRP(bp), PACK(size, 0));          // 헤더 갱신
    PUT(FTRP(bp), PACK(size, 0));          // 푸터 갱신
  }
  else if (!prev_alloc && next_alloc)
  { // case 3 - 이전 블록은 가용상태, 다음 블록은 할당 상태. 이전 블록은 현재 블록과 통합.
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));

    /* 우성 : 깨달은 내용이실 수 있지만 아래 "?"가 저에게 주석을 남기게 만들어 주석을 남깁니다." */
    /* mm.c 에서는 NEXT_BLOCK에 접근하기 위해서는 자신의 Header를 이용하고, 
    PREV_BLKP에 접근하기 위해서는 이전 녀석의 Footer(본인보다 DSIZE 앞에있는) 에 접근합니다. 
    따라서 나의 (병합되면서도 Footer의 신분을 유지할)Footer를 선 변경해주는 행동이 
    PREV_BLKP과 NEXT_BLKP에 접근하는 것에 전혀 영향을 주지 않습니다. 
    그래서 Footer에 먼저 size 삽입을 진행하고 곧 나의 Header가 될 이전녀석의 Header에 
                        이전녀석의 Footer를 타고 접근하여 size를 Pack해주는 순서로 구성되어있습니다.
    
    아래와 같은 코드로 대체도 가능합니다!
    bp = PREV_BLKP(bp);
    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    
    
    알고 계실 것 같지만 키보드 타이핑 하고싶어서 길게 적어보았습니다.ㅎㅎ*/

    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 헤더를 이전 블록의 BLKP만큼 통합?
    bp = PREV_BLKP(bp);
  }
  else
  {                                                                        // case 4- 이전 블록과 다음 블록 모두 가용상태. 이전,현재,다음 3개의 블록 모두 하나의 가용 블록으로 통합.
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 블록 헤더, 다음 블록 푸터 까지로 사이즈 늘리기
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  /* 우성 : 엄밀하게 말하면 아래 행위(start_nextfit = bp)는 next fit이 아니라고 생각합니다. 
  next fit은 (malloc을 진행 하면서) 검색을 마친 후 검색 포인트를 마지막 검색 포인트에 그대로 두고 
  이후에 그 point부터 탐색을 진행하는 방법입니다.

  하지만 coalesce는 malloc을 진행하면서 실행되는 함수가 아니라, 
  free가 진행될 때 혹은 extend heap이 진행될 때 사용됩니다.
  
  그럼 왜 버그가 날까용
  마지막으로 할당한 block이 free가 되고 앞의 block과 합쳐지면 nextfit의 행방은 어떻게 될까요?
  전혀 의미가 없는 위치인 block 사이에 끼어있습니다. 이때만 nextfit을 bp로 변경해줘야합니다.


  아래 코드는 free가 될 때, extend heap이 될때, 조건에 상관없이 모두 nextfit pointer를 변경해주기 때문에 
  필요한 조건을 포함하는 더 큰 조건에서 start_nextfit을 변경해주고 있어서
  next fit이 아니라고 생각했습니다. 
  
  저의 짧은 식견이오니 저에게 누나의 의견을 꼭 들려주세요.

  */

  start_nextfit = bp;

  return bp;
}

void *mm_malloc(size_t size)
{
  size_t asize;      // 블록 사이즈 조정
  size_t extendsize; // heap에 맞는 fit이 없으면 확장하기 위한 사이즈
  char *bp;

  /* 거짓된 요청 무시*/
  if (size == 0)
    return NULL;

  /* overhead, alignment 요청 포함해서 블록 사이즈 조정*/
  if (size <= DSIZE)
  {
    asize = 2 * DSIZE;
  }
  else
  {
    asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
  }
  /* fit에 맞는 free 리스트를 찾는다.*/
  if ((bp = find_fit(asize)) != NULL)
  {
    place(bp, asize);
    return bp;
  }
  /* fit 맞는게 없다. 메모리를 더 가져와 block을 위치시킨다.*/
  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
  {
    return NULL;
  }
  place(bp, asize);
  return bp;
}

static void *find_fit(size_t asize)
{ // next fit 검색을 수행
  void *bp;

  for (bp = start_nextfit; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
  {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    {
      return bp;
    }
  }
  for (bp = heap_listp; bp != start_nextfit; bp = NEXT_BLKP(bp))
  {
    if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
    {
      return bp;
    }
  }
  return NULL;
}

static void place(void *bp, size_t asize)
{ // 요청한 블록을 가용 블록의 시작 부분에 배치, 나머지 부분의 크기가 최소 블록크기와 같거나 큰 경우에만 분할하는 함수.
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - asize) >= (2 * DSIZE))
  {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize - asize, 0));
    PUT(FTRP(bp), PACK(csize - asize, 0));
  }
  else
  {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

/*
 * mm_free - Freeing a block does nothing.
 */
// 블록을 반환하고 경계 태그 연결 사용 -> 상수 시간 안에 인접한 가용 블록들과 통합하는 함수들
void mm_free(void *bp)
{
  size_t size = GET_SIZE(HDRP(bp));
  PUT(HDRP(bp), PACK(size, 0)); // header, footer 들을 free 시킨다.
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{

  void *oldptr = ptr;
  void *newptr;
  size_t copySize;

  /* 우성 : 아래 realloc은 현재 메모리에 뒷공간이 충분히 남아 있어도 
  그 곳이 아닌(아닐 확률이 매우높은) 위치에 먼저 size만큼 할당을 진행한 후에 복사를 합니다.
  
  여유가 있으시면 위에서 언급한 경우 즉, 
  현재 메모리 뒷공간을 활용하여 재할당할 수 있는지 확인하는 조건문을 추가하여
  그 행동을 우선시 하게 만들어보세요.

  점수가 올라갑니다!
  */

  newptr = mm_malloc(size);
  if (newptr == NULL)
    return NULL;
  copySize = GET_SIZE(HDRP(oldptr));
  if (size < copySize)
    copySize = size;
  memcpy(newptr, oldptr, copySize);
  mm_free(oldptr);
  return newptr;
}

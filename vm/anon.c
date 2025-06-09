/* anon.c: 디스크 이미지가 아닌 페이지(익명 페이지)의 구현 */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "devices/disk.h"
#include "kernel/bitmap.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);
struct bitmap *swap_table;
struct lock bitmap_lock;

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages
 * 익명 페이지 전체 스왑 공간을 초기화 - swap_disk 가져오기, swap_table(bitmap) 생성, bitmap_lock 초기화 */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	if (swap_disk == NULL) {
		PANIC("No swap disk found!");
	}

	swap_table = bitmap_create (disk_size(swap_disk) / (PGSIZE / DISK_SECTOR_SIZE));
	if (swap_table == NULL) {
		PANIC("Failed to create swap bitmap!");
	lock_init(&bitmap_lock);
	}
}

/* Initialize the file mapping 
 * 익명 페이지로 변환 시 호출되는 초기화 함수. anon_ops 설정 및 swap_idx 초기화 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) 
{	
	// 예외 처리
	if (page == NULL) {
		return false;
	}

	page->operations = &anon_ops;	// 이 페이지가 anon page임을 표시

	struct anon_page *anon_page = &page->anon;	// 현재 페이지의 anon_page 구조체를 참조, uninit → anon으로 타입 전환하며 초기화 단계 수행
	anon_page->swap_idx = BITMAP_ERROR;			// 유효하지 않은 인덱스(-1)를 사용해 아직 스왑되지 않은 상태로 초기화

	// /* 보안 및 예측 가능한 동작 보장을 위한 선택적 초기화 코드:
	//  * 페이지의 물리 주소(kva)가 유효하며, 해당 페이지의 frame이 1개 이하로만 참조되고 있을 경우,
	//  * 새로 할당된 물리 페이지라고 간주하고 물리페이지 전체를 0으로 초기화 (보안 및 예측 가능한 동작 보장) */
	// if (kva != NULL && page->frame->ref_cnt <= 1) {
	// 	memset (kva, 0, PGSIZE);
	// }

	return true;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) 
{
	struct anon_page *anon_page = &page->anon;

	size_t swap_idx = anon_page->swap_idx;
	ASSERT(swap_idx != BITMAP_ERROR);			// swap_idx는 정상 값이어야 함
	ASSERT(bitmap_test(swap_table, swap_idx));	// 해당 slot은 반드시 사용중이어야 함 (true)

	for (int i = 0; i < 8; i++) {
		disk_read(swap_disk, (swap_idx * 8) + i, kva + (DISK_SECTOR_SIZE * i));
	}

	page->frame->kva = kva;
	
	lock_acquire(&bitmap_lock);		// 동시에 다른 thread가 bitmap을 scan+flip 못 하게 serialize 처리
	bitmap_reset(swap_table, swap_idx);
	lock_release(&bitmap_lock);
	
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) 
{
	// 페이지 유효성 확인
	if (page == NULL) {
		return false;
	}
	
	// 현재 페이지의 anon_page 구조체 참조
	struct anon_page *anon_page = &page->anon;

	// 빈 swap slot 할당 (scan + flip 과정 동기화)
	lock_acquire(&bitmap_lock);		// 동시에 다른 thread가 bitmap을 scan+flip 못 하게 serialize 처리
	size_t swap_idx = bitmap_scan_and_flip(swap_table, 0, 1, false);
	lock_release(&bitmap_lock);

	// swap slot 할당 실패 시 처리
	if (swap_idx == BITMAP_ERROR) {
		ASSERT(bitmap_test(swap_table, swap_idx) == false);	// 해당 비트는 flip되지 않았어야 정상 → bitmap_test()로 검증
		return false;		// 할당 실패했으므로 해당 bit는 여전히 false여야 함
	}

	// anon_page에 swap 위치 기록 → 이후 swap_in 시 사용
	anon_page->swap_idx = swap_idx;

	// 현재 페이지의 내용을 swap_disk에 저장 (8 sector 단위로 저장)
	for (int i = 0; i < 8; i++) {
		disk_write (swap_disk, swap_idx * 8 + i, page->frame->kva + DISK_SECTOR_SIZE * i);
	}

	// 프레임 연결 해제
	page->frame->page = NULL;	// frame과 page 연결 해제
	page->frame = NULL;			// page가 frame 가리키지 않게 초기화

	// 현재 가상 주소(pml4)에서 해당 물리 페이지 매핑 제거 (다음 access 시 page fault 발생 유도)
	pml4_clear_page (thread_current()->pml4, page->va);
	return true;				// 성공적으로 swap out 완료
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* anon.c: 디스크 이미지가 아닌 페이지(익명 페이지)의 구현 */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "devices/disk.h"

/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* 익명 페이지를 위한 데이터를 초기화하는 함수 */
void
vm_anon_init (void) {
	/* TODO: 스왑 디스크를 설정해야 합니다. */
	swap_disk = NULL;
}

/* anonymous 페이지 타입의 초기화를 수행하는 함수 */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* 페이지에 anon 페이지 핸들러를 설정합니다. */
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	struct anon_page *anon_page = &page->anon;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
}

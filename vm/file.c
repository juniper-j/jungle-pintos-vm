/* file.c: Implementation of memory backed file object (mmaped object). */

#include <stdlib.h>
#include "threads/mmu.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "vm/vm.h"
#include "vm/file.h"
#include "filesys/filesys.h"

extern struct lock filesys_lock;

static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) 
{
	/* 전역 자료구조 초기화 */
	/** TODO: mmap 리스트 초기화
	 * mmap으로 만들어진 페이지를 리스트로 관리하면
	 * munmmap 시에 더 빠르게 할 수 있을 듯 합니다
	 *
	 */
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva)
{
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;

	// TODO: page에 page->uninit.aux에 들어있는 정보를 구조체로 형변환 (ex. lazy_load_arg *)
	struct lazy_load_arg *aux = (struct lazy_load_arg *)page->uninit.aux;
	file_page->file = aux->file;
	file_page->ofs = aux->ofs;
	file_page->read_bytes = aux->read_bytes;
	file_page->zero_bytes = aux->zero_bytes;
	return true;
}

/* 파일에서 내용을 읽어와 페이지를 스왑인합니다. */
static bool
file_backed_swap_in(struct page *page, void *kva)
{
	struct file_page *file_page = &page->file;
	// struct file *file = file_page->file;
	// size_t read_bytes = file_page->read_bytes;
	// off_t ofs = file_page->ofs;
	
	// // 파일에서 데이터를 읽어와 kva(페이지가 매핑된 커널 가상 주소)에 저장 
	// if (file_read_at(file, kva, read_bytes, ofs) != (off_t)read_bytes) {
	// 	return false;
	// }
	
	file_read_at (file_page->file, kva, file_page->read_bytes, file_page->ofs);

	// // 파일에서 읽어오지 못한 나머지 페이지 영역을 0으로 초기화
	// memset(kva + read_bytes, 0, page->file.zero_bytes);
	return true;
}

/* 페이지의 내용을 파일에 기록(writeback)하여 스왑아웃합니다. */
static bool
file_backed_swap_out(struct page *page)
{
	struct file_page *file_page = &page->file;
	// struct file *file = file_page->file;
	// size_t read_bytes = file_page->read_bytes;
	// off_t ofs = file_page->ofs;
	struct thread *cur = thread_current();
	
	// bool dirty_bit = pml4_is_dirty(cur->pml4, page->va);
	
	// // dirty + writable → writeback 필요
	// if (dirty_bit && page->writable) 
	// {
	// 	lock_acquire(&filesys_lock);
	// 	if (file_write_at(file, page->frame->kva, read_bytes, ofs) != (off_t)read_bytes)
	// 	{
	// 		lock_release(&filesys_lock);
	// 		return false;
	// 	}	
	// 	lock_release(&filesys_lock);
	// 	pml4_set_dirty(cur->pml4, page->va, false);
	// }

	if (pml4_is_dirty(thread_current()->pml4, page->va) && page->writable) {
		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}
	
	page->frame->page = NULL;
	page->frame = NULL;
	
	pml4_clear_page(cur->pml4, page->va);	// (optional)
	
	return true;
}

/* 파일 기반 페이지를 소멸시킵니다. PAGE는 호출자가 해제합니다. */
static void
file_backed_destroy(struct page *page)
{
	struct file_page *file_page = &page->file;
	// struct file *file = file_page->file;
	// size_t read_bytes = file_page->read_bytes;
	// off_t ofs = file_page->ofs;
	struct thread *cur = thread_current();

	// // 파일을 쓰기 가능하게 설정 (read-only로 열렸을 가능성 있음)
	// file_allow_write(file_page->file);

	 // dirty + writable 체크 후 write-back
	 if (pml4_is_dirty(cur->pml4, page->va) && page->writable) 
	 {
		// lock_acquire(&filesys_lock);	
		// off_t written = file_write_at(file, page->frame->kva, read_bytes, ofs);
		// lock_release(&filesys_lock);
		// ASSERT(written == read_bytes);

		file_write_at(file_page->file, page->va, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(cur->pml4, page->va, false);
	 }

	 hash_delete(&thread_current()->spt.pages, &page->hash_elem);
	 if (page->frame) {
		list_remove(&page->frame->elem);
		page->frame->page = NULL;
		page->frame = NULL;
		free(page->frame);
	}

	//  // frame 해제
	//  if (page->frame != NULL)
	//  {
	// 	palloc_free_page(page->frame->kva);
	// 	free(page->frame);
	// 	page->frame = NULL;
	//  }
	 // 가상 주소 공간에서 매핑 제거
	 pml4_clear_page(cur->pml4, page->va);
}

/* do_mmap(): mmap 로직 전체를 책임 (페이지 등록, lazy loading, file 재열기 포함) 
 * 
 * lazy loading으로 할당
 * 파일에 대한 독립적인 참조
 * 여러 페이지에 매핑되는 파일 처리
 */
void *
do_mmap(void *addr, size_t length, int writable, struct file *file, off_t offset)
{
	void *start_addr = addr;				// 성공 시 반환할 원래 가상 주소

	// 파일 재열기 → 독립된 file 객체 필요
	lock_acquire(&filesys_lock);			
	struct file *reopened_file = file_reopen(file);	
	lock_release(&filesys_lock);			

	if (reopened_file == NULL) {
		return NULL;
	}

	// 실제로 읽어야 할 바이트 수 + zero padding 계산
	size_t read_bytes = (file_length(reopened_file) < length) ? file_length(reopened_file) : length;
	size_t zero_bytes = (read_bytes % PGSIZE == 0) ? 0 : PGSIZE - (read_bytes % PGSIZE);
	
	// 전체 매핑 범위에 이미 매핑된 페이지가 있는지 충돌 검사
	for (void *check = addr; check < addr + read_bytes + zero_bytes; check += PGSIZE) {
		if (spt_find_page(&thread_current()->spt, check) != NULL)
			return NULL;
	}

	// 정렬 조건 검사
	ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
	ASSERT(pg_ofs(addr) == 0);
	ASSERT(offset % PGSIZE == 0);

	// 페이지 단위로 매핑 수행
	int page_cnt = (read_bytes + zero_bytes) / PGSIZE;	// 전체 페이지 수
	size_t total_bytes = read_bytes + zero_bytes;		// 전체 바이트 수

	while (total_bytes > 0) 
	{
		// 현재 페이지에서 실제로 읽을 바이트 수와 남은 공간 zero padding 계산
		size_t page_read_bytes = (read_bytes < PGSIZE) ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		// lazy load용 인자 구조체 생성
		struct lazy_load_arg *aux = malloc(sizeof(struct lazy_load_arg));
		if (aux == NULL) 
		{	// 전체 실패 시 열린 파일 닫기
			file_close(reopened_file);
			return NULL;
		}

		aux->file = reopened_file;
		aux->ofs = offset;
		aux->read_bytes = page_read_bytes;
		aux->zero_bytes = page_zero_bytes;

		// lazy load로 VM_FILE 페이지 등록
		if (!vm_alloc_page_with_initializer(VM_FILE, addr, writable, lazy_load_segment, aux)) 
		{
			free(aux);
			file_close(reopened_file);
			return NULL;
		}

		// page->mmap_idx 저장
		struct page *p = spt_find_page(&thread_current()->spt, addr);
		ASSERT(p != NULL);
		p->mmap_idx = page_cnt;

		// 다음 페이지로 이동
		total_bytes -= PGSIZE;
		if (read_bytes >= page_read_bytes)
			read_bytes -= page_read_bytes;
		else
			read_bytes = 0;

		if (zero_bytes >= page_zero_bytes)
			zero_bytes -= page_zero_bytes;
		else
			zero_bytes = 0;

		addr += PGSIZE;
		offset += page_read_bytes;
	}	

	return start_addr;
}

/** TODO: mmap으로 매핑된 모든 페이지를 없애야함
 * 1. SPT에서 제거
 * 2. 물리 페이지에서도 제거
 * 3. 매핑 카운트나 page 구조체 내의 카운트를 사용해서 제거
 */
void do_munmap(void *addr) 
{
	struct supplemental_page_table *spt = &thread_current()->spt;

	lock_acquire(&filesys_lock);

	struct page *page = spt_find_page(spt, addr);
	if (page == NULL) {
		lock_release(&filesys_lock);
		return;
	}

	int mmap_idx = page->mmap_idx;

	// mmap_idx 만큼 반복하며 페이지 해제 수행
	for (int i = 0; i < mmap_idx; i++) {
		struct page *target = spt_find_page(spt, addr);
		if (target == NULL) {
			addr += PGSIZE;
			continue;
		}
	
		// TODO: dirty 처리
		spt_remove_page(spt, target);
		addr += PGSIZE;
	}
	lock_release(&filesys_lock);
}

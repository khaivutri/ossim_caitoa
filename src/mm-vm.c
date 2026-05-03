/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <mm64.h>
/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  if (mm == NULL) return NULL;
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = pvma->vm_id;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
    if (pvma == NULL) // Kiểm tra ngay sau khi chuyển
      return NULL;
    vmait = pvma->vm_id;
  }

  return pvma;
}

int __mm_swap_page(struct pcb_t *caller, addr_t vicfpn , addr_t swpfpn)
{
    __swap_cp_page(caller->krnl->mram, vicfpn, caller->krnl->active_mswp, swpfpn);
    return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, addr_t size, addr_t alignedsz)
{
  struct vm_rg_struct * newrg;
  /* 1. Validation an toàn để chống Crash */
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return NULL;
  }
  /* TO-DO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/

  /* TO-DO: update the newrg boundary
  */
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL) return NULL;
  newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL) {
      return NULL; // Lỗi: Hết RAM thực sự của máy tính
  }
  newrg->vmaid = vmaid;
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;
  newrg->rg_next = NULL;
  /* END TO-DO */

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, addr_t vmastart, addr_t vmaend)
{
  /* 1. Validation an toàn */
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL)
  {
    return -1;
  }
  /* TO-DO validate the planned memory area is not overlapped */
  /* 2. Kích thước dự kiến không hợp lệ (Bắt đầu lại lớn hơn hoặc bằng Kết thúc) */
  if (vmastart >= vmaend)
  {
    return -1;
  }

  struct vm_area_struct *vma = caller->mm->mmap;
  if (vma == NULL)
  {
    return -1;
  }

  /* TO-DO validate the planned memory area is not overlapped */

  while (vma != NULL)
  {
    if (vma->vm_id != vmaid) 
    {
      /* CÔNG THỨC KIỂM TRA OVERLAP:
       * Vùng 1: [vmastart, vmaend)
       * Vùng 2: [vma->vm_start, vma->vm_end)
       * Chồng lấn xảy ra nếu Vùng 1 bắt đầu trước khi Vùng 2 kết thúc 
       * VÀ Vùng 2 bắt đầu trước khi Vùng 1 kết thúc.
       */
      if (vmastart < vma->vm_end && vma->vm_start < vmaend)
      {
        return -1; /* BÁO ĐỘNG: Phát hiện chồng lấn! */
      }
    }
    
    /* Chuyển sang VMA tiếp theo để kiểm tra */
    vma = vma->vm_next;
  }
  /* End TODO*/

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, addr_t inc_sz)
{
  if (caller == NULL || caller->krnl == NULL || caller->mm == NULL) {
      return -1; 
  }
  if (inc_sz == 0) {
      return 0; // Không cần làm gì cả, coi như thành công
  }
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
  if (cur_vma == NULL) {
      return -1; // Lỗi: Vùng nhớ yêu cầu nới rộng không tồn tại
  }
  //struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));

  /* TO-DO with new address scheme, the size need tobe aligned 
   *      the raw inc_sz maybe not fit pagesize
   */ 
  addr_t inc_amt;
  int incnumpage;

/* Làm tròn kích thước lên mức Trang (Page Alignment) */
#ifdef MM64
    inc_amt = PAGING64_PAGE_ALIGNSZ(inc_sz);
    incnumpage = inc_amt / PAGING64_PAGESZ;
#else
    inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
    incnumpage = inc_amt / PAGING_PAGESZ;
#endif
  /* TODO Validate overlap of obtained region */
  /* Xác định các cột mốc ranh giới ảo */
    addr_t old_end = cur_vma->vm_end;
    addr_t new_end = old_end + inc_amt;
  if (validate_overlap_vm_area(caller, vmaid, old_end, new_end) < 0) {
      return -1; /* Overlap and failed allocation */
  }

  /* TODO: Obtain the new vm area based on vmaid */
  //cur_vma->vm_end... 
  // inc_limit_ret...
  /* The obtained vm area (only)
   * now will be alloc real ram region */
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  if (newrg == NULL) {
      return -1;
  }
  newrg->rg_start = old_end;
  newrg->rg_end = new_end;
  newrg->rg_next = NULL;
  newrg->vmaid = vmaid;

  cur_vma->vm_end = new_end;
  cur_vma->sbrk += inc_sz;

  if (vm_map_ram(caller, old_end, new_end, old_end, incnumpage, newrg) < 0) {
      /* BẮT LỖI TỐI QUAN TRỌNG (ROLLBACK): 
        * Nếu hết RAM vật lý, vm_map_ram sẽ thất bại. 
        * Ta phải rút lại sổ đỏ, hoàn trả lại ranh giới cũ để không làm hỏng tiến trình!
        */
      cur_vma->vm_end = old_end;
      cur_vma->sbrk -= inc_sz;
      free(newrg);
      return -1; 
  }

  return 0;
}

// #endif

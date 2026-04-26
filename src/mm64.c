/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm64.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// /* Tra cứu Memory Context thông qua Kernel Registry (Danh bạ) */
// struct mm_struct *get_mm_of_proc(struct krnl_t *krnl, uint32_t pid) {
//     if (krnl == NULL || pid >= PID_MAX) {
//         return NULL;
//     }

//     struct pcb_t *proc = krnl->proc_table[pid];
//     if (proc == NULL) {
//         return NULL; // PID này chưa được nạp
//     }

//     return proc->mm; // An toàn trả về không gian bộ nhớ của tiến trình
// }
#ifdef MM64
// Hàm Helper: Trả về địa chỉ con trỏ của PTE thật sự trong cây 5 cấp
static uint64_t *get_pte_ptr(struct pcb_t *caller, addr_t pgn, int is_alloc) {
    if (caller == NULL || caller->krnl == NULL)
        return NULL;

    struct mm_struct *mm = caller->krnl->mm;
    if (mm == NULL)
        return NULL; // Guard chống lỗi

    addr_t pgd, p4d, pud, pmd, pt;

    get_pd_from_pagenum(pgn, &pgd, &p4d, &pud, &pmd, &pt);

    if (mm->pgd == NULL)
        return NULL; // Root chưa được khởi tạo

    // Tầng 1: PGD -> P4D
    if (mm->pgd[pgd] == 0) {
        if (!is_alloc)
            return NULL; // Nếu chỉ đọc mà chưa cấp phát thì trả về NULL
        mm->pgd[pgd] = (uint64_t)calloc(PAGING64_DIR_ENTRIES, sizeof(uint64_t));
        // Xử lý lỗi cấp phát thất bại
        if (mm->pgd[pgd] == 0)
            return NULL;
    }
    uint64_t *p4d_tbl = (uint64_t *)mm->pgd[pgd];

    // Tầng 2: P4D -> PUD
    if (p4d_tbl[p4d] == 0) {
        if (!is_alloc)
            return NULL;
        p4d_tbl[p4d] = (uint64_t)calloc(PAGING64_DIR_ENTRIES, sizeof(uint64_t));
        // Xử lý lỗi cấp phát thất bại
        if (p4d_tbl[p4d] == 0)
            return NULL;
    }
    uint64_t *pud_tbl = (uint64_t *)p4d_tbl[p4d];

    // Tầng 3: PUD -> PMD
    if (pud_tbl[pud] == 0) {
        if (!is_alloc)
            return NULL;
        pud_tbl[pud] = (uint64_t)calloc(PAGING64_DIR_ENTRIES, sizeof(uint64_t));
        // Xử lý lỗi cấp phát thất bại
        if (pud_tbl[pud] == 0)
            return NULL;
    }
    uint64_t *pmd_tbl = (uint64_t *)pud_tbl[pud];

    // Tầng 4: PMD -> PT
    if (pmd_tbl[pmd] == 0) {
        if (!is_alloc)
            return NULL;
        pmd_tbl[pmd] = (uint64_t)calloc(PAGING64_DIR_ENTRIES, sizeof(uint64_t));
        // Xử lý lỗi cấp phát thất bại
        if (pmd_tbl[pmd] == 0)
            return NULL;
    }
    uint64_t *pt_tbl = (uint64_t *)pmd_tbl[pmd];

    // Tầng 5: Trả về địa chỉ của Entry chứa thông tin trang
    return &pt_tbl[pt];
}
#endif
#if defined(MM64)

/*
 * init_pte - Initialize PTE entry
 */
int init_pte(addr_t *pte,
             int pre,       // present
             addr_t fpn,    // FPN
             int drt,       // dirty
             int swp,       // swap
             int swptyp,    // swap type
             addr_t swpoff) // swap offset
{
    if (pre != 0) {
        if (swp == 0) { // Non swap ~ page online

            /* Valid setting with FPN */
            SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
            CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

            SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

            if (drt != 0) {
                SETBIT(*pte, PAGING_PTE_DIRTY_MASK);
            } else {
                CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            }
        } else { // page swapped
            CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
            SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
            CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

            SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
            SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

            if (drt != 0) {
                SETBIT(*pte, PAGING_PTE_DIRTY_MASK);
            } else {
                CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);
            }
        }
    }

    return 0;
}

/*
 * get_pd_from_pagenum - Parse address to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table
 */
int get_pd_from_address(addr_t addr, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt) {
    /* Extract page direactories */

    addr_t top_bits = (addr >> 57);
    if (top_bits != 0 && top_bits != 0x7f) {
        return -1;
    }
    *pgd = (addr & PAGING64_ADDR_PGD_MASK) >> PAGING64_ADDR_PGD_LOBIT;
    *p4d = (addr & PAGING64_ADDR_P4D_MASK) >> PAGING64_ADDR_P4D_LOBIT;
    *pud = (addr & PAGING64_ADDR_PUD_MASK) >> PAGING64_ADDR_PUD_LOBIT;
    *pmd = (addr & PAGING64_ADDR_PMD_MASK) >> PAGING64_ADDR_PMD_LOBIT;
    *pt = (addr & PAGING64_ADDR_PT_MASK) >> PAGING64_ADDR_PT_LOBIT;

    /* TO-DO: implement the page direactories mapping */

    return 0;
}

/*
 * get_pd_from_pagenum - Parse page number to 5 page directory level
 * @pgn   : pagenumer
 * @pgd   : page global directory
 * @p4d   : page level directory
 * @pud   : page upper directory
 * @pmd   : page middle directory
 * @pt    : page table
 */
int get_pd_from_pagenum(addr_t pgn, addr_t *pgd, addr_t *p4d, addr_t *pud, addr_t *pmd, addr_t *pt) {
    /* Shift the address to get page num and perform the mapping*/
    return get_pd_from_address(pgn << PAGING64_ADDR_PT_SHIFT, pgd, p4d, pud, pmd, pt);
}

/*
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
/*Hàm này được gọi trong quá trình Swap-out (khi RAM đầy, OS đẩy một trang xuống đĩa để lấy chỗ trống).*/
int pte_set_swap(struct pcb_t *caller, addr_t pgn, int swptyp, addr_t swpoff) {
    if (caller == NULL || caller->krnl == NULL) {
        printf("pte_set_swap: Invalid caller/mm pointer!\n");
        return -1;
    }
    // struct krnl_t *krnl = caller->krnl;
    addr_t *pte;
#ifdef MM64
    /* Get value from the system */
    /* TO-DO Perform multi-level page mapping */
    pte = (addr_t *)get_pte_ptr(caller, pgn, 1);
    if (pte == NULL)
        return -1;
#else
    pte = &krnl->mm->pgd[pgn];
#endif
    // Present = 0, Swapped = 1
    CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
    SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

    // Ghi thông tin thiết bị Swap
    SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
    SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

    return 0;
}

/*
 * pte_set_fpn - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
/*Hàm này được gọi khi hệ thống đã tìm thấy một khung trang vật lý (Frame) trống trong RAM và muốn gắn nó vào địa chỉ ảo
  của tiến trình.*/
int pte_set_fpn(struct pcb_t *caller, addr_t pgn, addr_t fpn) {
    if (caller == NULL || caller->krnl == NULL) {
        return -1;
    }
    // struct krnl_t *krnl = caller->krnl;
    addr_t *pte;

#ifdef MM64
    /* Get value from the system */
    /* TO-DO Perform multi-level page mapping */
    // Số 1 nghĩa là: Cho phép cấp phát bảng mới nếu chưa có
    pte = (addr_t *)get_pte_ptr(caller, pgn, 1);
    if (pte == NULL)
        return -1;
#else
    pte = &krnl->mm->pgd[pgn];
#endif
    // Present = 1, Swapped = 0, set FPN
    SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
    CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
    SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);

    return 0;
}

/* Get PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
uint32_t pte_get_entry(struct pcb_t *caller, addr_t pgn) {
    if (caller == NULL || caller->krnl == NULL) {
        return 0; // Trả về 0 (trống/lỗi) thay vì -1 để tránh lỗi Page Fault ảo
    }

    uint32_t pte = 0;

    /* TO-DO Perform multi-level page mapping */
#ifdef MM64
    // Số 0 nghĩa là: Chỉ đọc, không cấp phát bảng ảo. Nếu nhánh cây trống thì trả về NULL
    uint64_t *pte_ptr = get_pte_ptr(caller, pgn, 0);

    if (pte_ptr != NULL) {
        pte = (uint32_t)(*pte_ptr); // Ép kiểu an toàn từ 64-bit xuống chữ ký 32-bit
    }
#else
    // Nhánh 32-bit (Paging 1 cấp/phẳng):
    // Bắt buộc phải móc mm từ Danh bạ Kernel ra, KHÔNG DÙNG krnl->mm
    struct mm_struct *mm = caller->krnl->mm;
    if (mm != NULL) {
        pte = mm->pgd[pgn];
    }
#endif

    return pte;
}

/* Set PTE page table entry
 * @caller : caller
 * @pgn    : page number
 * @ret    : page table entry
 **/
int pte_set_entry(struct pcb_t *caller, addr_t pgn, uint32_t pte_val) {
    if (caller == NULL || caller->krnl == NULL) {
        printf("pte_set_entry: Invalid caller/mm pointer!\n");
        return -1;
    }
    // struct krnl_t *krnl = caller->krnl;
#ifdef MM64
    uint64_t *pte_ptr = get_pte_ptr(caller, pgn, 1);
    if (pte_ptr != NULL) {
        *pte_ptr = pte_val;
        return 0;
    }
#else
    krnl->mm->pgd[pgn] = pte_val;
#endif
    return -1;
}

/*
 * vmap_pgd_memset - map a range of page at aligned address
 */
/*thực hiện "cấp phát giả lập" (dummy allocation) bằng cách ghi một giá trị mẫu (pattern) vào bảng phân trang*/
int vmap_pgd_memset(struct pcb_t *caller, // process call
                    addr_t addr,          // start address which is aligned to pagesz
                    int pgnum)            // num of mapping page
{
    if (caller == NULL || caller->krnl == NULL) {
        printf("vmap_pgd_memset: Invalid caller/mm pointer!\n");
        return -1;
    }
    int pgit = 0;
    uint64_t pattern = 0xdeadbeef;
    // struct krnl_t *krnl = caller->krnl;

    // TO-DO memset the page table with given pattern
    for (pgit = 0; pgit < pgnum; pgit++) {
#ifdef MM64
        // Tính pgn
        addr_t pgn = addr >> PAGING64_ADDR_PT_SHIFT;

        // Xin cấp phát và lấy con trỏ ở tầng cuối (PT)
        uint64_t *pte_ptr = get_pte_ptr(caller, pgn, 1);

        if (pte_ptr != NULL) {
            *pte_ptr = pattern; // Ghi pattern vào đúng trang ảo tương ứng
        } else {
            return -1;
        }
#else
        // Giữ nguyên logic cũ nếu chạy 32-bit
        addr_t pgn = PAGING_PGN(addr);
        krnl->mm->pgd[pgn] = pattern;
#endif

        // Tịnh tiến địa chỉ ảo lên đúng 1 trang (4KB trong MM64)
        addr += PAGING64_PAGESZ;
    }

    return 0;
}

/*
 * vmap_page_range - map a range of page at aligned address
 */
addr_t vmap_page_range(struct pcb_t *caller,           // process call
                       addr_t addr,                    // start address which is aligned to pagesz
                       int pgnum,                      // num of mapping page
                       struct framephy_struct *frames, // list of the mapped frames
                       struct vm_rg_struct *ret_rg)    // return mapped region, the real mapped fp
{                                                      // no guarantee all given pages are mapped
                                                       // struct framephy_struct *fpit;
    if (caller == NULL || caller->krnl == NULL) {
        return -1;
    }
    //  Khởi tạo biến chạy để duyệt danh sách frame
    struct framephy_struct *fpit = frames;
    int pgit = 0;
    addr_t pgn;

    // TO-DO: update the rg_end and rg_start of ret_rg
    // Cập nhật vùng nhớ trả về (ret_rg)
    if (ret_rg != NULL) {
        ret_rg->rg_start = addr; // Điểm bắt đầu dải địa chỉ ảo
        // ret_rg->rg_end = addr + pgnum * PAGING64_PAGESZ;
    }
    // ret_rg->vmaid = ...
    // Vòng lặp "Ánh xạ": Chạy qua từng trang một
    int map_err = 0;
    for (pgit = 0; pgit < pgnum; pgit++) {
        // A. Tính Page Number (PGN) từ địa chỉ ảo hiện tại
#ifdef MM64
        pgn = (addr + pgit * PAGING64_PAGESZ) >> PAGING64_ADDR_PT_SHIFT;
#else
        pgn = PAGING_PGN(addr + pgit * PAGING_PAGESZ);
#endif
        // Kiểm tra an toàn: Nếu rổ frames hết trước pgnum thì dừng lại
        if (fpit == NULL)
            break;
        // B. Ghi vào Page Table
        if (pte_set_fpn(caller, pgn, fpit->fpn) != 0) {
            printf("vmap_page_range: pte_set_fpn failed at pgn %lu\n", (unsigned long)pgn);
            map_err = -1;
            break;
        }
        // C. Đưa vào hàng đợi FIFO (Để sau này OS biết trang nào vào trước để ưu tiên đuổi ra khi RAM đầy)
        struct mm_struct *mm = caller->krnl->mm;
        if (mm != NULL) {
            enlist_pgn_node(&mm->fifo_pgn, pgn);
        }

        // D. Di chuyển biến chạy sang frame tiếp theo trong danh sách
        fpit = fpit->fp_next;
    }

    if (ret_rg != NULL) {
#ifdef MM64
        ret_rg->rg_end = addr + pgit * PAGING64_PAGESZ; // Điểm kết thúc
#else
        ret_rg->rg_end = addr + pgit * PAGING_PAGESZ;
#endif
    }
    return map_err;
}

/*
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

addr_t alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct **frm_lst) {
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mram == NULL) {
        return -1;
    }
    addr_t fpn;
    int pgit;
    int alloc_success = 1;
    struct framephy_struct *newfp_str = NULL;
    struct framephy_struct *tail = *frm_lst;
    if (tail != NULL) {
        while (tail->fp_next != NULL)
            tail = tail->fp_next;
    }

    for (pgit = 0; pgit < req_pgnum; pgit++) {
        // TO-DO: allocate the page
        if (MEMPHY_get_freefp(caller->krnl->mram, &fpn) == 0) {
            // Cấp phát bộ nhớ cho 1 node mới của danh sách liên kết
            newfp_str = malloc(sizeof(struct framephy_struct));
            if (newfp_str == NULL) {
                MEMPHY_put_freefp(caller->krnl->mram, fpn); // Trả lại frame vừa lấy
                alloc_success = 0;
                break;
            }
            newfp_str->fpn = fpn; // Lưu số hiệu frame vừa xin được
            newfp_str->fp_next = NULL;
            // Ráp node mới vào danh sách liên kết frm_lst
            if (*frm_lst == NULL) {
                // Nếu danh sách đang trống, node mới chính là phần tử đầu tiên
                *frm_lst = newfp_str;
                tail = newfp_str;
            } else {
                // Nếu đã có phần tử, nối node mới vào đuôi và cập nhật lại con trỏ đuôi
                tail->fp_next = newfp_str;
                tail = newfp_str;
            }
        } else { // TO-DO: ERROR CODE of obtaining somes but not enough frames
            alloc_success = 0;
            break;
        }
    }
    if (alloc_success == 0) {
        // CƠ CHẾ ROLLBACK: Dọn dẹp sạch sẽ những gì đã cấp phát trước đó
        struct framephy_struct *curr = *frm_lst;
        while (curr != NULL) {
            struct framephy_struct *temp = curr;
            curr = curr->fp_next;

            // 1. Trả lại Frame vật lý cho RAM
            MEMPHY_put_freefp(caller->krnl->mram, temp->fpn);
            // 2. Giải phóng node bộ nhớ
            free(temp);
        }
        *frm_lst = NULL; // Đảm bảo con trỏ quay về NULL
        return -3000;    // Trả về mã lỗi
    }
    return 0;
}

/*
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
addr_t vm_map_ram(struct pcb_t *caller, addr_t astart, addr_t aend, addr_t mapstart, int incpgnum,
                  struct vm_rg_struct *ret_rg) {
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mram == NULL) {
        printf("vm_map_ram: Invalid caller/mram pointer!\n");
        return -1;
    }
    struct framephy_struct *frm_lst = NULL;
    int ret_alloc = 0;
    int pgnum = incpgnum;

    /*@bksysnet: author provides a feasible solution of getting frames
     *FATAL logic in here, wrong behaviour if we have not enough page
     *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
     *Don't try to perform that case in this simple work, it will result
     *in endless procedure of swap-off to get frame and we have not provide
     *duplicate control mechanism, keep it simple
     */
    ret_alloc = alloc_pages_range(caller, pgnum, &frm_lst);

    if (ret_alloc < 0 && ret_alloc != -3000)
        return -1;

    /* Out of memory */
    if (ret_alloc == -3000) {
        return -1;
    }

    /* it leaves the case of memory is enough but half in ram, half in swap
     * do the swaping all to swapper to get the all in ram */
    if (vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg) < 0) {
        ret_alloc = -1; // Báo hiệu lỗi để return -1 ở cuối hàm
    }

    struct framephy_struct *curr = frm_lst;
    while (curr != NULL) {
        struct framephy_struct *temp = curr; // Giữ lại node hiện tại
        curr = curr->fp_next;                // Tiến tới node tiếp theo
        //  Trả lại physical frame cho RAM nếu toàn bộ quá trình map thất bại
        if (ret_alloc < 0) {
            MEMPHY_put_freefp(caller->krnl->mram, temp->fpn);
        }
        free(temp); // Giải phóng node hiện tại
    }

    return (ret_alloc < 0) ? -1 : 0;
}

/* Swap copy content page from source frame to destination frame
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, addr_t srcfpn, struct memphy_struct *mpdst, addr_t dstfpn) {
    int cellidx;
    addr_t addrsrc, addrdst;
    for (cellidx = 0; cellidx < PAGING64_PAGESZ; cellidx++) {
        addrsrc = srcfpn * PAGING64_PAGESZ + cellidx;
        addrdst = dstfpn * PAGING64_PAGESZ + cellidx;

        BYTE data;
        MEMPHY_read(mpsrc, addrsrc, &data);
        MEMPHY_write(mpdst, addrdst, data);
    }

    return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller) {
    if (mm == NULL || caller == NULL || caller->krnl == NULL) {
        printf("init_mm: Invalid caller/krnl pointer!\n");
        return -1;
    }

    // Bắt lỗi cấp phát cho VMA
    struct vm_area_struct *vma0 = malloc(sizeof(struct vm_area_struct));
    if (vma0 == NULL)
        return -1;

    //  Bắt lỗi cấp phát cho bảng Root PGD
    mm->pgd = calloc(PAGING64_DIR_ENTRIES, sizeof(uint64_t));
    if (mm->pgd == NULL) {
        free(vma0); // Dọn dẹp rác nếu lỗi
        return -1;
    }

    // Cây phân trang sẽ tự mọc ra khi có dữ liệu.
    mm->p4d = NULL;
    mm->pud = NULL;
    mm->pmd = NULL;
    mm->pt = NULL;

    // Xóa sạch danh sách trước khi sử dụng
    vma0->vm_freerg_list = NULL;
    for (int i = 0; i < PAGING_MAX_SYMTBL_SZ; i++) {
        mm->symrgtbl[i].rg_start = 0;
        mm->symrgtbl[i].rg_end = 0;
        mm->symrgtbl[i].rg_next = NULL;
    }

    /* By default the owner comes with at least one vma */
    vma0->vm_id = 0;
    vma0->vm_start = 0;
    vma0->vm_end = vma0->vm_start;
    vma0->sbrk = vma0->vm_start;

    // 4. Bắt lỗi cấp phát cho Region đầu tiên
    struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end);
    if (first_rg == NULL) {
        free(mm->pgd); // Trả lại RAM
        free(vma0);    // Trả lại RAM
        return -1;
    }

    enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

    /* TO-DO update VMA0 next */
    vma0->vm_next = NULL;

    /* Point vma owner backward */
    vma0->vm_mm = mm;

    /* TO-DO: update mmap */
    mm->mmap = vma0;
    mm->fifo_pgn = NULL;
    mm->kcpooltbl = NULL;

    return 0;
}

struct vm_rg_struct *init_vm_rg(addr_t rg_start, addr_t rg_end) {
    struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));
    if (rgnode == NULL) {
        return NULL;
    }
    rgnode->rg_start = rg_start;
    rgnode->rg_end = rg_end;
    rgnode->rg_next = NULL;

    return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct *rgnode) {
    rgnode->rg_next = *rglist;
    *rglist = rgnode;

    return 0;
}

int enlist_pgn_node(struct pgn_t **plist, addr_t pgn) {
    struct pgn_t *pnode = malloc(sizeof(struct pgn_t));

    pnode->pgn = pgn;
    pnode->pg_next = *plist;
    *plist = pnode;

    return 0;
}

int print_list_fp(struct framephy_struct *ifp) {
    struct framephy_struct *fp = ifp;

    printf("print_list_fp: ");
    if (fp == NULL) {
        printf("NULL list\n");
        return -1;
    }
    printf("\n");
    while (fp != NULL) {
        printf("fp[" FORMAT_ADDR "]\n", fp->fpn);
        fp = fp->fp_next;
    }
    printf("\n");
    return 0;
}

int print_list_rg(struct vm_rg_struct *irg) {
    struct vm_rg_struct *rg = irg;

    printf("print_list_rg: ");
    if (rg == NULL) {
        printf("NULL list\n");
        return -1;
    }
    printf("\n");
    while (rg != NULL) {
        printf("rg[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", rg->rg_start, rg->rg_end);
        rg = rg->rg_next;
    }
    printf("\n");
    return 0;
}

int print_list_vma(struct vm_area_struct *ivma) {
    struct vm_area_struct *vma = ivma;

    printf("print_list_vma: ");
    if (vma == NULL) {
        printf("NULL list\n");
        return -1;
    }
    printf("\n");
    while (vma != NULL) {
        printf("va[" FORMAT_ADDR "->" FORMAT_ADDR "]\n", vma->vm_start, vma->vm_end);
        vma = vma->vm_next;
    }
    printf("\n");
    return 0;
}

int print_list_pgn(struct pgn_t *ip) {
    printf("print_list_pgn: ");
    if (ip == NULL) {
        printf("NULL list\n");
        return -1;
    }
    printf("\n");
    while (ip != NULL) {
        printf("va[" FORMAT_ADDR "]-\n", ip->pgn);
        ip = ip->pg_next;
    }
    printf("n");
    return 0;
}

int print_pgtbl(struct pcb_t *caller, addr_t start, addr_t end) {
    if (caller == NULL || caller->krnl == NULL)
        return -1;

    // Lấy mm qua hàm tra cứu
    struct mm_struct *mm = caller->krnl->mm;
    if (mm == NULL || mm->pgd == NULL)
        return -1;

    if (mm->pgd == NULL) {
        printf("print_pgtbl: Root page table is not initialized!\n");
        return -1;
    }

    printf("==== PAGE TABLE DUMP ====\n");

#ifdef MM64
    if (end == (addr_t)-1) {
        printf("Mode: Full Tree Traversal\n");
        // Duyệt trực tiếp cây 5 cấp, bỏ qua hoàn toàn các nhánh NULL
        for (addr_t pgd = 0; pgd < PAGING64_DIR_ENTRIES; pgd++) {
            if (mm->pgd[pgd] == 0)
                continue;
            uint64_t *p4d_tbl = (uint64_t *)mm->pgd[pgd];

            for (addr_t p4d = 0; p4d < PAGING64_DIR_ENTRIES; p4d++) {
                if (p4d_tbl[p4d] == 0)
                    continue;
                uint64_t *pud_tbl = (uint64_t *)p4d_tbl[p4d];

                for (addr_t pud = 0; pud < PAGING64_DIR_ENTRIES; pud++) {
                    if (pud_tbl[pud] == 0)
                        continue;
                    uint64_t *pmd_tbl = (uint64_t *)pud_tbl[pud];

                    for (addr_t pmd = 0; pmd < PAGING64_DIR_ENTRIES; pmd++) {
                        if (pmd_tbl[pmd] == 0)
                            continue;
                        uint64_t *pt_tbl = (uint64_t *)pmd_tbl[pmd];

                        for (addr_t pt = 0; pt < PAGING64_DIR_ENTRIES; pt++) {
                            uint64_t pte = pt_tbl[pt];
                            if (pte != 0) {
                                // Tái tạo PGN từ 5 chỉ số (Mỗi index 9 bit)
                                addr_t pgn = (pgd << 36) | (p4d << 27) | (pud << 18) | (pmd << 9) | pt;

                                // Giải mã PTE
                                int pre = (pte & PAGING_PTE_PRESENT_MASK) ? 1 : 0;
                                int swp = (pte & PAGING_PTE_SWAPPED_MASK) ? 1 : 0;
                                int drt = (pte & PAGING_PTE_DIRTY_MASK) ? 1 : 0;

                                if (pre) {
                                    addr_t fpn = GETVAL(pte, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT);
                                    printf("va[%016lx] -> pte[0x%016lx] (P:%d S:%d D:%d FPN:%lu)\n", (unsigned long)pgn,
                                           (unsigned long)pte, pre, swp, drt, (unsigned long)fpn);
                                } else if (swp) {
                                    addr_t swpoff = GETVAL(pte, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
                                    printf("va[%016lx] -> pte[0x%016lx] (P:%d S:%d D:%d SWPOFF:%lu)\n",
                                           (unsigned long)pgn, (unsigned long)pte, pre, swp, drt,
                                           (unsigned long)swpoff);
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        printf("Mode: Linear Range Scan (%08lx - %08lx)\n", (unsigned long)start, (unsigned long)end);
        // Quét tuyến tính khi có end xác định
        for (addr_t addr = start; addr < end; addr += PAGING64_PAGESZ) {
            addr_t pgd = 0, p4d = 0, pud = 0, pmd = 0, pt = 0;
            get_pd_from_address(addr, &pgd, &p4d, &pud, &pmd, &pt);

            if (mm->pgd[pgd] == 0)
                continue;
            uint64_t *p4d_tbl = (uint64_t *)mm->pgd[pgd];

            if (p4d_tbl[p4d] == 0)
                continue;
            uint64_t *pud_tbl = (uint64_t *)p4d_tbl[p4d];

            if (pud_tbl[pud] == 0)
                continue;
            uint64_t *pmd_tbl = (uint64_t *)pud_tbl[pud];

            if (pmd_tbl[pmd] == 0)
                continue;
            uint64_t *pt_tbl = (uint64_t *)pmd_tbl[pmd];

            uint64_t pte = pt_tbl[pt];
            if (pte != 0) {
                addr_t pgn = addr >> PAGING64_ADDR_PT_SHIFT;
                int pre = (pte & PAGING_PTE_PRESENT_MASK) ? 1 : 0;
                int swp = (pte & PAGING_PTE_SWAPPED_MASK) ? 1 : 0;
                int drt = (pte & PAGING_PTE_DIRTY_MASK) ? 1 : 0;

                printf("va[%016lx] -> pte[0x%016lx] (P:%d S:%d D:%d)\n", (unsigned long)pgn, (unsigned long)pte, pre,
                       swp, drt);
            }
        }
    }
#else
    // Dành cho hệ thống 32-bit (Thường là cấu trúc phẳng hoặc 2 cấp)
    for (addr = start; addr < end; addr += PAGING_PAGESZ) {
        addr_t pgn = PAGING_PGN(addr);

        // Giả sử Paging 32-bit là mảng phẳng 1 cấp
        if (mm->pgd[pgn] != 0) {
            printf("va[" FORMAT_ADDR "] -> pte[" FORMATX_ADDR "]\n", (unsigned int)pgn, (unsigned int)mm->pgd[pgn]);
        }
    }
#endif

    printf("=========================================\n");
    return 0;
}

#endif // def MM64

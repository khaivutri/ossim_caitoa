/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* Caitoa release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

// #ifdef MM_PAGING
/*
 * System Library
 * Memory Module Library libmem.c
 */

#include "libmem.h"
#include "mm.h"
#include "mm64.h"
#include "string.h"
#include "syscall.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static pthread_mutex_t mmvm_lock = PTHREAD_MUTEX_INITIALIZER;

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt) {
    struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

    if (rg_elmt->rg_start >= rg_elmt->rg_end)
        return -1;

    if (rg_node != NULL)
        rg_elmt->rg_next = rg_node;

    /* Enlist the new region */
    mm->mmap->vm_freerg_list = rg_elmt;

    return 0;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid) {
    if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
        return NULL;

    return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr) {
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ || caller == NULL || caller->krnl == NULL) {
        return -1;
    }
    /*Allocate at the toproof */
    pthread_mutex_lock(&mmvm_lock);
    struct vm_rg_struct rgnode;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);
    // int inc_sz = 0;

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0) {
        caller->krnl->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
        caller->krnl->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

        *alloc_addr = rgnode.rg_start;

        pthread_mutex_unlock(&mmvm_lock);
        return 0;
    }

    /* TO-DO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
    int old_sbrk = cur_vma->sbrk;
    addr_t aligned_size;
    /*Attempt to increate limit to get space */
#ifdef MM64
    aligned_size = PAGING64_PAGE_ALIGNSZ(size);
#else
    aligned_size = PAGING_PAGE_ALIGNSZ(size);
#endif
    /* TO-DO INCREASE THE LIMIT
     * SYSCALL 17 sys_memmap
     */
    struct sc_regs regs;
    regs.a1 = SYSMEM_INC_OP; // Xin OS cấp phát đúng kích thước đã làm tròn
    regs.a2 = vmaid;
    regs.a3 = aligned_size;                         // Xin OS cấp phát đúng kích thước đã làm tròn
    _syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */

    /* Cập nhật thông tin vùng nhớ cho biến đang xin cấp phát */
    caller->krnl->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->krnl->mm->symrgtbl[rgid].rg_end = old_sbrk + size;
    *alloc_addr = old_sbrk;

    /* XỬ LÝ PHẦN DƯ: Đưa phần không gian thừa vào danh sách free list */
    if (aligned_size > size) {
        struct vm_rg_struct *remain_rg = malloc(sizeof(struct vm_rg_struct));
        remain_rg->rg_start = old_sbrk + size;       // Bắt đầu từ chỗ biến vừa dùng xong
        remain_rg->rg_end = old_sbrk + aligned_size; // Kéo dài đến hết trang vừa xin
        remain_rg->rg_next = NULL;

        enlist_vm_freerg_list(caller->krnl->mm, remain_rg); // Quăng vào rổ free
    }

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid) {
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ || caller == NULL || caller->krnl == NULL) {
        return -1;
    }
    pthread_mutex_lock(&mmvm_lock);
    /* TO-DO: Manage the collect freed region to freerg_list */
    struct vm_rg_struct *rgnode = get_symrg_byid(caller->krnl->mm, rgid);
    if (rgnode == NULL) {
        pthread_mutex_unlock(&mmvm_lock);
        return -1;
    }
    if (rgnode->rg_start == 0 && rgnode->rg_end == 0) {
        pthread_mutex_unlock(&mmvm_lock);
        return -1;
    }
    struct vm_rg_struct *freerg_node = malloc(sizeof(struct vm_rg_struct));
    if (freerg_node == NULL) {
        pthread_mutex_unlock(&mmvm_lock);
        return -1; // Lỗi hết RAM vật lý của máy tính thật
    }
    freerg_node->rg_start = rgnode->rg_start;
    freerg_node->rg_end = rgnode->rg_end;
    freerg_node->rg_next = NULL;

    rgnode->rg_start = rgnode->rg_end = 0;
    rgnode->rg_next = NULL;

    /*enlist the obsoleted memory region */
    enlist_vm_freerg_list(caller->krnl->mm, freerg_node);

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
}

/*liballoc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int liballoc(struct pcb_t *proc, addr_t size, uint32_t reg_index) {
    addr_t addr;
    int val = __alloc(proc, 0, reg_index, size, &addr);
    if (val == -1) {
        return -1;
    }
    proc->regs[reg_index] = addr;
#ifdef IODUMP
    /* TO-DO dump IO content (if needed) */
    printf("ALLOC: Process PID[%d] allocated %lu bytes -> Virtual Address: 0x%lx (Region ID: %u)\n", proc->pid,
           (unsigned long)size, (unsigned long)addr, reg_index);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

    /* By default using vmaid = 0 */
    return val;
}

/*libfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int libfree(struct pcb_t *proc, uint32_t reg_index) {
    int val = __free(proc, 0, reg_index);
    if (val == -1) {
        return -1;
    }
    printf("%s:%d\n", __func__, __LINE__);
    addr_t freed_addr = proc->regs[reg_index]; // Lưu tạm để in log
    proc->regs[reg_index] = 0;                 // Xóa địa chỉ trong thanh ghi
#ifdef IODUMP
    /* TO-DO dump IO content (if needed) */
    printf("FREE: Process PID[%d] freed region at Virtual Address: 0x%lx (Region ID: %u)\n", proc->pid,
           (unsigned long)freed_addr, reg_index);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif
    return 0; // val;
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
// Truy cập dữ liệu một page
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller) {

    uint32_t pte = pte_get_entry(caller, pgn);

    if (!PAGING_PAGE_PRESENT(pte)) { /* Page is not online, make it actively living */
        addr_t vicpgn, swpfpn;
        addr_t vicfpn;
        uint32_t vicpte;
        struct sc_regs regs;

        /* TO-DO Initialize the target frame storing our variable */
        addr_t tgtfpn;

        /* TO-DO: Play with your paging theory here */
        /* Get free frame in MEMSWP */
        // Kiểm tra ram đã đầy hay chưa
        if (MEMPHY_get_freefp(caller->krnl->mram, &tgtfpn) == -1) {
            /* Find victim page */
            if (find_victim_page(caller->krnl->mm, &vicpgn) == -1) {
                return -1;
            }
            /* Lấy số hiệu khung RAM (FPN) mà nạn nhân đang chiếm giữ */
            vicpte = pte_get_entry(caller, vicpgn);
            vicfpn = PAGING_FPN(vicpte);
            if (MEMPHY_get_freefp(caller->krnl->active_mswp, &swpfpn) == -1) {
                return -1; // Đĩa Swap cũng đầy
            }
            /* TO-DO copy victim frame to swap
             * SWP(vicfpn <--> swpfpn)
             * SYSCALL 17 sys_memmap
             */
            regs.a1 = SYSMEM_SWP_OP; // Đẩy page từ RAM xuống đĩa
            regs.a2 = vicfpn;        // Từ: Khung RAM của nạn nhân
            regs.a3 = swpfpn;        // Đến: Khung đĩa Swap
            _syscall(caller->krnl, caller->pid, 17, &regs);

            /* Cập nhật Page Table: Đánh dấu nạn nhân đã bị đẩy xuống đĩa */
            pte_set_swap(caller, vicpgn, 0, swpfpn);

            /* Tịch thu khung RAM của nạn nhân để làm target frame cho mình */
            tgtfpn = vicfpn;
        }
        /* TO-DO: Implement swap frame from MEMRAM to MEMSWP and vice versa*/
        if (pte & PAGING_PTE_SWAPPED_MASK) {
            addr_t saved_swpfpn = PAGING_SWP(pte); // Lấy vị trí đĩa cũ

            // Chép dữ liệu từ Đĩa ngược lên khung RAM vừa có được
            __swap_cp_page(caller->krnl->active_mswp, saved_swpfpn, caller->krnl->mram, tgtfpn);

            // Trả lại khung vừa dùng cho đĩa Swap
            MEMPHY_put_freefp(caller->krnl->active_mswp, saved_swpfpn);
        }
        /* Update its online status of the target page */
        pte_set_fpn(caller, pgn, tgtfpn);
        enlist_pgn_node(&caller->krnl->mm->fifo_pgn, pgn);
    }

    *fpn = PAGING_FPN(pte_get_entry(caller, pgn));

    return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller) {
#ifdef MM64
    // Dịch phải 12 bit để tính Page Number cho kích thước trang 4KB
    int pgn = addr >> PAGING64_ADDR_PT_SHIFT;
    int off = addr & ((1 << PAGING64_ADDR_PT_SHIFT) - 1); // Nếu sau này cần tính offset
#else
    int pgn = PAGING_PGN(addr);
// int off = PAGING_OFFST(addr);
#endif
    int fpn;
    int phyaddr;
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */

#ifdef MM64
    phyaddr = (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif

    /* TO-DO
     *  MEMPHY_read(caller->krnl->mram, phyaddr, data);
     *  MEMPHY READ
     *  SYSCALL 17 sys_memmap with SYSMEM_IO_READ
     */
    struct sc_regs regs;
    regs.a1 = SYSMEM_IO_READ;      // Lệnh yêu cầu Kernel đọc dữ liệu
    regs.a2 = phyaddr;             // Đọc tại địa chỉ vật lý này
    regs.a3 = (unsigned long)data; // Lưu giá trị đọc được vào con trỏ data

    _syscall(caller->krnl, caller->pid, 17, &regs);

    return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller) {
    /*  Tách địa chỉ ảo thành Page Number (pgn) và Offset (off) */
#ifdef MM64
    // Dịch phải 12 bit để tính Page Number (4KB Page Size)
    int pgn = addr >> PAGING64_ADDR_PT_SHIFT;
    int off = addr & ((1 << PAGING64_ADDR_PT_SHIFT) - 1); // Lấy phần đuôi (12 bit) làm offset
#else
    pgn = PAGING_PGN(addr);
    off = PAGING_OFFST(addr);
#endif
    int fpn;
    /* Get the page to MEMRAM, swap from MEMSWAP if needed */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0)
        return -1; /* invalid page access */
    /* Tính toán địa chỉ vật lý (Physical Address) */
    int phyaddr;
#ifdef MM64
    phyaddr = (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif
    /* TO-DO
     *  MEMPHY_write(caller->krnl->mram, phyaddr, value);
     *  MEMPHY WRITE with SYSMEM_IO_WRITE
     * SYSCALL 17 sys_memmap
     */
    /* Ghi dữ liệu xuống RAM vật lý thông qua System Call 17 */
    struct sc_regs regs;
    regs.a1 = SYSMEM_IO_WRITE; // Mã lệnh: GHI
    regs.a2 = phyaddr;         // Đích đến: Địa chỉ vật lý đã tính
    regs.a3 = value;           // Dữ liệu: 1 Byte cần ghi (Không cần ép kiểu con trỏ)

    _syscall(caller->krnl, caller->pid, 17, &regs); /* SYSCALL 17 sys_memmap */

    return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data) {
    if (caller == NULL || caller->krnl == NULL || rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1;
    }

    struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
    /* TO-DO Invalid memory identify */

    if (currg == NULL) {
        return -1;
    }
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        return -1; // Chặn đọc vùng nhớ đã bị giải phóng
    }
    if (currg->rg_start + offset >= currg->rg_end) {
        return -1; // Lỗi: Truy cập vượt quá giới hạn mảng (Out-of-bounds / Segmentation Fault)
    }

    int ret = pg_getval(caller->krnl->mm, currg->rg_start + offset, data, caller);

    return ret;
}

/*libread - PAGING-based read a region memory */
int libread(struct pcb_t *proc, // Process executing the instruction
            uint32_t source,    // Index of source register
            addr_t offset,      // Source address = [source] + [offset]
            uint32_t *destination) {
    BYTE data;
    printf("%s:%d\n", __func__, __LINE__);
    int val = __read(proc, 0, source, offset, &data);

    *destination = data;
#ifdef IODUMP
    /* TO-DO dump IO content (if needed) */
    if (val == 0) {
        printf("read region=%d offset=%ld value=%d\n", source, offset, data);
    }
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

    return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value) {
    if (caller == NULL || caller->krnl == NULL || rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1;
    }
    pthread_mutex_lock(&mmvm_lock);
    struct vm_rg_struct *currg = get_symrg_byid(caller->krnl->mm, rgid);
    /*Invalid memory identify */
    if (currg == NULL) {
        pthread_mutex_unlock(&mmvm_lock);
        return -1;
    }
    /* Đảm bảo vùng nhớ này đang tồn tại (chưa bị FREE) */
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        pthread_mutex_unlock(&mmvm_lock);
        return -1; // Lỗi: Ghi vào vùng nhớ đã bị giải phóng (Use-after-free)
    }
    /* Đảm bảo độ lệch (offset) không đâm thủng ranh giới của vùng nhớ */
    if (currg->rg_start + offset >= currg->rg_end) {
        pthread_mutex_unlock(&mmvm_lock);
        return -1; // Lỗi: Tràn bộ đệm (Buffer Overflow / Out-of-bounds write)
    }

    int ret = pg_setval(caller->krnl->mm, currg->rg_start + offset, value, caller);

    /* Mở khóa trước khi thoát */
    pthread_mutex_unlock(&mmvm_lock);

    // Trả về mã lỗi của pg_setval thay vì luôn return 0
    return ret;
}

/*libwrite - PAGING-based write a region memory */
int libwrite(struct pcb_t *proc,   // Process executing the instruction
             BYTE data,            // Data to be wrttien into memory
             uint32_t destination, // Index of destination register
             addr_t offset) {
    int val = __write(proc, 0, destination, offset, data);
    if (val == -1) {
        return -1;
    }
#ifdef IODUMP
    /* TO-DO dump IO content (if needed) */
    printf("write region=%d offset=%ld value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
    print_pgtbl(proc, 0, -1); // print max TBL
#endif
#endif

    return val;
}

/*libkmem_malloc- alloc region memory in kmem
 *@caller: caller
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 */

int libkmem_malloc(struct pcb_t *caller, uint32_t size, uint32_t reg_index) {
    /* TO-DO: provide OS level management
     *       and forward the request to helper
     */
    if (caller == NULL) {
        return -1; // Lỗi: Không xác định được tiến trình gọi
    }
    if (size == 0) {
        return -1; // Lỗi: Yêu cầu cấp phát 0 byte là vô nghĩa
    }
    addr_t addr;
    int val = __kmalloc(caller, -1, reg_index, size, &addr);
    /* TO-DO: provide OS kmem allocation validation
     */
    if (val != 0) {
        return -1; // Lỗi: Cấp phát thất bại (Có thể do hết bộ nhớ Kernel)
    }

    return 0;
}

/*kmalloc - alloc region memory in kmem
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: memory size
 *@alloc_addr: allocated address
 */
addr_t __kmalloc(struct pcb_t *caller, int vmaid, int rgid, addr_t size, addr_t *alloc_addr) {
    /* TO-DO: provide OS kernel memory allocation
     *       update krnl_pgd for OS kernel level management */

    struct krnl_t *krnl = caller->krnl;
    if (krnl == NULL || size == 0) {
        return -1; // Lỗi: Không hợp lệ
    }
#ifdef MM64
    int num_pages = (size + PAGING64_PAGESZ - 1) / PAGING64_PAGESZ;
    addr_t vaddr = (addr_t)rgid * PAGING64_PAGESZ * 1000;
#else
    int num_pages = (size + PAGING_PAGESZ - 1) / PAGING_PAGESZ;
    addr_t vaddr = (addr_t)rgid * PAGING_PAGESZ * 1000;
#endif

    /* 3. Cấp phát RAM và Ánh xạ vào hệ thống phân trang 5 cấp */
    for (int i = 0; i < num_pages; i++) {
        addr_t fpn;

        // Lấy Frame vật lý từ RAM của Kernel
        if (MEMPHY_get_freefp(krnl->mram, &fpn) == -1) {
            return -1;
        }

        addr_t current_vaddr = vaddr + (i * PAGING64_PAGESZ);
        addr_t pgn = current_vaddr >> PAGING64_ADDR_PT_SHIFT;
        /* Sử dụng hệ thống 5 cấp đã khai báo trong krnl_t */
        // Hàm get_pte_ptr sẽ dựa trên PAGING64_PAGESZ để bóc tách bit địa chỉ[cite: 2]
        uint64_t *pte = get_pte_ptr(caller, pgn, 1);

        if (pte != NULL) {
            if (pte_set_fpn(caller, pgn, fpn) != 0) {
                return -1; // Lỗi: Không thể ánh xạ trang ảo vào bảng trang
            }
            // Bật bit Present để MMU nhận diện trang hợp lệ
            PAGING_PTE_SET_PRESENT(*pte);
        } else {
            return -1;
        }
    }

    *alloc_addr = vaddr;
    return 0;
}

/*libkmem_cache_pool_create - create cache pool in kmem
 *@caller: caller
 *@size: memory size
 *@align: alignment size of each cache slot (identical cache slot size)
 *@cache_pool_id: cache pool ID
 */
int libkmem_cache_pool_create(struct pcb_t *caller, uint32_t size, uint32_t align, uint32_t cache_pool_id) {
    /* TO-DO: provide OS level management */
    /* 1. Validation cơ bản */
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL) {
        return -1;
    }

    if (size == 0 || align == 0 || size < align) {
        return -1; // Kích thước không hợp lệ
    }
    struct mm_struct *mm = caller->krnl->mm;
    /* 2. Cấp phát mảng Danh bạ Pool linh hoạt theo ID */
    /* Thay đổi logic cấp phát */
    if (mm->kcpooltbl == NULL) {
        // Cấp phát CỐ ĐỊNH 100 slot ngay từ lần gọi đầu tiên
        // Bất kể tiến trình gọi pool_id = 2 hay 15, mảng đều chứa được.
        mm->kcpooltbl = calloc(PAGING_MAX_KCACHE_POOLS, sizeof(struct kcache_pool_struct));

        if (mm->kcpooltbl == NULL) {
            return -1;
        }
    }
    if (cache_pool_id >= PAGING_MAX_KCACHE_POOLS) {
        return -1; // Lỗi: Vượt quá số lượng Pool tối đa của hệ thống
    }

    /* 3. Xin cấp phát RAM vật lý cho Pool */
    addr_t pool_storage_addr;
    int ret = __kmalloc(caller, -1, cache_pool_id, size, &pool_storage_addr);

    if (ret != 0) {
        return -1; // Lỗi: Quá trình ánh xạ phân trang thất bại
    }
    /* 4. Ghi chú cấu hình Pool vào kcpooltbl
     * Ép kiểu tường minh (int) để khớp với định nghĩa trong os-mm.h
     */
    mm->kcpooltbl[cache_pool_id].size = (int)size;
    mm->kcpooltbl[cache_pool_id].align = (int)align;
    mm->kcpooltbl[cache_pool_id].storage = pool_storage_addr;

    return 0;
}

/*libkmem_cache_alloc - allocate cache slot in cache pool, cache slot has identical size
 * the allocated size is embedded in pool management mechanism
 *@caller: caller
 *@cache_pool_id: cache pool ID
 *@reg_index: memory region index
 */
int libkmem_cache_alloc(struct pcb_t *proc, uint32_t cache_pool_id, uint32_t reg_index) {
    /* TO-DO: provide OS level management
     *       and forward the request to helper
     */
    /* 1. OS level management: Validation trước khi xử lý */
    if (proc == NULL || proc->krnl == NULL || proc->krnl->mm == NULL) {
        return -1; // Lỗi: Tiến trình hoặc Kernel context không hợp lệ
    }
    addr_t alloc_addr = 0; // Biến hứng địa chỉ ảo trả về
    /* 2. Chuyển tiếp yêu cầu xuống hàm helper (hàm lõi)*/
    addr_t val = __kmem_cache_alloc(proc, -1, reg_index, cache_pool_id, &alloc_addr);
    /* 3. Kiểm tra kết quả từ hàm helper */
    if (val == (addr_t)-1 || val == 0) {
        return -1; // Lỗi: Cấp phát Cache slot thất bại
    }
    return 0;
}

/*kmem_cache_alloc - alloc region memory in kmem cache
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@cache_pool_id: cached pool ID
 *@alloc_addr: allocated address
 */

addr_t __kmem_cache_alloc(struct pcb_t *caller, int vmaid, int rgid, int cache_pool_id, addr_t *alloc_addr) {
    /* TO-DO: provide OS level management */
    /* 1. Validation an toàn */
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL) {
        return -1;
    }

    // Kiểm tra giới hạn ID của Pool (Sử dụng Macro ta đã thống nhất)
    if (cache_pool_id < 0 || cache_pool_id >= PAGING_MAX_KCACHE_POOLS) {
        return -1;
    }
    struct mm_struct *mm = caller->krnl->mm;
    struct kcache_pool_struct *pool = &mm->kcpooltbl[cache_pool_id];
    /* 2. Kiểm tra tình trạng của Pool */
    // Nếu pool chưa được tạo (align = 0) hoặc dung lượng còn lại nhỏ hơn 1 slot
    if (pool->align == 0 || pool->size < pool->align) {
        return -1; // Lỗi: Pool không tồn tại hoặc đã hết chỗ
    }
    /* 3. Cấp phát ô nhớ (Slot) theo cơ chế Bump Allocator */
    // Lấy địa chỉ đầu tiên đang trống
    addr_t slot_addr = pool->storage;

    // Tịnh tiến (Bump) con trỏ kho lên ô tiếp theo
    pool->storage += pool->align;

    // Trừ đi dung lượng đã cấp phát khỏi tổng dung lượng còn lại
    pool->size -= pool->align;

    /* 4. Cập nhật Danh bạ vùng nhớ (Symbol Table) */
    if (rgid >= 0 && rgid < PAGING_MAX_SYMTBL_SZ) {
        mm->symrgtbl[rgid].vmaid = vmaid;
        mm->symrgtbl[rgid].rg_start = slot_addr;
        // Điểm kết thúc của vùng nhớ chính là điểm bắt đầu cộng với kích thước 1 slot
        mm->symrgtbl[rgid].rg_end = slot_addr + pool->align;
    } else {
        // Trả lại bộ nhớ nếu rgid không hợp lệ (Rollback)
        pool->storage -= pool->align;
        pool->size += pool->align;
        return -1;
    }
    /* 5. Trả về kết quả */
    *alloc_addr = slot_addr;

    return 0; // Thành công
}

int libkmem_copy_from_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset,
                           uint32_t size) {
    /* TO-DO: provide OS level management kmem
     */
    /* Validation cơ bản */
    if (caller == NULL || size == 0) {
        return -1;
    }
    /*
     * TO-DO: Map kernel address range
     */
    BYTE temp_data;

    /* Vòng lặp copy từng byte từ Nguồn (Kernel) sang Đích (User) */
    for (uint32_t i = 0; i < size; i++) {

        // 1. Lấy dữ liệu từ Kernel (Ví dụ: Từ Cache Pool)
        if (__read_kernel_mem(caller, -1, source, offset + i, &temp_data) != 0) {
            return -1; // Lỗi: Lỗi vùng nhớ Kernel
        }

        // 2. Trả dữ liệu về cho biến của User
        // Hàm này an toàn vì nó đã tự gọi pg_getpage() để xử lý Swap
        if (__write_user_mem(caller, -1, destination, offset + i, temp_data) != 0) {
            return -1; // Lỗi: User không có quyền ghi hoặc lỗi trang
        }
    }

    return 0; // Copy thành công
}

int libkmem_copy_to_user(struct pcb_t *caller, uint32_t source, uint32_t destination, uint32_t offset, uint32_t size) {
    /* TO-DO: provide OS level management kmem
     */
    /* Validation cơ bản */
    if (caller == NULL || size == 0) {
        return -1;
    }
    /*
     * TO-DO: Map kernel address range
     */
    BYTE temp_data;

    /* Vòng lặp copy từng byte từ Nguồn (Kernel) sang Đích (User) */
    for (uint32_t i = 0; i < size; i++) {

        // 1. Lấy dữ liệu từ Kernel (Ví dụ: Từ Cache Pool)
        if (__read_kernel_mem(caller, -1, source, offset + i, &temp_data) != 0) {
            return -1; // Lỗi: Lỗi vùng nhớ Kernel
        }

        // 2. Trả dữ liệu về cho biến của User
        // Hàm này an toàn vì nó đã tự gọi pg_getpage() để xử lý Swap
        if (__write_user_mem(caller, -1, destination, offset + i, temp_data) != 0) {
            return -1; // Lỗi: User không có quyền ghi hoặc lỗi trang
        }
    }

    return 0; // Copy thành công
}

/*__read_kernel_mem - read value in kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data) {
    /* TO-DO: provide OS memory operator for kernel memory region */
    /* 1. Validation cơ bản */
    if (caller == NULL || caller->krnl == NULL || data == NULL) {
        return -1;
    }
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1; // ID không hợp lệ
    }

    struct mm_struct *mm = caller->krnl->mm;
    struct vm_rg_struct *currg = &mm->symrgtbl[rgid];

    /* 2. Kiểm tra tính hợp lệ của vùng nhớ và offset */
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        return -1; // Lỗi: Vùng nhớ chưa được cấp phát hoặc đã bị Free
    }
    if (currg->rg_start + offset >= currg->rg_end) {
        return -1; // Lỗi: Đọc vượt rào (Out-of-bounds)
    }
    /* 3. Tính toán Địa chỉ Ảo (Virtual Address) */
    addr_t vaddr = currg->rg_start + offset;

    addr_t pgn;
    addr_t off;
    /* 4. Dịch Địa chỉ Ảo sang Số hiệu Trang (PGN) và Offset trong trang */
#ifdef MM64
    pgn = vaddr >> PAGING64_ADDR_PT_SHIFT;             // Bỏ 12 bit cuối để lấy PGN
    off = vaddr & ((1 << PAGING64_ADDR_PT_SHIFT) - 1); // Lấy 12 bit cuối làm Offset
#else
    pgn = PAGING_PGN(vaddr);
    off = PAGING_OFFST(vaddr);
#endif
    /* 5. Tra cứu Bảng phân trang để tìm Khung vật lý (Frame)
     * Dùng pte_get_entry để hỗ trợ cả 2 chiến lược pgd chung hoặc krnl_pgd riêng
     */
    uint32_t pte = pte_get_entry(caller, pgn);

    if (!PAGING_PAGE_PRESENT(pte)) {
        // Khác với User mem, Kernel mem bị miss page là lỗi nghiêm trọng
        printf("KERNEL PANIC: Page fault in kernel memory at vaddr %016lx\n", (unsigned long)vaddr);
        return -1;
    }

    addr_t fpn = PAGING_FPN(pte);
    addr_t phyaddr;
    /* 6. Ghép FPN và offset lại thành Địa chỉ Vật lý (Physical Address) */
#ifdef MM64
    phyaddr = (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif

    /* 7. Đọc trực tiếp từ Chip RAM (Không dùng Syscall) */
    if (MEMPHY_read(caller->krnl->mram, phyaddr, data) != 0) {
        return -1; // Lỗi phần cứng RAM
    }
    return 0;
}

/*__write_kernel_mem - write a kernel region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_kernel_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value) {
    /* TO-DO: provide OS memory operator for kernel memory region */
    /* 1. Validation cơ bản */
    if (caller == NULL || caller->krnl == NULL) {
        return -1;
    }
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1; // ID vùng nhớ (Region ID) không hợp lệ
    }

    struct mm_struct *mm = caller->krnl->mm;
    struct vm_rg_struct *currg = &mm->symrgtbl[rgid];

    /* 2. Kiểm tra tính hợp lệ của vùng nhớ và offset */
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        return -1; // Lỗi: Ghi vào vùng nhớ rỗng (chưa khởi tạo hoặc đã bị Free)
    }
    if (currg->rg_start + offset >= currg->rg_end) {
        return -1; // Lỗi: Ghi vượt ranh giới vùng nhớ (Out-of-bounds Write)
    }

    /* 3. Tính Địa chỉ Ảo (Virtual Address) */
    addr_t vaddr = currg->rg_start + offset;
    addr_t pgn, off;

    /* 4. Tách Số hiệu Trang (PGN) và Offset */
#ifdef MM64
    pgn = vaddr >> PAGING64_ADDR_PT_SHIFT;
    off = vaddr & ((1 << PAGING64_ADDR_PT_SHIFT) - 1);
#else
    pgn = PAGING_PGN(vaddr);
    off = PAGING_OFFST(vaddr);
#endif

    /* 5. Tra cứu Bảng phân trang qua pte_get_entry */
    uint32_t pte = pte_get_entry(caller, pgn);

    if (!PAGING_PAGE_PRESENT(pte)) {
        // Kernel panic nếu bộ nhớ ảo của Kernel không được ánh xạ
        printf("KERNEL PANIC: Write page fault in kernel memory at vaddr %016lx\n", (unsigned long)vaddr);
        return -1;
    }

    /* 6. Tính toán Địa chỉ Vật lý (Physical Address) */
    addr_t fpn = PAGING_FPN(pte);
    addr_t phyaddr;

#ifdef MM64
    phyaddr = (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif

    /* 7. Ghi trực tiếp xuống Chip RAM bằng hàm MEMPHY_write */
    // Không cần dùng _syscall vì Kernel có quyền can thiệp phần cứng
    if (MEMPHY_write(caller->krnl->mram, phyaddr, value) != 0) {
        return -1; // Lỗi: Ghi xuống RAM thất bại
    }
    return 0;
}

/*__read_user_mem - read value in user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __read_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE *data) {
    /* TO-DO: provide OS level management user memory access */
    /* 1. Validation cơ bản */
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL) {
        return -1;
    }
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1; // ID vùng nhớ (Region ID) không hợp lệ
    }
    struct mm_struct *mm = caller->krnl->mm;
    struct vm_rg_struct *currg = get_symrg_byid(mm, rgid);
    /* 2. Kiểm tra tính hợp lệ của vùng nhớ */
    if (currg == NULL) {
        return -1;
    }
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        return -1;
    }
    if (currg->rg_start + offset >= currg->rg_end) {
        return -1;
    }
    /* 3. Tính Địa chỉ Ảo (Virtual Address) */
    addr_t vaddr = currg->rg_start + offset;
    int pgn, off, fpn;
    /* 4. Tách Số hiệu Trang (PGN) và Offset */
#ifdef MM64
    pgn = vaddr >> PAGING64_ADDR_PT_SHIFT;
    off = vaddr & ((1 << PAGING64_ADDR_PT_SHIFT) - 1);
#else
    pgn = PAGING_PGN(vaddr);
    off = PAGING_OFFST(vaddr);
#endif
    /* 5. Lấy Frame vật lý (Xử lý Page Fault / Swap-in nếu cần)
     * Đây là khác biệt lớn nhất so với __read_kernel_mem!
     */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
        return -1; /* Lỗi: Truy cập trang nhớ không hợp lệ hoặc Swap lỗi */
    }
    /* 6. Tính toán Địa chỉ Vật lý (Physical Address) */
    int phyaddr;
#ifdef MM64
    phyaddr = (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif
    /* 7. Kernel trực tiếp đọc RAM vật lý */
    // Vì đây là Kernel đang làm việc (dù là đọc giùm User), nó có quyền
    // gọi thẳng MEMPHY_read mà không cần dùng _syscall như hàm libread!
    if (MEMPHY_read(caller->krnl->mram, phyaddr, data) != 0) {
        return -1; // Lỗi phần cứng RAM
    }
    return 0;
}

/*__write_user_mem - write a user region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@offset: offset to acess in memory region
 *@value: data value
 */
int __write_user_mem(struct pcb_t *caller, int vmaid, int rgid, addr_t offset, BYTE value) {
    /* TO-DO: provide OS level management user memory access */
    /* TO-DO: provide OS level management user memory access */
    /* 1. Validation cơ bản */
    if (caller == NULL || caller->krnl == NULL || caller->krnl->mm == NULL) {
        return -1;
    }
    if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ) {
        return -1; // ID vùng nhớ (Region ID) không hợp lệ
    }
    struct mm_struct *mm = caller->krnl->mm;
    struct vm_rg_struct *currg = get_symrg_byid(mm, rgid);
    /* 2. Kiểm tra tính hợp lệ của vùng nhớ */
    if (currg == NULL) {
        return -1;
    }
    if (currg->rg_start == 0 && currg->rg_end == 0) {
        return -1;
    }
    if (currg->rg_start + offset >= currg->rg_end) {
        return -1;
    }
    /* 3. Tính Địa chỉ Ảo (Virtual Address) */
    addr_t vaddr = currg->rg_start + offset;
    int pgn, off, fpn;
    /* 4. Tách Số hiệu Trang (PGN) và Offset */
#ifdef MM64
    pgn = vaddr >> PAGING64_ADDR_PT_SHIFT;
    off = vaddr & ((1 << PAGING64_ADDR_PT_SHIFT) - 1);
#else
    pgn = PAGING_PGN(vaddr);
    off = PAGING_OFFST(vaddr);
#endif
    /* 5. Lấy Frame vật lý (Xử lý Page Fault / Swap-in nếu cần)
     * Đây là khác biệt lớn nhất so với __read_kernel_mem!
     */
    if (pg_getpage(mm, pgn, &fpn, caller) != 0) {
        return -1; /* Lỗi: Truy cập trang nhớ không hợp lệ hoặc Swap lỗi */
    }
    /* 6. Tính toán Địa chỉ Vật lý (Physical Address) */
    int phyaddr;
#ifdef MM64
    phyaddr = (fpn << PAGING64_ADDR_PT_SHIFT) + off;
#else
    phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;
#endif
    // gọi thẳng MEMPHY_write mà không cần dùng _syscall như hàm libread!
    if (MEMPHY_write(caller->krnl->mram, phyaddr, value) != 0) {
        return -1; // Lỗi phần cứng RAM
    }
    return 0;
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller) {
    pthread_mutex_lock(&mmvm_lock);
    int pagenum, fpn;
    uint32_t pte;

    for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++) {
        pte = caller->krnl->mm->pgd[pagenum];

        if (PAGING_PAGE_PRESENT(pte)) {
            fpn = PAGING_FPN(pte);
            MEMPHY_put_freefp(caller->krnl->mram, fpn);
        } else {
            fpn = PAGING_SWP(pte);
            MEMPHY_put_freefp(caller->krnl->active_mswp, fpn);
        }
    }

    pthread_mutex_unlock(&mmvm_lock);
    return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, addr_t *retpgn) {
    struct pgn_t *pg = mm->fifo_pgn;

    /* TO-DO: Implement the theorical mechanism to find the victim page */
    if (!pg) {
        return -1;
    }
    struct pgn_t *prev = NULL;
    while (pg->pg_next) {
        prev = pg;
        pg = pg->pg_next;
    }
    *retpgn = pg->pgn;
    /* Xử lý an toàn khi cắt node */
    if (prev == NULL) {
        // Trường hợp danh sách chỉ có 1 phần tử
        mm->fifo_pgn = NULL;
    } else {
        // Trường hợp danh sách có nhiều phần tử
        prev->pg_next = NULL;
    }

    free(pg);

    return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg) {
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->krnl->mm, vmaid);

    struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

    if (rgit == NULL)
        return -1;

    /* Probe unintialized newrg */
    newrg->rg_start = newrg->rg_end = -1;

    /* Traverse on list of free vm region to find a fit space */
    while (rgit != NULL) {
        if (rgit->rg_start + size <= rgit->rg_end) { /* Current region has enough space */
            newrg->rg_start = rgit->rg_start;
            newrg->rg_end = rgit->rg_start + size;

            /* Update left space in chosen region */
            if (rgit->rg_start + size < rgit->rg_end) {
                rgit->rg_start = rgit->rg_start + size;
            } else { /*Use up all space, remove current node */
                /*Clone next rg node */
                struct vm_rg_struct *nextrg = rgit->rg_next;

                /*Cloning */
                if (nextrg != NULL) {
                    rgit->rg_start = nextrg->rg_start;
                    rgit->rg_end = nextrg->rg_end;

                    rgit->rg_next = nextrg->rg_next;

                    free(nextrg);
                } else {                           /*End of free list */
                    rgit->rg_start = rgit->rg_end; // dummy, size 0 region
                    rgit->rg_next = NULL;
                }
            }
            break;
        } else {
            rgit = rgit->rg_next; // Traverse next rg
        }
    }

    if (newrg->rg_start == -1) // new region not found
        return -1;

    return 0;
}

// #endif

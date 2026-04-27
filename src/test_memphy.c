/*
 * test_memphy.c — Test suite cho mm-memphy.c (CO2018)
 *
 * Compile:  gcc -o test_memphy test_memphy.c mm-memphy.c
 * Run:      ./test_memphy
 * Verbose:  ./test_memphy -v
 */

/* Redirect mm.h đến stub của mình */
#define MM_H_INCLUDED
#include "mm.h"  // Thay vì "mm_stub.h"

/* Kéo thẳng source vào để không cần build cả project */
/* Nếu compile riêng 2 file thì bỏ dòng include dưới đi */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ─── Mini test framework ─────────────────────────────────────── */

static int g_pass = 0, g_fail = 0;
static int g_verbose = 0;

#define TEST(name) static void name(void)

#define ASSERT_EQ(label, got, expect)                                      \
    do {                                                                    \
        if ((got) == (expect)) {                                            \
            g_pass++;                                                       \
            if (g_verbose)                                                  \
                printf("  [PASS] %s\n", label);                            \
        } else {                                                            \
            g_fail++;                                                       \
            printf("  [FAIL] %s — got %d, expected %d  (%s:%d)\n",        \
                   label, (int)(got), (int)(expect), __FILE__, __LINE__);  \
        }                                                                   \
    } while (0)

#define ASSERT_NEQ(label, got, not_expect)                                 \
    do {                                                                    \
        if ((got) != (not_expect)) {                                        \
            g_pass++;                                                       \
            if (g_verbose)                                                  \
                printf("  [PASS] %s\n", label);                            \
        } else {                                                            \
            g_fail++;                                                       \
            printf("  [FAIL] %s — got %d, should NOT be %d  (%s:%d)\n",  \
                   label, (int)(got), (int)(not_expect), __FILE__, __LINE__); \
        }                                                                   \
    } while (0)

#define RUN(name)                                           \
    do {                                                    \
        printf("\n[TEST] %s\n", #name);                    \
        name();                                             \
    } while (0)

/* ─── Teardown helper ──────────────────────────────────────────── */

/* Giải phóng toàn bộ frame nodes trong cả 2 list + storage.
 * Dùng ở cuối mỗi test thay cho free(mp.storage) đơn thuần. */
static void memphy_cleanup(struct memphy_struct *mp) {
    struct framephy_struct *fp, *next;
    fp = mp->free_fp_list;
    while (fp) { next = fp->fp_next; free(fp); fp = next; }
    fp = mp->used_fp_list;
    while (fp) { next = fp->fp_next; free(fp); fp = next; }
    free(mp->storage);
    mp->free_fp_list = NULL;
    mp->used_fp_list = NULL;
    mp->storage = NULL;
}

/* ─── Helpers ──────────────────────────────────────────────────── */

/* Đếm số node trong free_fp_list */
static int count_free(struct memphy_struct *mp) {
    int n = 0;
    struct framephy_struct *fp = mp->free_fp_list;
    while (fp) { n++; fp = fp->fp_next; }
    return n;
}

/* Đếm số node trong used_fp_list */
static int count_used(struct memphy_struct *mp) {
    int n = 0;
    struct framephy_struct *fp = mp->used_fp_list;
    while (fp) { n++; fp = fp->fp_next; }
    return n;
}

/* Kiểm tra fpn có nằm trong used_fp_list không */
static int in_used(struct memphy_struct *mp, addr_t fpn) {
    struct framephy_struct *fp = mp->used_fp_list;
    while (fp) {
        if (fp->fpn == (int)fpn) return 1;
        fp = fp->fp_next;
    }
    return 0;
}

/* Kiểm tra fpn có nằm trong free_fp_list không */
static int in_free(struct memphy_struct *mp, addr_t fpn) {
    struct framephy_struct *fp = mp->free_fp_list;
    while (fp) {
        if (fp->fpn == (int)fpn) return 1;
        fp = fp->fp_next;
    }
    return 0;
}

/* RAM_SZ = 4 frames (1024 / 256 = 4) để test nhanh */
#define RAM_SZ       1024
#define EXPECTED_FP  (RAM_SZ / PAGING_PAGESZ)   /* = 4 */

/* ═══════════════════════════════════════════════════════════════
 * TEST CASES
 * ═══════════════════════════════════════════════════════════════ */

/* ── TC-01: init ─────────────────────────────────────────────── */
TEST(tc01_init_basic) {
    struct memphy_struct mp;
    int ret = init_memphy(&mp, RAM_SZ, 1 /*random*/);

    ASSERT_EQ("init returns 0",      ret,               0);
    ASSERT_EQ("maxsz set",           mp.maxsz,          RAM_SZ);
    ASSERT_EQ("rdmflg set (random)", mp.rdmflg,         1);
    ASSERT_NEQ("storage allocated",  (int)(mp.storage != NULL), 0);
    ASSERT_EQ("free list has all frames", count_free(&mp), EXPECTED_FP);
    ASSERT_EQ("used list is empty",  count_used(&mp),   0);

    /* Storage phải được zero-init */
    int all_zero = 1;
    for (int i = 0; i < RAM_SZ; i++)
        if (mp.storage[i] != 0) { all_zero = 0; break; }
    ASSERT_EQ("storage zeroed",      all_zero,          1);

    memphy_cleanup(&mp);
}

TEST(tc02_init_sequential) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 0 /*sequential*/);

    ASSERT_EQ("rdmflg=0 for seq device", mp.rdmflg, 0);
    ASSERT_EQ("cursor reset to 0",       mp.cursor,  0);

    memphy_cleanup(&mp);
}

/* ── TC-03: get_freefp cơ bản ────────────────────────────────── */
TEST(tc03_get_freefp_basic) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t fpn;
    int ret = MEMPHY_get_freefp(&mp, &fpn);

    ASSERT_EQ("get returns 0",          ret,              0);
    ASSERT_EQ("free list decreases",    count_free(&mp),  EXPECTED_FP - 1);
    ASSERT_EQ("used list increases",    count_used(&mp),  1);
    ASSERT_EQ("fpn moved to used list", in_used(&mp, fpn), 1);
    ASSERT_EQ("fpn gone from free list",in_free(&mp, fpn), 0);

    memphy_cleanup(&mp);
}

/* ── TC-04: get_freefp tuần tự lấy hết tất cả frames ─────────── */
TEST(tc04_get_freefp_drain_all) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t fpn;
    for (int i = 0; i < EXPECTED_FP; i++) {
        int r = MEMPHY_get_freefp(&mp, &fpn);
        ASSERT_EQ("get succeeds each time", r, 0);
    }

    ASSERT_EQ("free list empty after drain",  count_free(&mp), 0);
    ASSERT_EQ("used list full after drain",   count_used(&mp), EXPECTED_FP);

    memphy_cleanup(&mp);
}

/* ── TC-05: get_freefp khi hết bộ nhớ ────────────────────────── */
TEST(tc05_get_freefp_oom) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t fpn;
    /* Drain all */
    for (int i = 0; i < EXPECTED_FP; i++)
        MEMPHY_get_freefp(&mp, &fpn);

    /* Một lần nữa → phải thất bại */
    int ret = MEMPHY_get_freefp(&mp, &fpn);
    ASSERT_EQ("get on empty list returns -1", ret, -1);

    memphy_cleanup(&mp);
}

/* ── TC-06: get_freefp với NULL ──────────────────────────────── */
TEST(tc06_get_freefp_null) {
    addr_t fpn;
    int ret = MEMPHY_get_freefp(NULL, &fpn);
    ASSERT_EQ("NULL mp returns -1", ret, -1);
}

/* ── TC-07: put_freefp cơ bản ────────────────────────────────── */
TEST(tc07_put_freefp_basic) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t fpn;
    MEMPHY_get_freefp(&mp, &fpn);   /* Lấy 1 frame */

    int ret = MEMPHY_put_freefp(&mp, fpn);  /* Trả lại */

    ASSERT_EQ("put returns 0",           ret,              0);
    ASSERT_EQ("free list restored",      count_free(&mp),  EXPECTED_FP);
    ASSERT_EQ("used list back to empty", count_used(&mp),  0);
    ASSERT_EQ("fpn back in free list",   in_free(&mp, fpn), 1);
    ASSERT_EQ("fpn gone from used list", in_used(&mp, fpn), 0);

    memphy_cleanup(&mp);
}

/* ── TC-08: put_freefp lấy hết rồi trả hết ──────────────────── */
TEST(tc08_put_freefp_full_cycle) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t fpns[EXPECTED_FP];
    for (int i = 0; i < EXPECTED_FP; i++)
        MEMPHY_get_freefp(&mp, &fpns[i]);

    /* Trả lại tất cả */
    for (int i = 0; i < EXPECTED_FP; i++) {
        int r = MEMPHY_put_freefp(&mp, fpns[i]);
        ASSERT_EQ("put each frame returns 0", r, 0);
    }

    ASSERT_EQ("free list fully restored", count_free(&mp), EXPECTED_FP);
    ASSERT_EQ("used list empty",          count_used(&mp), 0);

    /* Sau full cycle, vẫn có thể get lại bình thường */
    addr_t fpn;
    int r = MEMPHY_get_freefp(&mp, &fpn);
    ASSERT_EQ("can get again after full cycle", r, 0);

    memphy_cleanup(&mp);
}

/* ── TC-09: put_freefp với fpn không tồn tại trong used list ─── */
TEST(tc09_put_freefp_invalid_fpn) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    /* Không get gì cả → used list trống → put phải fail */
    int ret = MEMPHY_put_freefp(&mp, 0);
    ASSERT_EQ("put on empty used list returns -1", ret, -1);

    /* fpn không tồn tại */
    addr_t fpn;
    MEMPHY_get_freefp(&mp, &fpn);
    ret = MEMPHY_put_freefp(&mp, 9999);
    ASSERT_EQ("put non-existent fpn returns -1", ret, -1);

    memphy_cleanup(&mp);
}

/* ── TC-10: put_freefp với NULL ──────────────────────────────── */
TEST(tc10_put_freefp_null) {
    int ret = MEMPHY_put_freefp(NULL, 0);
    ASSERT_EQ("NULL mp returns -1", ret, -1);
}

/* ── TC-11: read/write trên random-access device ─────────────── */
TEST(tc11_readwrite_random) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1 /*random*/);

    BYTE val;
    /* Ghi tại addr 100 */
    int r = MEMPHY_write(&mp, 100, 0xAB);
    ASSERT_EQ("write returns 0", r, 0);

    /* Đọc lại */
    r = MEMPHY_read(&mp, 100, &val);
    ASSERT_EQ("read returns 0",    r,   0);
    ASSERT_EQ("read-back correct", val, 0xAB);

    /* Ghi nhiều địa chỉ */
    MEMPHY_write(&mp, 0,   0x01);
    MEMPHY_write(&mp, 255, 0xFF);
    MEMPHY_read(&mp, 0,   &val); ASSERT_EQ("addr 0 correct",   val, 0x01);
    MEMPHY_read(&mp, 255, &val); ASSERT_EQ("addr 255 correct", val, 0xFF);

    /* Ghi đè */
    MEMPHY_write(&mp, 100, 0x00);
    MEMPHY_read(&mp,  100, &val);
    ASSERT_EQ("overwrite correct", val, 0x00);

    memphy_cleanup(&mp);
}

/* ── TC-12: read/write trên sequential device ────────────────── */
TEST(tc12_readwrite_sequential) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 0 /*sequential*/);

    /* seq_write yêu cầu rdmflg != 0 theo code gốc → sẽ return -1
     * Test này xác nhận đúng hành vi (sequential device reject write/read) */
    BYTE val;
    int rw = MEMPHY_write(&mp, 50, 0x55);
    int rr = MEMPHY_read(&mp,  50, &val);

    /* Theo code: seq_write/seq_read return -1 khi !rdmflg */
    ASSERT_EQ("seq write on !rdmflg returns -1", rw, -1);
    ASSERT_EQ("seq read  on !rdmflg returns -1", rr, -1);

    memphy_cleanup(&mp);
}

/* ── TC-13: read/write NULL device ───────────────────────────── */
TEST(tc13_readwrite_null) {
    BYTE val;
    ASSERT_EQ("read  NULL mp = -1", MEMPHY_read(NULL,  0, &val), -1);
    ASSERT_EQ("write NULL mp = -1", MEMPHY_write(NULL, 0, 0x00), -1);
}

/* ── TC-14: no overlap — 2 frame độc lập nhau ───────────────── */
TEST(tc14_no_frame_overlap) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t f0, f1;
    MEMPHY_get_freefp(&mp, &f0);
    MEMPHY_get_freefp(&mp, &f1);

    ASSERT_NEQ("two frames have different fpn", (int)f0, (int)f1);

    /* Ghi vào frame 0, kiểm tra frame 1 không bị ảnh hưởng */
    addr_t base0 = f0 * PAGING_PAGESZ;
    addr_t base1 = f1 * PAGING_PAGESZ;
    MEMPHY_write(&mp, base0, 0xAA);
    BYTE v1;
    MEMPHY_read(&mp, base1, &v1);
    ASSERT_EQ("write to frame0 does not affect frame1", v1, 0x00);

    memphy_cleanup(&mp);
}

/* ── TC-15: MEMPHY_dump smoke test (không crash là pass) ─────── */
TEST(tc15_dump_smoke) {
    struct memphy_struct mp;
    init_memphy(&mp, RAM_SZ, 1);

    addr_t fpn;
    MEMPHY_get_freefp(&mp, &fpn);
    MEMPHY_write(&mp, 10, 0xDE);
    MEMPHY_write(&mp, 11, 0xAD);

    /* Nếu dump không crash và return 0 → pass */
    printf("  [INFO] Output của MEMPHY_dump:\n");
    int r = MEMPHY_dump(&mp);
    ASSERT_EQ("dump returns 0", r, 0);

    memphy_cleanup(&mp);
}

/* ═══════════════════════════════════════════════════════════════
 * MAIN
 * ═══════════════════════════════════════════════════════════════ */

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "-v") == 0)
        g_verbose = 1;

    printf("========================================\n");
    printf(" mm-memphy test suite  (PAGING_PAGESZ=%d)\n", PAGING_PAGESZ);
    printf("========================================\n");

    /* Init */
    RUN(tc01_init_basic);
    RUN(tc02_init_sequential);

    /* get_freefp */
    RUN(tc03_get_freefp_basic);
    RUN(tc04_get_freefp_drain_all);
    RUN(tc05_get_freefp_oom);
    RUN(tc06_get_freefp_null);

    /* put_freefp */
    RUN(tc07_put_freefp_basic);
    RUN(tc08_put_freefp_full_cycle);
    RUN(tc09_put_freefp_invalid_fpn);
    RUN(tc10_put_freefp_null);

    /* read/write */
    RUN(tc11_readwrite_random);
    RUN(tc12_readwrite_sequential);
    RUN(tc13_readwrite_null);

    /* Integration */
    RUN(tc14_no_frame_overlap);
    RUN(tc15_dump_smoke);

    printf("\n========================================\n");
    printf(" PASSED: %d  |  FAILED: %d  |  TOTAL: %d\n",
           g_pass, g_fail, g_pass + g_fail);
    printf("========================================\n");

    return (g_fail == 0) ? 0 : 1;
}

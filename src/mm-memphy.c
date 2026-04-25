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
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, addr_t offset)
{
   int numstep = 0;

   mp->cursor = 0;
   while (numstep < offset && numstep < mp->maxsz)
   {
      /* Traverse sequentially */
      mp->cursor = (mp->cursor + 1) % mp->maxsz;
      numstep++;
   }

   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, addr_t addr, BYTE *value)
{
   if (mp == NULL)
      return -1;

   if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   *value = (BYTE)mp->storage[addr];

   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct *mp, addr_t addr, BYTE *value)
{
   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      *value = mp->storage[addr];
   else /* Sequential access device */
      return MEMPHY_seq_read(mp, addr, value);

   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct *mp, addr_t addr, BYTE value)
{

   if (mp == NULL)
      return -1;

   if (!mp->rdmflg)
      return -1; /* Not compatible mode for sequential read */

   MEMPHY_mv_csr(mp, addr);
   mp->storage[addr] = value;

   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct *mp, addr_t addr, BYTE data)
{
   if (mp == NULL)
      return -1;

   if (mp->rdmflg)
      mp->storage[addr] = data;
   else /* Sequential access device */
      return MEMPHY_seq_write(mp, addr, data);

   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
   /* This setting come with fixed constant PAGESZ */
   int numfp = mp->maxsz / pagesz;
   struct framephy_struct *newfst, *fst;
   int iter = 0;
 
   if (numfp <= 0)
      return -1;
 
   /* Init head of free framephy list */
   fst = malloc(sizeof(struct framephy_struct));
   fst->fpn = iter;
   fst->fp_next = NULL; /* FIX: explicitly initialize fp_next of head node */
   mp->free_fp_list = fst;
 
   /* Fill in the rest num-1 element members */
   for (iter = 1; iter < numfp; iter++)
   {
      newfst = malloc(sizeof(struct framephy_struct));
      newfst->fpn = iter;
      newfst->fp_next = NULL;
      fst->fp_next = newfst;
      fst = newfst;
   }
 
   /* used_fp_list starts empty */
   mp->used_fp_list = NULL;
 
   return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, addr_t *retfpn)
{
   /* 1. Validate and check availability */
   if (mp == NULL || mp->free_fp_list == NULL)
      return -1; /* Out of memory */
 
   /* 2. Pop the head of free_fp_list */
   struct framephy_struct *fp = mp->free_fp_list;
   mp->free_fp_list = fp->fp_next;
 
   /* 3. Return the frame number to caller */
   *retfpn = fp->fpn;
 
   /* 4. Push onto used_fp_list (prepend — O(1)) */
   fp->fp_next = mp->used_fp_list;
   mp->used_fp_list = fp;
 
   return 0;
}

int MEMPHY_dump(struct memphy_struct *mp)
{
   if (mp == NULL)
      return -1;
 
   printf("\n---- MEMPHY DUMP (maxsz=%d) ----\n", mp->maxsz);
 
   /* Count and list free frames */
   printf("Free frames : ");
   int free_cnt = 0;
   struct framephy_struct *fp = mp->free_fp_list;
   while (fp != NULL)
   {
      printf("%d ", fp->fpn);
      free_cnt++;
      fp = fp->fp_next;
   }
   printf("(total: %d)\n", free_cnt);
 
   /* Count and list used frames */
   printf("Used frames : ");
   int used_cnt = 0;
   fp = mp->used_fp_list;
   while (fp != NULL)
   {
      printf("%d ", fp->fpn);
      used_cnt++;
      fp = fp->fp_next;
   }
   printf("(total: %d)\n", used_cnt);
 
   /* Hex dump of first portion of storage (up to 256 bytes) */
   int dump_bytes = (mp->maxsz < 256) ? mp->maxsz : 256;
   printf("Storage[0..%d]:\n", dump_bytes - 1);
   for (int i = 0; i < dump_bytes; i++)
   {
      if (i % 16 == 0)
         printf("  [%04x] ", i);
      printf("%02x ", (unsigned char)mp->storage[i]);
      if ((i + 1) % 16 == 0)
         printf("\n");
   }
   if (dump_bytes % 16 != 0)
      printf("\n");
 
   printf("--------------------------------\n\n");
   return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, addr_t fpn)
{
   if (mp == NULL)
      return -1;
 
   struct framephy_struct *curr = mp->used_fp_list;
   struct framephy_struct *prev = NULL;
 
   /* 1. Find the frame in used_fp_list */
   while (curr != NULL && curr->fpn != (int)fpn)
   {
      prev = curr;
      curr = curr->fp_next;
   }
 
   /* Frame not found in used list */
   if (curr == NULL)
      return -1;
 
   /* 2. Unlink from used_fp_list */
   if (prev == NULL)
      mp->used_fp_list = curr->fp_next; /* head removal */
   else
      prev->fp_next = curr->fp_next;    /* mid/tail removal */
 
   /* 3. Prepend to free_fp_list (O(1)) */
   curr->fp_next = mp->free_fp_list;
   mp->free_fp_list = curr;
 
   return 0;
}

/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, addr_t max_size, int randomflg)
{
   mp->storage = (BYTE *)malloc(max_size * sizeof(BYTE));
   mp->maxsz = max_size;
   memset(mp->storage, 0, max_size * sizeof(BYTE));

   MEMPHY_format(mp, PAGING_PAGESZ);

   mp->rdmflg = (randomflg != 0) ? 1 : 0;

   if (!mp->rdmflg) /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;

   return 0;
}

// #endif

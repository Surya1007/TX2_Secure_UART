/*
 * Copyright (c) 2017, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <arch.h>
#include <arch_helpers.h>
#include <assert.h>
#include <cassert.h>
#include <common_def.h>
#include <debug.h>
#include <errno.h>
#include <platform_def.h>
#include <stdbool.h>
#include <string.h>
#include <types.h>
#include <utils.h>
#include <xlat_tables_v2.h>
#ifdef AARCH32
# include "aarch32/xlat_tables_arch.h"
#else
# include "aarch64/xlat_tables_arch.h"
#endif
#include "xlat_tables_private.h"

#if PLAT_XLAT_TABLES_DYNAMIC

/*
 * The following functions assume that they will be called using subtables only.
 * The base table can't be unmapped, so it is not needed to do any special
 * handling for it.
 */

/*
 * Returns the index of the array corresponding to the specified translation
 * table.
 */
static int32_t xlat_table_get_index(xlat_ctx_t *ctx, const uint64_t *table)
{
	for (uint32_t i = 0; i < ctx->tables_num; i++) {
		if (ctx->tables[i] == table) {
			return (int32_t)i;
		}
	}

	/*
	 * Maybe we were asked to get the index of the base level table, which
	 * should never happen.
	 */
	assert(false);

	return -1;
}

/* Returns a pointer to an empty translation table. */
static uint64_t *xlat_table_get_empty(xlat_ctx_t *ctx)
{
	for (uint32_t i = 0; i < ctx->tables_num; i++) {
		if (ctx->tables_mapped_regions[i] == 0) {
			return ctx->tables[i];
		}
	}

	return NULL;
}

/* Increments region count for a given table. */
static void xlat_table_inc_regions_count(xlat_ctx_t *ctx, const uint64_t *table)
{
	ctx->tables_mapped_regions[xlat_table_get_index(ctx, table)]++;
}

/* Decrements region count for a given table. */
static void xlat_table_dec_regions_count(xlat_ctx_t *ctx, const uint64_t *table)
{
	ctx->tables_mapped_regions[xlat_table_get_index(ctx, table)]--;
}

/* Returns 0 if the speficied table isn't empty, otherwise 1. */
static bool xlat_table_is_empty(xlat_ctx_t *ctx, const uint64_t *table)
{
	return (ctx->tables_mapped_regions[xlat_table_get_index(ctx, table)] == 0);
}

#else /* PLAT_XLAT_TABLES_DYNAMIC */

/* Returns a pointer to the first empty translation table. */
static uint64_t *xlat_table_get_empty(xlat_ctx_t *ctx)
{
	assert(ctx->next_table < ctx->tables_num);

	return ctx->tables[ctx->next_table++];
}

#endif /* PLAT_XLAT_TABLES_DYNAMIC */

/* Returns a block/page table descriptor for the given level and attributes. */
static uint64_t xlat_desc(uint32_t attr, uint64_t addr_pa,
			  uint32_t level, uint64_t execute_never_mask)
{
	uint64_t desc;
	uint32_t mem_type;

	/* Make sure that the granularity is fine enough to map this address. */
	assert((addr_pa & XLAT_BLOCK_MASK(level)) == 0U);

	desc = addr_pa;
	/*
	 * There are different translation table descriptors for level 3 and the
	 * rest.
	 */
	desc |= (level == XLAT_TABLE_LEVEL_MAX) ? PAGE_DESC : BLOCK_DESC;
	/*
	 * Always set the access flag, as TF doesn't manage access flag faults.
	 * Deduce other fields of the descriptor based on the MT_NS and MT_RW
	 * memory region attributes.
	 */
	desc |= ((attr & MT_NS) != 0U) ? LOWER_ATTRS(NS) : 0ULL;
	desc |= ((attr & MT_RW) != 0U) ? LOWER_ATTRS(AP_RW) : LOWER_ATTRS(AP_RO);
	desc |= LOWER_ATTRS(ACCESS_FLAG);

	/*
	 * Deduce shareability domain and executability of the memory region
	 * from the memory type of the attributes (MT_TYPE).
	 *
	 * Data accesses to device memory and non-cacheable normal memory are
	 * coherent for all observers in the system, and correspondingly are
	 * always treated as being Outer Shareable. Therefore, for these 2 types
	 * of memory, it is not strictly needed to set the shareability field
	 * in the translation tables.
	 */
	mem_type = MT_TYPE(attr);
	if (mem_type == (uint32_t)MT_DEVICE) {
		desc |= LOWER_ATTRS(ATTR_DEVICE_INDEX | OSH);
		/*
		 * Always map device memory as execute-never.
		 * This is to avoid the possibility of a speculative instruction
		 * fetch, which could be an issue if this memory region
		 * corresponds to a read-sensitive peripheral.
		 */
		desc |= execute_never_mask;

	} else { /* Normal memory */
		/*
		 * Always map read-write normal memory as execute-never.
		 * (Trusted Firmware doesn't self-modify its code, therefore
		 * R/W memory is reserved for data storage, which must not be
		 * executable.)
		 * Note that setting the XN bit here is for consistency only.
		 * The function that enables the MMU sets the SCTLR_ELx.WXN bit,
		 * which makes any writable memory region to be treated as
		 * execute-never, regardless of the value of the XN bit in the
		 * translation table.
		 *
		 * For read-only memory, rely on the MT_EXECUTE/MT_EXECUTE_NEVER
		 * attribute to figure out the value of the XN bit.
		 */
		if (((attr & MT_RW) != 0U) || ((attr & MT_EXECUTE_NEVER) != 0U)) {
			desc |= execute_never_mask;
		}

		if (mem_type == MT_MEMORY) {
			desc |= LOWER_ATTRS(ATTR_IWBWA_OWBWA_NTR_INDEX | ISH);
		} else {
			assert(mem_type == MT_NON_CACHEABLE);
			desc |= LOWER_ATTRS(ATTR_NON_CACHEABLE_INDEX | OSH);
		}
	}

	return desc;
}

/*
 * Enumeration of actions that can be made when mapping table entries depending
 * on the previous value in that entry and information about the region being
 * mapped.
 */
typedef enum {

	/* Do nothing */
	ACTION_NONE,

	/* Write a block (or page, if in level 3) entry. */
	ACTION_WRITE_BLOCK_ENTRY,

	/*
	 * Create a new table and write a table entry pointing to it. Recurse
	 * into it for further processing.
	 */
	ACTION_CREATE_NEW_TABLE,

	/*
	 * There is a table descriptor in this entry, read it and recurse into
	 * that table for further processing.
	 */
	ACTION_RECURSE_INTO_TABLE,

} action_t;

#if PLAT_XLAT_TABLES_DYNAMIC

/*
 * Recursive function that writes to the translation tables and unmaps the
 * specified region.
 */
static void xlat_tables_unmap_region(xlat_ctx_t *ctx, mmap_region_t *mm,
				     const uintptr_t table_base_va,
				     uint64_t *const table_base,
				     const uint32_t table_entries,
				     const uint32_t level)
{
	assert(level >= ctx->base_level && level <= XLAT_TABLE_LEVEL_MAX);

	uint64_t *subtable;
	uint64_t desc;

	uintptr_t table_idx_va;
	uintptr_t table_idx_end_va; /* End VA of this entry */

	uintptr_t region_end_va = mm->base_va + mm->size - 1U;

	uint32_t table_idx;

	if (mm->base_va > table_base_va) {
		/* Find the first index of the table affected by the region. */
		table_idx_va = mm->base_va & ~XLAT_BLOCK_MASK(level);

		table_idx = (uint32_t)((table_idx_va - table_base_va) >>
			    XLAT_ADDR_SHIFT(level));

		assert(table_idx < table_entries);
	} else {
		/* Start from the beginning of the table. */
		table_idx_va = table_base_va;
		table_idx = 0U;
	}

	while (table_idx < table_entries) {

		table_idx_end_va = table_idx_va + XLAT_BLOCK_SIZE(level) - 1U;

		desc = table_base[table_idx];
		uint64_t desc_type = desc & DESC_MASK;

		action_t action = ACTION_NONE;

		if ((mm->base_va <= table_idx_va) &&
		    (region_end_va >= table_idx_end_va)) {

			/* Region covers all block */

			if (level == 3U) {
				/*
				 * Last level, only page descriptors allowed,
				 * erase it.
				 */
				assert(desc_type == PAGE_DESC);

				action = ACTION_WRITE_BLOCK_ENTRY;
			} else {
				/*
				 * Other levels can have table descriptors. If
				 * so, recurse into it and erase descriptors
				 * inside it as needed. If there is a block
				 * descriptor, just erase it. If an invalid
				 * descriptor is found, this table isn't
				 * actually mapped, which shouldn't happen.
				 */
				if (desc_type == TABLE_DESC) {
					action = ACTION_RECURSE_INTO_TABLE;
				} else {
					assert(desc_type == BLOCK_DESC);
					action = ACTION_WRITE_BLOCK_ENTRY;
				}
			}

		} else if ((mm->base_va <= table_idx_end_va) ||
			   (region_end_va >= table_idx_va)) {

			/*
			 * Region partially covers block.
			 *
			 * It can't happen in level 3.
			 *
			 * There must be a table descriptor here, if not there
			 * was a problem when mapping the region.
			 */

			assert(level < 3U);

			assert(desc_type == TABLE_DESC);

			action = ACTION_RECURSE_INTO_TABLE;
		} else {
			; /* do nothing */
		}

		if (action == ACTION_WRITE_BLOCK_ENTRY) {

			table_base[table_idx] = INVALID_DESC;
			xlat_arch_tlbi_va(table_idx_va);

		} else if (action == ACTION_RECURSE_INTO_TABLE) {

			subtable = (uint64_t *)(uintptr_t)(desc & TABLE_ADDR_MASK);

			/* Recurse to write into subtable */
			xlat_tables_unmap_region(ctx, mm, table_idx_va,
						 subtable, XLAT_TABLE_ENTRIES,
						 level + 1U);

			/*
			 * If the subtable is now empty, remove its reference.
			 */
			if (xlat_table_is_empty(ctx, subtable)) {
				table_base[table_idx] = INVALID_DESC;
				xlat_arch_tlbi_va(table_idx_va);
			}

		} else {
			assert(action == ACTION_NONE);
		}

		table_idx++;
		table_idx_va += XLAT_BLOCK_SIZE(level);

		/* If reached the end of the region, exit */
		if (region_end_va <= table_idx_va) {
			break;
		}
	}

	if (level > ctx->base_level) {
		xlat_table_dec_regions_count(ctx, table_base);
	}
}

#endif /* PLAT_XLAT_TABLES_DYNAMIC */

/*
 * From the given arguments, it decides which action to take when mapping the
 * specified region.
 */
static action_t xlat_tables_map_region_action(const mmap_region_t *mm,
		const uint64_t desc_type, const uint64_t dest_pa,
		const uintptr_t table_entry_base_va, const uint32_t level)
{
	uintptr_t mm_end_va = mm->base_va + mm->size - 1U;
	uintptr_t table_entry_end_va =
			table_entry_base_va + XLAT_BLOCK_SIZE(level) - 1U;

	/*
	 * The descriptor types allowed depend on the current table level.
	 */

	if ((mm->base_va <= table_entry_base_va) &&
	    (mm_end_va >= table_entry_end_va)) {

		/*
		 * Table entry is covered by region
		 * --------------------------------
		 *
		 * This means that this table entry can describe the whole
		 * translation with this granularity in principle.
		 */

		if (level == 3U) {
			/*
			 * Last level, only page descriptors are allowed.
			 */
			if (desc_type == PAGE_DESC) {
				/*
				 * There's another region mapped here, don't
				 * overwrite.
				 */
				return ACTION_NONE;
			} else {
				assert(desc_type == INVALID_DESC);
				return ACTION_WRITE_BLOCK_ENTRY;
			}

		} else {

			/*
			 * Other levels. Table descriptors are allowed. Block
			 * descriptors too, but they have some limitations.
			 */

			if (desc_type == TABLE_DESC) {
				/* There's already a table, recurse into it. */
				return ACTION_RECURSE_INTO_TABLE;

			} else if (desc_type == INVALID_DESC) {
				/*
				 * There's nothing mapped here, create a new
				 * entry.
				 *
				 * Check if the destination granularity allows
				 * us to use a block descriptor or we need a
				 * finer table for it.
				 *
				 * Also, check if the current level allows block
				 * descriptors. If not, create a table instead.
				 */
				if (((dest_pa & XLAT_BLOCK_MASK(level)) != 0U)||
				    (level < MIN_LVL_BLOCK_DESC)) {
					return ACTION_CREATE_NEW_TABLE;
				} else {
					return ACTION_WRITE_BLOCK_ENTRY;
				}

			} else {
				/*
				 * There's another region mapped here, don't
				 * overwrite.
				 */
				assert(desc_type == BLOCK_DESC);

				return ACTION_NONE;
			}
		}

	} else if ((mm->base_va <= table_entry_end_va) ||
		   (mm_end_va >= table_entry_base_va)) {

		/*
		 * Region partially covers table entry
		 * -----------------------------------
		 *
		 * This means that this table entry can't describe the whole
		 * translation, a finer table is needed.

		 * There cannot be partial block overlaps in level 3. If that
		 * happens, some of the preliminary checks when adding the
		 * mmap region failed to detect that PA and VA must at least be
		 * aligned to PAGE_SIZE.
		 */
		assert(level < 3U);

		if (desc_type == INVALID_DESC) {
			/*
			 * The block is not fully covered by the region. Create
			 * a new table, recurse into it and try to map the
			 * region with finer granularity.
			 */
			return ACTION_CREATE_NEW_TABLE;

		} else {
			assert(desc_type == TABLE_DESC);
			/*
			 * The block is not fully covered by the region, but
			 * there is already a table here. Recurse into it and
			 * try to map with finer granularity.
			 *
			 * PAGE_DESC for level 3 has the same value as
			 * TABLE_DESC, but this code can't run on a level 3
			 * table because there can't be overlaps in level 3.
			 */
			return ACTION_RECURSE_INTO_TABLE;
		}
	} else {
		; /* do nothing */
	}

	/*
	 * This table entry is outside of the region specified in the arguments,
	 * don't write anything to it.
	 */
	return ACTION_NONE;
}

/*
 * Recursive function that writes to the translation tables and maps the
 * specified region. On success, it returns the VA of the last byte that was
 * succesfully mapped. On error, it returns the VA of the next entry that
 * should have been mapped.
 */
static uintptr_t xlat_tables_map_region(xlat_ctx_t *ctx, mmap_region_t *mm,
				   const uintptr_t table_base_va,
				   uint64_t *const table_base,
				   const uint32_t table_entries,
				   const uint32_t level)
{
	assert(level >= ctx->base_level && level <= XLAT_TABLE_LEVEL_MAX);

	uintptr_t mm_end_va = mm->base_va + mm->size - 1U;

	uintptr_t table_idx_va;
	uint64_t table_idx_pa;

	uint64_t *subtable;
	uint64_t desc;

	uint32_t table_idx;

	if (mm->base_va > table_base_va) {
		/* Find the first index of the table affected by the region. */
		table_idx_va = mm->base_va & ~XLAT_BLOCK_MASK(level);

		table_idx = (uint32_t)((table_idx_va - table_base_va) >>
			    XLAT_ADDR_SHIFT(level));

		assert(table_idx < table_entries);
	} else {
		/* Start from the beginning of the table. */
		table_idx_va = table_base_va;
		table_idx = 0;
	}

#if PLAT_XLAT_TABLES_DYNAMIC
	if (level > ctx->base_level) {
		xlat_table_inc_regions_count(ctx, table_base);
	}
#endif

	while (table_idx < table_entries) {

		desc = table_base[table_idx];

		table_idx_pa = mm->base_pa + table_idx_va - mm->base_va;

		action_t action = xlat_tables_map_region_action(mm,
			desc & DESC_MASK, table_idx_pa, table_idx_va, level);

		if (action == ACTION_WRITE_BLOCK_ENTRY) {

			table_base[table_idx] =
				xlat_desc(mm->attr, table_idx_pa, level,
					  ctx->execute_never_mask);

		} else if (action == ACTION_CREATE_NEW_TABLE) {

			subtable = xlat_table_get_empty(ctx);
			if (subtable == NULL) {
				/* Not enough free tables to map this region */
				return table_idx_va;
			}

			/* Point to new subtable from this one. */
			table_base[table_idx] = TABLE_DESC | (uint64_t)subtable;

			/* Recurse to write into subtable */
			uintptr_t end_va = xlat_tables_map_region(ctx, mm, table_idx_va,
					       subtable, XLAT_TABLE_ENTRIES,
					       level + 1U);
			if (end_va != table_idx_va + XLAT_BLOCK_SIZE(level) - 1U) {
				return end_va;
			}

		} else if (action == ACTION_RECURSE_INTO_TABLE) {

			subtable = (uint64_t *)(uintptr_t)(desc & TABLE_ADDR_MASK);
			/* Recurse to write into subtable */
			uintptr_t end_va =  xlat_tables_map_region(ctx, mm, table_idx_va,
					       subtable, (uint32_t)XLAT_TABLE_ENTRIES,
					       level + 1U);
			if (end_va != (table_idx_va + XLAT_BLOCK_SIZE(level) - 1U)) {
				return end_va;
			}

		} else {

			assert(action == ACTION_NONE);

		}

		table_idx++;
		table_idx_va += XLAT_BLOCK_SIZE(level);

		/* If reached the end of the region, exit */
		if (mm_end_va <= table_idx_va) {
			break;
		}
	}

	return table_idx_va - 1U;
}

void print_mmap(mmap_region_t *const mmap)
{
#if LOG_LEVEL >= LOG_LEVEL_VERBOSE
	tf_printf("mmap:\n");
	mmap_region_t *mm = mmap;

	while (mm->size != 0U) {
		tf_printf(" VA:%p  PA:0x%lx  size:0x%zx  attr:0x%x\n", // CHANGED: original PA:0x%llx
				(void *)mm->base_va, mm->base_pa,
				mm->size, mm->attr);
		++mm;
	};
	tf_printf("\n");
#endif
}

/*
 * Function that verifies that a region can be mapped.
 * Returns:
 *        0: Success, the mapping is allowed.
 *   EINVAL: Invalid values were used as arguments.
 *   ERANGE: The memory limits were surpassed.
 *   ENOMEM: There is not enough memory in the mmap array.
 *    EPERM: Region overlaps another one in an invalid way.
 */
static int32_t mmap_add_region_check(xlat_ctx_t *ctx, uint64_t base_pa,
				 uintptr_t base_va, size_t size,
				 uint32_t attr)
{
	mmap_region_t *mm = ctx->mmap;
	uint64_t end_pa = base_pa + size - 1U;
	uintptr_t end_va = base_va + size - 1U;

	if (!IS_PAGE_ALIGNED(base_pa) || !IS_PAGE_ALIGNED(base_va) ||
			!IS_PAGE_ALIGNED(size)) {
		return -EINVAL;
	}

	/* Check for overflows */
	if ((base_pa > end_pa) || (base_va > end_va)) {
		return -ERANGE;
	}

	if ((base_va + (uintptr_t)size - (uintptr_t)1) > ctx->va_max_address) {
		return -ERANGE;
	}

	if ((base_pa + (uint64_t)size - 1ULL) > ctx->pa_max_address) {
		return -ERANGE;
	}

	/* Check that there is space in the mmap array */
	if (ctx->mmap[ctx->mmap_num - 1U].size != 0U) {
		return -ENOMEM;
	}

	/* Check for PAs and VAs overlaps with all other regions */
	for (mm = ctx->mmap; mm->size != 0U; ++mm) {

		uintptr_t mm_end_va = mm->base_va + mm->size - 1U;

		/*
		 * Check if one of the regions is completely inside the other
		 * one.
		 */
		bool fully_overlapped_va =
			((base_va >= mm->base_va) && (end_va <= mm_end_va)) ||
			((mm->base_va >= base_va) && (mm_end_va <= end_va));

		/*
		 * Full VA overlaps are only allowed if both regions are
		 * identity mapped (zero offset) or have the same VA to PA
		 * offset. Also, make sure that it's not the exact same area.
		 * This can only be done with static regions.
		 */
		if (fully_overlapped_va) {

#if PLAT_XLAT_TABLES_DYNAMIC
			if (((attr & MT_DYNAMIC) != 0U) || ((mm->attr & MT_DYNAMIC) != 0U)) {
				return -EPERM;
			}
#endif /* PLAT_XLAT_TABLES_DYNAMIC */
			if ((mm->base_va - mm->base_pa) != (base_va - base_pa)) {
				return -EPERM;
			}

			if ((base_va == mm->base_va) && (size == mm->size)) {
				return -EPERM;
			}

		} else {
			/*
			 * If the regions do not have fully overlapping VAs,
			 * then they must have fully separated VAs and PAs.
			 * Partial overlaps are not allowed
			 */

			uint64_t mm_end_pa =
						     mm->base_pa + mm->size - 1U;

			bool separated_pa =
				(end_pa < mm->base_pa) || (base_pa > mm_end_pa);
			bool separated_va =
				(end_va < mm->base_va) || (base_va > mm_end_va);

			if (!(separated_va && separated_pa)) {
				return -EPERM;
			}
		}
	}

	return 0;
}

void mmap_add_region_ctx(xlat_ctx_t *ctx, mmap_region_t *mm)
{
	mmap_region_t *mm_cursor = ctx->mmap, *mm_destination;
	mmap_region_t *mm_last, *mm_end = ctx->mmap + ctx->mmap_num;
	uint32_t mm_num = U(0);
	uint64_t end_pa = mm->base_pa + mm->size - 1U;
	uintptr_t end_va = mm->base_va + mm->size - 1U;
	int32_t ret;

	/* Ignore empty regions */
	if (mm->size == 0U) {
		return;
	}

	/* Static regions must be added before initializing the xlat tables. */
	assert(ctx->initialized == 0U);

	ret = mmap_add_region_check(ctx, mm->base_pa, mm->base_va, mm->size,
				    mm->attr);
	if (ret != 0) {
		ERROR("mmap_add_region_check() failed. error %d\n", ret);
		assert(false);
		return;
	}

	/*
	 * Find correct place in mmap to insert new region.
	 *
	 * 1 - Lower region VA end first.
	 * 2 - Smaller region size first.
	 *
	 * VA  0                                   0xFF
	 *
	 * 1st |------|
	 * 2nd |------------|
	 * 3rd                 |------|
	 * 4th                            |---|
	 * 5th                                   |---|
	 * 6th                            |----------|
	 * 7th |-------------------------------------|
	 *
	 * This is required for overlapping regions only. It simplifies adding
	 * regions with the loop in xlat_tables_init_internal because the outer
	 * ones won't overwrite block or page descriptors of regions added
	 * previously.
	 *
	 * Overlapping is only allowed for static regions.
	 */

	while (((mm_cursor->base_va + mm_cursor->size - 1U) < end_va)
	       && (mm_cursor->size != 0U)) {
		++mm_cursor;
	}

	while (((mm_cursor->base_va + mm_cursor->size - 1U) == end_va)
	       && (mm_cursor->size < mm->size)) {
		++mm_cursor;
	}

	/*
	 * Find the last entry marker in the mmap
	 */
	mm_last = ctx->mmap;
	while ((mm_last->size != 0U) && (mm_last < mm_end)) {
		++mm_last;
	}

	/*
	 * Check if we have enough space in the memory mapping table.
	 * This shouldn't happen as we have checked in mmap_add_region_check
	 * that there is free space.
	 */
	assert(mm_last->size == 0U);

	/* Find out the number of regions to move */
	mm_destination = mm_cursor;
	while (mm_destination < mm_last) {
		++mm_num;
		++mm_destination;
	}

	/* Make room for new region by moving other regions up by one place */
	mm_destination = mm_cursor + 1;
	(void)memmove(mm_destination, mm_cursor, sizeof(mmap_region_t) * mm_num);

	/*
	 * Check we haven't lost the empty sentinel from the end of the array.
	 * This shouldn't happen as we have checked in mmap_add_region_check
	 * that there is free space.
	 */
	assert(mm_end->size == 0U);

	mm_cursor->base_pa = mm->base_pa;
	mm_cursor->base_va = mm->base_va;
	mm_cursor->size = mm->size;
	mm_cursor->attr = mm->attr;

	if (end_pa > ctx->max_pa) {
		ctx->max_pa = end_pa;
	}
	if (end_va > ctx->max_va) {
		ctx->max_va = end_va;
	}
}

#if PLAT_XLAT_TABLES_DYNAMIC

int32_t mmap_add_dynamic_region_ctx(xlat_ctx_t *ctx, mmap_region_t *mm)
{
	mmap_region_t *mm_cursor = ctx->mmap, *mm_destination;
	mmap_region_t *mm_last = mm_cursor + ctx->mmap_num;
	uint32_t mm_num = U(0);
	uint64_t end_pa = mm->base_pa + mm->size - 1U;
	uintptr_t end_va = mm->base_va + mm->size - 1U;
	int32_t ret;

	/* Nothing to do */
	if (mm->size == 0U) {
		return 0;
	}

	ret = mmap_add_region_check(ctx, mm->base_pa, mm->base_va, mm->size, mm->attr | MT_DYNAMIC);
	if (ret != 0) {
		return ret;
	}

	/*
	 * Find the adequate entry in the mmap array in the same way done for
	 * static regions in mmap_add_region_ctx().
	 */

	while (((mm_cursor->base_va + mm_cursor->size - 1U) < end_va) && (mm_cursor->size != 0U)) {
		++mm_cursor;
	}

	while (((mm_cursor->base_va + mm_cursor->size - 1U) == end_va) && (mm_cursor->size < mm->size)) {
		++mm_cursor;
	}

	/* Find out the number of regions to move */
	mm_destination = mm_cursor;
	while (mm_destination < mm_last) {
		++mm_num;
		++mm_destination;
	}

	/* Make room for new region by moving other regions up by one place */
	mm_destination = mm_cursor + 1;
	(void)memmove(mm_destination, mm_cursor, sizeof(mmap_region_t) * mm_num);

	/*
	 * Check we haven't lost the empty sentinal from the end of the array.
	 * This shouldn't happen as we have checked in mmap_add_region_check
	 * that there is free space.
	 */
	assert(mm_last->size == 0U);

	mm_cursor->base_pa = mm->base_pa;
	mm_cursor->base_va = mm->base_va;
	mm_cursor->size = mm->size;
	mm_cursor->attr = (uint32_t)mm->attr | MT_DYNAMIC;

	/*
	 * Update the translation tables if the xlat tables are initialized. If
	 * not, this region will be mapped when they are initialized.
	 */
	if (ctx->initialized != 0U) {
		end_va = xlat_tables_map_region(ctx, mm_cursor, 0, ctx->base_table,
				ctx->base_table_entries, ctx->base_level);

		/* Failed to map, remove mmap entry, unmap and return error. */
		if (end_va != mm_cursor->base_va + mm_cursor->size - 1U) {
			/* Find out the number of regions to move */
			mm_destination = mm_cursor;
			while (mm_destination < mm_last) {
				++mm_num;
				++mm_destination;
			}

			mm_destination = mm_cursor + 1;
			(void)memmove(mm_cursor, mm_destination,
				      sizeof(mmap_region_t) * mm_num);

			/*
			 * Check if the mapping function actually managed to map
			 * anything. If not, just return now.
			 */
			if (mm_cursor->base_va >= end_va) {
				return -ENOMEM;
			}

			/*
			 * Something went wrong after mapping some table entries,
			 * undo every change done up to this point.
			 */
			mmap_region_t unmap_mm = {
					.base_pa = 0,
					.base_va = mm->base_va,
					.size = end_va - mm->base_va,
					.attr = 0
			};
			xlat_tables_unmap_region(ctx, &unmap_mm, 0, ctx->base_table,
							ctx->base_table_entries, ctx->base_level);

			return -ENOMEM;
		}

		/*
		 * Make sure that all entries are written to the memory. There
		 * is no need to invalidate entries when mapping dynamic regions
		 * because new table/block/page descriptors only replace old
		 * invalid descriptors, that aren't TLB cached.
		 */
		dsbishst();
	}

	if (end_pa > ctx->max_pa) {
		ctx->max_pa = end_pa;
	}
	if (end_va > ctx->max_va) {
		ctx->max_va = end_va;
	}

	return 0;
}

/*
 * Removes the region with given base Virtual Address and size from the given
 * context.
 *
 * Returns:
 *        0: Success.
 *   EINVAL: Invalid values were used as arguments (region not found).
 *    EPERM: Tried to remove a static region.
 */
int32_t mmap_remove_dynamic_region_ctx(xlat_ctx_t *ctx, uintptr_t base_va,
				   size_t size)
{
	mmap_region_t *mm = ctx->mmap, *mm_source;
	uint32_t mm_num = ctx->mmap_num;
	int32_t update_max_va_needed = 0;
	int32_t update_max_pa_needed = 0;

	/* Check sanity of mmap array. */
	assert(mm[ctx->mmap_num].size == 0U);

	while ((mm->size != 0U)) {
		if ((mm->base_va == base_va) && (mm->size == size)) {
			break;
		}
		++mm;
		--mm_num;
	}

	/* Check that the region was found */
	if (mm->size == 0U) {
		return -EINVAL;
	}

	/* If the region is static it can't be removed */
	if ((mm->attr & MT_DYNAMIC) == 0U) {
		return -EPERM;
	}

	/* Check if this region is using the top VAs or PAs. */
	if ((mm->base_va + mm->size - 1U) == ctx->max_va) {
		update_max_va_needed = 1;
	}
	if ((mm->base_pa + mm->size - 1U) == ctx->max_pa) {
		update_max_pa_needed = 1;
	}

	/* Update the translation tables if needed */
	if (ctx->initialized != 0U) {
		xlat_tables_unmap_region(ctx, mm, 0, ctx->base_table,
					 ctx->base_table_entries,
					 ctx->base_level);
		xlat_arch_tlbi_va_sync();
	}

	/* Remove this region by moving the rest down by one place. */
	mm_source = mm + 1;
	(void)memmove(mm, mm_source, sizeof(mmap_region_t) * mm_num);

	/* Check if we need to update the max VAs and PAs */
	if (update_max_va_needed != 0) {
		ctx->max_va = 0;
		mm = ctx->mmap;
		while (mm->size != 0U) {
			if ((mm->base_va + mm->size - 1U) > ctx->max_va) {
				ctx->max_va = mm->base_va + mm->size - 1U;
			}
			++mm;
		}
	}

	if (update_max_pa_needed != 0) {
		ctx->max_pa = 0;
		mm = ctx->mmap;
		while (mm->size != 0U) {
			if ((mm->base_pa + mm->size - 1U) > ctx->max_pa) {
				ctx->max_pa = mm->base_pa + mm->size - 1U;
			}
			++mm;
		}
	}

	return 0;
}

#endif /* PLAT_XLAT_TABLES_DYNAMIC */

#if LOG_LEVEL >= LOG_LEVEL_VERBOSE

/* Print the attributes of the specified block descriptor. */
static void xlat_desc_print(uint64_t desc, uint64_t execute_never_mask)
{
	int32_t mem_type_index = ATTR_INDEX_GET(desc);

	if (mem_type_index == ATTR_IWBWA_OWBWA_NTR_INDEX) {
		tf_printf("MEM");
	} else if (mem_type_index == ATTR_NON_CACHEABLE_INDEX) {
		tf_printf("NC");
	} else {
		assert(mem_type_index == ATTR_DEVICE_INDEX);
		tf_printf("DEV");
	}

	tf_printf(LOWER_ATTRS(AP_RO) & desc ? "-RO" : "-RW");
	tf_printf(LOWER_ATTRS(NS) & desc ? "-NS" : "-S");
	tf_printf(execute_never_mask & desc ? "-XN" : "-EXEC");
}

static const char * const level_spacers[] = {
	"[LV0] ",
	"  [LV1] ",
	"    [LV2] ",
	"      [LV3] "
};

static const char *invalid_descriptors_ommited =
		"%s(%d invalid descriptors omitted)\n";

/*
 * Recursive function that reads the translation tables passed as an argument
 * and prints their status.
 */
static void xlat_tables_print_internal(const uintptr_t table_base_va,
		uint64_t *const table_base, const int32_t table_entries,
		const uint32_t level, const uint64_t execute_never_mask)
{
	assert(level <= XLAT_TABLE_LEVEL_MAX);

	uint64_t desc;
	uintptr_t table_idx_va = table_base_va;
	int32_t table_idx = 0;

	size_t level_size = XLAT_BLOCK_SIZE(level);

	/*
	 * Keep track of how many invalid descriptors are counted in a row.
	 * Whenever multiple invalid descriptors are found, only the first one
	 * is printed, and a line is added to inform about how many descriptors
	 * have been omitted.
	 */
	int32_t invalid_row_count = 0;

	while (table_idx < table_entries) {

		desc = table_base[table_idx];

		if ((desc & DESC_MASK) == INVALID_DESC) {

			if (invalid_row_count == 0) {
				tf_printf("%sVA:%p size:0x%zx\n",
					  level_spacers[level],
					  (void *)table_idx_va, level_size);
			}
			invalid_row_count++;

		} else {

			if (invalid_row_count > 1) {
				tf_printf(invalid_descriptors_ommited,
					  level_spacers[level],
					  invalid_row_count - 1);
			}
			invalid_row_count = 0;

			/*
			 * Check if this is a table or a block. Tables are only
			 * allowed in levels other than 3, but DESC_PAGE has the
			 * same value as DESC_TABLE, so we need to check.
			 */
			if (((desc & DESC_MASK) == TABLE_DESC) &&
					(level < XLAT_TABLE_LEVEL_MAX)) {
				/*
				 * Do not print any PA for a table descriptor,
				 * as it doesn't directly map physical memory
				 * but instead points to the next translation
				 * table in the translation table walk.
				 */
				tf_printf("%sVA:%p size:0x%zx\n",
					  level_spacers[level],
					  (void *)table_idx_va, level_size);

				uintptr_t addr_inner = desc & TABLE_ADDR_MASK;

				xlat_tables_print_internal(table_idx_va,
					(uint64_t *)addr_inner,
					XLAT_TABLE_ENTRIES, level+1,
					execute_never_mask);
			} else {
				tf_printf("%sVA:%p PA:0x%lx size:0x%zx ", // CHANGED: original-> PA:0x%llx
					  level_spacers[level],
					  (void *)table_idx_va,
					  (uint64_t)(desc & TABLE_ADDR_MASK),
					  level_size);
				xlat_desc_print(desc, execute_never_mask);
				tf_printf("\n");
			}
		}

		table_idx++;
		table_idx_va += level_size;
	}

	if (invalid_row_count > 1) {
		tf_printf(invalid_descriptors_ommited,
			  level_spacers[level], invalid_row_count - 1);
	}
}

#endif /* LOG_LEVEL >= LOG_LEVEL_VERBOSE */

void xlat_tables_print(xlat_ctx_t *ctx)
{
#if LOG_LEVEL >= LOG_LEVEL_VERBOSE
	xlat_tables_print_internal(0, ctx->base_table, ctx->base_table_entries,
				   ctx->base_level, ctx->execute_never_mask);
#endif /* LOG_LEVEL >= LOG_LEVEL_VERBOSE */
}

void init_xlation_table(xlat_ctx_t *ctx)
{
	mmap_region_t *mm = ctx->mmap;

	/* All tables must be zeroed before mapping any region. */

	for (uint32_t i = 0; i < ctx->base_table_entries; i++) {
		ctx->base_table[i] = INVALID_DESC;
	}

	for (uint32_t j = 0; j < ctx->tables_num; j++) {
#if PLAT_XLAT_TABLES_DYNAMIC
		ctx->tables_mapped_regions[j] = 0;
#endif
		for (uint32_t i = 0; i < XLAT_TABLE_ENTRIES; i++) {
			ctx->tables[j][i] = INVALID_DESC;
		}
	}

	while (mm->size != 0U) {
		uintptr_t end_va = xlat_tables_map_region(ctx, mm, 0, ctx->base_table,
				ctx->base_table_entries, ctx->base_level);

		if (end_va != (mm->base_va + mm->size - 1U)) {
			ERROR("Not enough memory to map region:\n"
			      " VA:0x%lx  PA:0x%lx  size:0x%zx  attr:0x%x\n",
			      mm->base_va, mm->base_pa, mm->size, mm->attr);
			panic();
		}

		mm++;
	}

	ctx->initialized = 1;
}

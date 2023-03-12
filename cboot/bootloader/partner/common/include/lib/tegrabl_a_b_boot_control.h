/*
 * Copyright (c) 2016-2021, NVIDIA Corporation.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_BOOTCTRL_H
#define INCLUDED_TEGRABL_BOOTCTRL_H

#include <stdint.h>

#define OFFSET_SLOT_METADATA 0U

#define BOOT_CHAIN_MAGIC 0x43424E00U /*magic number: '\0NBC' */
#define BOOT_CHAIN_SUFFIX_A "_a"
#define BOOT_CHAIN_SUFFIX_B "_b"
#define BOOT_CHAIN_SUFFIX_LEN 2U

/* SMD version history */
	/* initial SMD */
#define BOOT_CHAIN_VERSION_ONE 1U
	/* add crc32 to SMD */
#define	BOOT_CHAIN_VERSION_CRC32 2U
	/* add redundancy boot flag to SMD */
#define	BOOT_CHAIN_VERSION_REDUNDANCY 3U
	/* add rootfs AB info, max BL retry count to SMD */
#define BOOT_CHAIN_VERSION_ROOTFS_AB 4U
	/* add rootfs AB max retry count, unify RF/BL AB */
#define BOOT_CHAIN_VERSION_UNIFY_RF_BL_AB 5U
	/* current version */
#define	BOOT_CHAIN_VERSION BOOT_CHAIN_VERSION_UNIFY_RF_BL_AB

#define BOOT_CHAIN_VERSION_MASK 0xFFU
#define BOOT_CHAIN_VERSION_GET(version) \
			((version) & BOOT_CHAIN_VERSION_MASK)

/* SMD enhancement features (upper 8 bits of smd version field) */
#define BOOTCTRL_FEATURE_MASK (0xFFUL << 8)
#define BOOTCTRL_REDUNDANCY_ENABLE (0x01UL << 8)
#define BOOTCTRL_REDUNDANCY_USER (0x01UL << 9)
#define BOOTCTRL_ROOTFS_AB (0x01UL << 10)
#define BOOTCTRL_UNIFY_RF_BL_AB (0x01UL << 13)

#define BOOTCTRL_SUPPORT_REDUNDANCY(version) \
			((version) & BOOTCTRL_REDUNDANCY_ENABLE)
#define BOOTCTRL_SUPPORT_REDUNDANCY_USER(version) \
			((version) & BOOTCTRL_REDUNDANCY_USER)
#define BOOTCTRL_SUPPORT_ROOTFS_AB(version) \
			((version) & BOOTCTRL_ROOTFS_AB)
#define BOOTCTRL_SUPPORT_UNIFIED_AB(version) \
			((version) &  BOOTCTRL_UNIFY_RF_BL_AB)
#define BOOTCTRL_FEATURE_UNIFIED_AB(version) \
			(BOOT_CHAIN_VERSION_GET(version) >= \
			 BOOT_CHAIN_VERSION_UNIFY_RF_BL_AB)
#define BOOTCTRL_IS_UNIFIED_AB_ENABLED(version) \
			((BOOTCTRL_SUPPORT_UNIFIED_AB(version)) && \
			 (BOOTCTRL_SUPPORT_ROOTFS_AB(version)) && \
			 (BOOTCTRL_FEATURE_UNIFIED_AB(version)))

#define SLOT_RETRY_COUNT_DEFAULT 7U
#define SLOT_PRIORITY_DEFAULT 15U

typedef uint32_t boot_slot_id_t;
#define BOOT_SLOT_A 0U
#define BOOT_SLOT_B 1U
#define MAX_SLOTS 2U

#define BOOTCTRL_EXTENSION_MAGIC 0x43424E45 /*magic number: 'ENBC' */

/* Extension SMD version history */
	/* initial extension SMD, add rootfs AB and log history */
#define BOOT_CHAIN_EXT_VERSION_ONE 1U
	/* current extenstion version */
#define BOOT_CHAIN_EXT_VERSION BOOT_CHAIN_EXT_VERSION_ONE

#define ROOTFS_A 0U
#define ROOTFS_B 1U
#define ROOTFS_INVALID 3U
#define ROOTFS_RETRY_COUNT_DEFAULT 3U
#define ROOTFS_AB_ROTATE_COUNT 4U
#define MAX_LOGS 16U

#define ROOTFS_RETRY_COUNT_SHIFT              0
#define ROOTFS_RETRY_COUNT_MASK               (3 << ROOTFS_RETRY_COUNT_SHIFT)
#define ROOTFS_CURRENT_SHIFT                  2
#define ROOTFS_CURRENT_MASK                   (3 << ROOTFS_CURRENT_SHIFT)

#define ROOTFS_MAX_RETRY_COUNT_SHIFT          0
#define ROOTFS_MAX_RETRY_COUNT_MASK           (3 << ROOTFS_MAX_RETRY_COUNT_SHIFT)

#define ROOTFS_STATUS_NORMAL                  0
#define ROOTFS_STATUS_IN_PROGRESS             1
#define ROOTFS_STATUS_DONE                    2
#define ROOTFS_STATUS_BOOT_FAILED             3
#define ROOTFS_STATUS_END                     ROOTFS_STATUS_BOOT_FAILED

#define ROOTFS_UPDATE_MODE_NORMAL             0
#define ROOTFS_UPDATE_MODE_IN_OTA             1
#define ROOTFS_UPDATE_MODE_IN_SYNC            2
#define ROOTFS_UPDATE_MODE_END                ROOTFS_UPDATE_MODE_IN_SYNC

#define GET_ROOTFS_ACTIVE(rf_sel) \
	(((rf_sel) & ROOTFS_CURRENT_MASK) >> ROOTFS_CURRENT_SHIFT)
#define SET_ROOTFS_ACTIVE(slot, rf_sel) \
	(((rf_sel) & ~ROOTFS_CURRENT_MASK) | \
	 ((slot) << ROOTFS_CURRENT_SHIFT))
#define GET_ROOTFS_RETRY_COUNT(rf_sel) \
	(((rf_sel) & ROOTFS_RETRY_COUNT_MASK) >> ROOTFS_RETRY_COUNT_SHIFT)
#define SET_ROOTFS_RETRY_COUNT(cnt, rf_sel) \
	(((rf_sel) & ~ROOTFS_RETRY_COUNT_MASK) | \
	 ((cnt) << ROOTFS_RETRY_COUNT_SHIFT))

#define SET_MAX_ROOTFS_RETRY_COUNT(cnt, rf_misc) \
	(((rf_misc) & ~ROOTFS_MAX_RETRY_COUNT_MASK) | \
	 ((cnt) << ROOTFS_MAX_RETRY_COUNT_SHIFT))
#define GET_MAX_ROOTFS_RETRY_COUNT(rf_misc) \
	(((rf_misc) & ROOTFS_MAX_RETRY_COUNT_MASK) >> ROOTFS_MAX_RETRY_COUNT_SHIFT)

#define ROOTFS_SELECT(smd_v2) \
	((smd_v2)->smd_ext.features.rootfs_select)
#define ROOTFS_STATUS(smd_v2, rfs_id) \
	((smd_v2)->smd_ext.features.rootfs_status[rfs_id])

/*
 * BOOT_CHAIN Scratch register
 *
 * 00:15 magic 'CAFE'
 * 16:18 slot number
 * 19:21 max slots
 * 22:25 slot B retry count
 * 26:29 slot A retry count
 * 30:31 OTA update flag
 */
#define BOOT_CHAIN_REG_MAGIC_MASK 0x0000FFFFU
#define BOOT_CHAIN_REG_MAGIC 0xCAFEUL	/* 'CAFE' */
#define BOOT_CHAIN_REG_MAGIC_SET(reg)	\
			(((reg) & ~BOOT_CHAIN_REG_MAGIC_MASK) | BOOT_CHAIN_REG_MAGIC)
#define BOOT_CHAIN_REG_MAGIC_GET(reg) ((reg) & BOOT_CHAIN_REG_MAGIC_MASK)
#define BOOT_CHAIN_REG_SLOT_NUM_MASK (0x07UL << 16)
#define BOOT_CHAIN_REG_SLOT_NUM_SET(slot, reg) \
			(((reg) & ~BOOT_CHAIN_REG_SLOT_NUM_MASK) | (((slot) & 0x07UL) << 16))
#define BOOT_CHAIN_REG_SLOT_NUM_GET(reg)	(((reg) >> 16) & 0x07U)

#define BOOT_CHAIN_REG_MAX_SLOTS_MASK (0x07UL << 19)
#define BOOT_CHAIN_REG_MAX_SLOTS_SET(max_slots, reg) \
		(((reg) & ~BOOT_CHAIN_REG_MAX_SLOTS_MASK) | (((max_slots) & 0x07UL) << 19))
#define BOOT_CHAIN_REG_MAX_SLOTS_GET(reg)	(((reg) >> 19) & 0x07U)

#define BOOT_CHAIN_REG_B_RETRY_COUNT_MASK (0x0FUL << 22)
#define BOOT_CHAIN_REG_B_RETRY_COUNT_SET(count, reg) \
	(((reg) & ~BOOT_CHAIN_REG_B_RETRY_COUNT_MASK) | (((count) & 0x0FUL) << 22))
#define BOOT_CHAIN_REG_B_RETRY_COUNT_GET(reg)	(((reg) >> 22) & 0x0FUL)

#define BOOT_CHAIN_REG_A_RETRY_COUNT_MASK (0x0FUL << 26)
#define BOOT_CHAIN_REG_A_RETRY_COUNT_SET(count, reg) \
	(((reg) & ~BOOT_CHAIN_REG_A_RETRY_COUNT_MASK) | (((count) & 0x0FUL) << 26))
#define BOOT_CHAIN_REG_A_RETRY_COUNT_GET(reg)	(((reg) >> 26) & 0x0FUL)

#define BOOT_CHAIN_REG_UPDATE_FLAG_MASK (0x03UL << 30)
#define BOOT_CHAIN_REG_UPDATE_FLAG_SET(flag, reg) \
	(((reg) & ~BOOT_CHAIN_REG_UPDATE_FLAG_MASK) | (((flag) & 0x03UL) << 30))
#define BOOT_CHAIN_REG_UPDATE_FLAG_GET(reg)	(((reg) >> 30) & 0x03UL)

#define BC_FLAG_OTA_OFF			0x00U
#define BC_FLAG_REDUNDANCY_BOOT	0x01U
#define BC_FLAG_RESERVED_2		0x02U
#define BC_FLAG_OTA_ON			0x03U

#define FROM_SMD_TO_REG 0U
#define FROM_REG_TO_SMD 1U

/*
 * ROOTFS A/B Scratch Register
 * 00:15 magic 'FACE'
 * 16:19 rotate count
 * 20:21 rootfs A/B retry count
 * 22:31 reserved
 */
#define ROOTFS_AB_REG_MAGIC_MASK 0x0000FFFFU
#define ROOTFS_AB_REG_MAGIC 0xFACEUL	/* 'FACE' */

#define ROOTFS_AB_REG_MAGIC_SET(reg) \
	(((reg) & ~ROOTFS_AB_REG_MAGIC_MASK) | ROOTFS_AB_REG_MAGIC)
#define ROOTFS_AB_REG_MAGIC_GET(reg) ((reg) & ROOTFS_AB_REG_MAGIC_MASK)

#define ROOTFS_AB_REG_ROTATE_SHIFT 16
#define ROOTFS_AB_REG_ROTATE_MASK (0x0FUL << ROOTFS_AB_REG_ROTATE_SHIFT)

#define ROOTFS_AB_REG_ROTATE_COUNT_SET(count, reg) \
	(((reg) & ~ROOTFS_AB_REG_ROTATE_MASK) | \
	 (((count) & 0x0FUL) << ROOTFS_AB_REG_ROTATE_SHIFT))
#define ROOTFS_AB_REG_ROTATE_COUNT_GET(reg) \
	(((reg) & ROOTFS_AB_REG_ROTATE_MASK) >> ROOTFS_AB_REG_ROTATE_SHIFT)

#define ROOTFS_AB_REG_RETRY_SHIFT 20
#define ROOTFS_AB_REG_RETRY_MASK (0x03UL << ROOTFS_AB_REG_RETRY_SHIFT)

#define ROOTFS_AB_REG_RETRY_COUNT_SET(count, reg) \
	(((reg) & ~ROOTFS_AB_REG_RETRY_MASK) | \
	(((count) & 0x03UL) << ROOTFS_AB_REG_RETRY_SHIFT))
#define ROOTFS_AB_REG_RETRY_COUNT_GET(reg) \
	(((reg) & ROOTFS_AB_REG_RETRY_MASK) >> ROOTFS_AB_REG_RETRY_SHIFT)


TEGRABL_PACKED(
struct boot_slot_info {
	/*
	 * boot priority of slot.
	 * range [0:15]
	 * 15 meaning highest priortiy,
	 * 0 mean that the slot is unbootable.
	 */
	uint8_t priority;

	/*
	 * suffix of partition
	 * value of either _a or _b
	 */
	char suffix[BOOT_CHAIN_SUFFIX_LEN];

	/*
	 * retry count of booting
	 * range [0:7]
	 */
	uint8_t retry_count;

	/* set true if slot can boot successfully */
	uint8_t boot_successful;

}
);

/**
 * @brief Slot Meta Data (SMD) structure
 *
 * @param magic - the majic number for idetification
 * @param version - version number of data structure
 * @param num_slots - the number of slot info present in this data structure
 * @param slot_info[] - details of all slot info
 */
TEGRABL_PACKED(
struct slot_meta_data {
	uint32_t magic;
	uint16_t version;
	uint16_t num_slots;
	struct boot_slot_info slot_info[MAX_SLOTS];
	uint32_t crc32;
}
);

TEGRABL_PACKED(
struct slot_meta_data_ext {
	/* crc32 calculation starts from crc32_len */
	uint32_t crc32;
	uint32_t crc32_len;

	uint32_t magic;
	uint32_t version;

	struct features_ext_t {
		/*
		 * range [0:1]: rootfs A/B retry count:
		 * 11: Rootfs_First_Try
		 * 10: Rootfs_Second_Try
		 * 01: Rootfs_Third_Try
		 * 00: Rootfs_No_More_Try
		 *
		 * range [2:3]: current rootfs:
		 * 0: A
		 * 1: B
		 * 10: Reserved
		 * 11: Invalid
		 *
		 * range [4:7]: RESERVED
		 */
		uint8_t rootfs_select;

		/*
		 * rootfs status:
		 * 00: Normal (Boot Success)
		 * 01: UPDATE_IN_PROGRESS
		 * 10: UPDATE_DONE
		 * 11: BOOT_FAILED
		 */
		uint8_t rootfs_status[MAX_SLOTS];

		/*
		 *
		 * rootfs update mode:
		 * 00: Normal (No OTA/SYNC)
		 * 01: UPDATE_IN_OTA
		 * 10: UPDATE_IN_SYNC
		 * 11: RESERVED
		 *
		 * range [4:7]: RESERVED
		 */
		uint8_t rootfs_update_mode[MAX_SLOTS];

		/*
		 * rootfs misc info:
		 * range [0:1]: max rootfs A/B retry count:
		 * 00: Invalid
		 * 01: 1
		 * 10: 2
		 * 11: 3
		 *
		 * range [2:7]: RESERVED
		 */
		uint8_t rootfs_misc_info;

		uint8_t reserved[2];
	} features;

	uint8_t max_bl_retry_count;

	struct smd_log_t {
		uint32_t version;
		uint32_t first_entry;
		uint32_t last_entry;
		uint32_t reserved;
		struct entries {
			uint32_t event;
			uint32_t time_stamp;
		} all_entries[MAX_LOGS];
	} smd_log;
}
);

TEGRABL_PACKED(
struct slot_meta_data_v2 {
	struct slot_meta_data smd;
	struct slot_meta_data_ext smd_ext;
}
);

/**
 * @brief Get smd struct version
 *
 * @param smd the memory address where smd is located
 *
 * @return smd structure version
 */
uint16_t tegrabl_a_b_get_version(void *smd);

/**
 * @brief Set default boot slot info
 *
 * @param smd the memory address where smd is located
 *
 * @return none
 */
void tegrabl_a_b_init(void *smd);

/**
 * @brief Get the active boot slot number
 *
 * @param smd the memory address of smd buffer. If NULL, active_slot is
 *			retrieved from scrach register
 * @param active_slot the memory address where selected boot slot is returned
 *
 * @return TEGRABL_NO_ERROR if active slot is found, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_get_active_slot(void *smd, uint32_t *active_slot);

/**
 * @brief Save retry_count to scratch register
 *
 * @param slot slot to be set with retry count
 * @param retry_count retry count to be set to reg
 *
 */
void tegrabl_a_b_set_retry_count_reg(uint32_t slot, uint8_t retry_count);

/**
 * @brief Save rootfs retry_count to smd buffer
 *
 * @param smd the memory address of smd buffer.
 * @param rootfs_retry_count rootfs retry count to be set to smd
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_set_rootfs_retry_count(void *smd,
						    uint8_t rootfs_retry_count);
/**
 * @brief Copy retry count between SMD buffer and SR
 *
 * @param smd smd load address
 * @param reg reg value address
 * @param direct copy direction, FROM_SMD_TO_REG or FROM_REG_TO_SMD
 *
 */
void tegrabl_a_b_copy_retry_count(void *smd, uint32_t *reg, uint32_t direct);

/**
 * @brief Save the active boot slot info to scratch register
 *
 * @param smd smd load address
 * @param slot active slot
 *
 */
void tegrabl_a_b_save_boot_slot_reg(void *smd, uint32_t slot);

/**
 * @brief Init boot slot reg if invalid
 *
 * @param smd smd load address
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_init_boot_slot_reg(void *smd);

/**
 * @brief Init rootfs slot reg if invalid
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_init_rootfs_slot_reg(void *smd);

/**
 * @brief Update retry count if successful state is not true
 *
 * @param smd the memory address of smd buffer.
 * @param slot active slot number
 *
 * @return TEGRABL_NO_ERROR if successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_check_and_update_retry_count(void *smd,
														 uint32_t slot);

/**
 * @brief Get the boot slot suffix
 *
 * @param suffix the memory address where suffix string is returned
 * @param full_suffix Flag if we need full_suffix to be returned or "" for the
 *        slot 0
 *
 * @return TEGRABL_NO_ERROR if getting bootslot successful, otherwise
 * an appropriate error value.
 */
tegrabl_error_t tegrabl_a_b_get_bootslot_suffix(char *suffix, bool full_suffix);

/**
 * @brief Set the boot slot partition name suffix
 *  Note: To be compatible with legacy partition name, this function will only
 *        set suffix for slot 1 (or above if any)
 *
 * @param slot the indicated boot slot number
 * @param partition the memory address of given partition name
 * @param full_suffix Flag if we need full_suffix to be returned or "" for the
 *        slot 0
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_set_bootslot_suffix(uint32_t slot, char *partition,
												bool full_suffix);

/**
 * @brief Get the boot slot id with given partition name suffix
 *  Note: To be compatible with legacy partition name, this function will take
 *        "" for slot 0
 *
 * @param suffix the memory address of given suffix
 * @param slot the indicated boot slot number (output param)
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_get_slot_via_suffix(const char *suffix,
												uint32_t *slot);

/**
 * @brief Get the num of slots
 *
 * @param smd_addr the memory address of smd buffer.
 * @param num_slots slot num (output param)
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_get_slot_num(void *smd_addr, uint8_t *num_slots);

/**
 * @brief Get the successful flag of given slot
 *
 * @param suffix the memory address of given suffix
 * @param slot slot index
 * @param successful successful flag of the slot (output param)
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_get_successful(void *smd, uint32_t slot,
										   uint8_t *successful);

/**
 * @brief Get the priority of given slot
 *
 * @param smd the memory address of smd buffer.
 * @param slot slot index
 * @param priority priority of the slot (output param)
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_get_priority(void *smd, uint32_t slot,
										 uint8_t *priority);

/**
 * @brief Get the retry count of given slot
 *
 * @param smd the memory address of smd buffer.
 * @param slot slot index
 * @param retry_count retry_count of the slot (output param)
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_get_retry_count(void *smd, uint32_t slot,
											uint8_t *retry_count);

/**
 * @brief Check if the given slot is unbootable
 *
 * @param smd the memory address of smd buffer.
 * @param slot slot index
 * @param is_unbootable unbootable flag of the slot (output param)
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_is_unbootable(void *smd, uint32_t slot,
										  bool *is_unbootable);

/**
 * @brief Set the successful flag of given slot
 *
 * @param smd the memory address of smd buffer.
 * @param slot slot index
 * @param successful successful flag of the slot
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_set_successful(void *smd, uint32_t slot,
										   uint8_t successful);

/**
 * @brief Set the priority of given slot
 *
 * @param smd the memory address of smd buffer.
 * @param slot slot index
 * @param priority priority of the slot
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_set_priority(void *smd, uint32_t slot,
										 uint8_t priority);

/**
 * @brief Set the retry count of given slot
 *
 * @param smd the memory address of smd buffer.
 * @param slot slot index
 * @param retry_count retry_count of the slot
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_set_retry_count(void *smd, uint32_t slot,
											uint8_t retry_count);

/**
 * @brief Active the given slot in smd
 *
 * @param smd_addr the memory address of smd buffer.
 * @param slot slot index
 *
 * @return TEGRABL_NO_ERROR if setting was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_set_active_slot(void *smd_addr, uint32_t slot_id);

/**
 * @brief Get the current rootfs id in smd
 *
 * @param smd_addr the memory address of smd buffer.
 * @param rootfs_id the current rootfs id 0 or 1, if no bootable rootfs,
 *                  it's 0xff. (output param)
 *
 * @return TEGRABL_NO_ERROR if getting was successful, otherwise an appropriate
 *		   error value, mean no rootfs AB enabled.
 */
tegrabl_error_t
tegrabl_a_b_get_current_rootfs_id(void *smd, uint8_t *rootfs_id);

/**
 * @brief Check if rootfs AB are both un-bootable
 *
 * @return true if both rootfs are invalid, otherwise false
 */
bool tegrabl_a_b_rootfs_is_all_unbootable(void *smd);

/**
 * @brief Get the active rootfs suffix
 *
 * @param suffix the memory address where suffix string is returned.
 *		 "_a" or "" for rootfs A, "_b" for rootfs B.
 * @param full_suffix Flag if we need full_suffix to be returned or "" for the
 *        slot 0
 *
 * @return TEGRABL_NO_ERROR if getting was successful, otherwise an appropriate
 *                 error value.
 */
tegrabl_error_t tegrabl_a_b_get_rootfs_suffix(char *suffix, bool full_suffix);

/**
 * @brief Get the smd address, load it from storage if not
 *
 * @param smd smd load address (output param)
 *
 * @return TEGRABL_NO_ERROR on success. Otherwise, return appropriate error code
 */
tegrabl_error_t tegrabl_a_b_get_smd(void **smd);

/**
 * @brief Write smd to storage device
 *
 * @param smd the memory address of smd buffer.
 *
 * @return TEGRABL_NO_ERROR if writing was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_flush_smd(void *smd);

/**
 * @brief update smd in storage device from scratch register
 *
 * @return TEGRABL_NO_ERROR if updating was successful, otherwise an appropriate
 *		   error value.
 */
tegrabl_error_t tegrabl_a_b_update_smd(void);

#endif /* INCLUDED_TEGRABL_BOOTCTRL_H */

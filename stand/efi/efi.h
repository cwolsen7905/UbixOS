/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Minimal UEFI definitions, written from the UEFI specification (chapters 4, 7,
 * 12, 13, 9) for the uBix loader.  Only what stand/efi uses.  Machine
 * independent: on x86_64 the compiler's Windows target gives the required
 * calling convention; on AArch64 the one AAPCS64 convention applies.
 */

#ifndef _STAND_EFI_H
#define _STAND_EFI_H

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef uint64_t uintn_t;
typedef uint64_t uintptr_t;
typedef uint64_t efi_status_t;
typedef void *efi_handle_t;
typedef uint16_t char16_t;

#define EFI_SUCCESS 0ULL
#define EFI_ERR(n) ((1ULL << 63) | (n))
#define EFI_BUFFER_TOO_SMALL EFI_ERR(5)
#define EFI_NOT_FOUND EFI_ERR(14)
#define EFI_ERROR(s) ((s) >> 63)

typedef struct
{
	uint32_t d1;
	uint16_t d2, d3;
	uint8_t d4[8];
} efi_guid_t;

typedef struct
{
	uint64_t signature;
	uint32_t revision, header_size, crc32, reserved;
} efi_table_header_t;

enum
{
	EFI_ALLOCATE_ANY_PAGES = 0,
	EFI_ALLOCATE_MAX_ADDRESS = 1,
	EFI_ALLOCATE_ADDRESS = 2
};
enum
{
	EFI_LOADER_CODE = 1,
	EFI_LOADER_DATA = 2,
	EFI_CONVENTIONAL = 7
};

typedef struct
{
	uint32_t type, pad;
	uint64_t physical_start, virtual_start, number_of_pages, attribute;
} efi_memory_descriptor_t;

typedef struct efi_simple_text_output
{
	void *reset;
	efi_status_t (*output_string)(struct efi_simple_text_output *, const char16_t *);
} efi_simple_text_output_t;

typedef struct
{
	efi_table_header_t hdr;
	void *raise_tpl, *restore_tpl;
	efi_status_t (*allocate_pages)(uint32_t type, uint32_t mem_type, uintn_t pages, uint64_t *addr);
	efi_status_t (*free_pages)(uint64_t addr, uintn_t pages);
	efi_status_t (*get_memory_map)(
	    uintn_t *size, efi_memory_descriptor_t *map, uintn_t *key, uintn_t *desc_size, uint32_t *desc_version);
	efi_status_t (*allocate_pool)(uint32_t mem_type, uintn_t size, void **buf);
	efi_status_t (*free_pool)(void *buf);
	void *create_event, *set_timer, *wait_for_event, *signal_event, *close_event, *check_event;
	void *install_protocol_interface, *reinstall_protocol_interface, *uninstall_protocol_interface;
	efi_status_t (*handle_protocol)(efi_handle_t, const efi_guid_t *, void **);
	void *reserved, *register_protocol_notify, *locate_handle, *locate_device_path, *install_configuration_table;
	void *load_image, *start_image, *exit, *unload_image;
	efi_status_t (*exit_boot_services)(efi_handle_t, uintn_t key);
	void *get_next_monotonic_count;
	efi_status_t (*stall)(uintn_t usec);
	efi_status_t (*set_watchdog_timer)(uintn_t timeout, uint64_t code, uintn_t size, const char16_t *data);
	void *connect_controller, *disconnect_controller, *open_protocol, *close_protocol, *open_protocol_information;
	void *protocols_per_handle, *locate_handle_buffer;
	efi_status_t (*locate_protocol)(const efi_guid_t *, void *registration, void **iface);
} efi_boot_services_t;

typedef struct
{
	efi_guid_t guid;
	void *table;
} efi_configuration_table_t;

typedef struct
{
	efi_table_header_t hdr;
	char16_t *firmware_vendor;
	uint32_t firmware_revision, pad;
	efi_handle_t console_in_handle;
	void *con_in;
	efi_handle_t console_out_handle;
	efi_simple_text_output_t *con_out;
	efi_handle_t std_err_handle;
	efi_simple_text_output_t *std_err;
	void *runtime_services;
	efi_boot_services_t *boot_services;
	uintn_t number_of_table_entries;
	efi_configuration_table_t *configuration_table;
} efi_system_table_t;

typedef struct
{
	uint32_t revision;
	efi_handle_t parent_handle;
	efi_system_table_t *system_table;
	efi_handle_t device_handle;
} efi_loaded_image_t; /* leading fields only */

typedef struct efi_file
{
	uint64_t revision;
	efi_status_t (*open)(struct efi_file *, struct efi_file **, const char16_t *name, uint64_t mode, uint64_t attr);
	efi_status_t (*close)(struct efi_file *);
	void *del;
	efi_status_t (*read)(struct efi_file *, uintn_t *size, void *buf);
	void *write, *get_position;
	efi_status_t (*set_position)(struct efi_file *, uint64_t pos);
	efi_status_t (*get_info)(struct efi_file *, const efi_guid_t *, uintn_t *size, void *buf);
} efi_file_t;

#define EFI_FILE_MODE_READ 1ULL

typedef struct
{
	uint64_t size, file_size, physical_size;
	uint8_t times[48];
	uint64_t attribute;
	char16_t file_name[1];
} efi_file_info_t;

typedef struct
{
	uint64_t revision;
	efi_status_t (*open_volume)(void *, efi_file_t **root);
} efi_simple_fs_t;

typedef struct
{
	uint32_t version, h_res, v_res, pixel_format;
	uint32_t masks[4];
	uint32_t pixels_per_scan_line;
} efi_gop_mode_info_t;

typedef struct
{
	uint32_t max_mode, mode;
	efi_gop_mode_info_t *info;
	uintn_t info_size;
	uint64_t fb_base, fb_size;
} efi_gop_mode_t;

typedef struct
{
	void *query_mode, *set_mode, *blt;
	efi_gop_mode_t *mode;
} efi_gop_t;

enum
{
	EFI_GOP_RGBX = 0,
	EFI_GOP_BGRX = 1,
	EFI_GOP_BITMASK = 2,
	EFI_GOP_BLT_ONLY = 3
};

#endif /* _STAND_EFI_H */

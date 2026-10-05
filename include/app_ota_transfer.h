#ifndef APP_OTA_TRANSFER_H
#define APP_OTA_TRANSFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OTA_PAGE_SIZE 4096U
#define OTA_IMAGE_CAPACITY 0x73000U
#define OTA_PREFIX_MAX 96U

struct ota_image_id {
  uint32_t version;
  uint32_t file_size;
  uint16_t manufacturer;
  uint16_t image_type;
};

/* Offsets are relative to the journal or secondary partition. Operations run
 * on the ZBOSS thread and writes obey NOR flash semantics. */
struct ota_storage {
  int (*read)(bool journal, uint32_t offset, void *data, size_t size);
  int (*write)(bool journal, uint32_t offset, const void *data, size_t size);
  int (*erase)(bool journal, uint32_t offset, size_t size);
};

/* Bind storage at startup without changing persisted progress, so a network
 * leave can discard an earlier checkpoint before a new download starts. */
int ota_transfer_init(const struct ota_storage *storage);
int ota_transfer_start(const struct ota_storage *storage, const struct ota_image_id *id);
int ota_transfer_receive(uint32_t offset, const uint8_t *data, size_t size);
uint32_t ota_transfer_offset(void);
int ota_transfer_check(void);
/* Discard on leave, invalid data, or installation; timeout/server abort keeps
 * committed pages. Bytes still in RAM are never resumed. */
int ota_transfer_discard(void);

#endif

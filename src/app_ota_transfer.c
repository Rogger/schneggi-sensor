#include "app_ota_transfer.h"
#include <errno.h>
#include <string.h>

#define JOURNAL_MAGIC 0x3152544fU /* OTR1: changing the format invalidates old data. */
#define COMMITTED 0x00000000U
#define ERASED 0xffffffffU

struct journal_header {
  uint32_t magic;
  struct ota_image_id id;
  uint32_t prefix_size;
  uint32_t image_size;
  uint8_t prefix[OTA_PREFIX_MAX];
  uint32_t crc;
  uint32_t commit;
};
struct checkpoint {
  uint32_t bytes;
  uint32_t data_crc;
  uint32_t crc;
  uint32_t commit;
};
_Static_assert(sizeof(struct journal_header) == 128, "Journal format changed");
_Static_assert(sizeof(struct checkpoint) == 16, "Checkpoint format changed");

static const struct ota_storage *store;
static struct journal_header header, saved;
_Alignas(4) static uint8_t page[OTA_PAGE_SIZE];
static uint32_t cursor, image_bytes, page_fill, data_crc, journal_next;
static uint32_t saved_bytes, saved_crc;
static bool active, prefix_ready, checked, resuming;

static uint16_t le16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return le16(p) | (uint32_t)le16(p + 2) << 16; }

/* Incremental IEEE CRC32, also used to reject torn journal writes. */
static uint32_t crc32(uint32_t crc, const void *data, size_t size)
{
  const uint8_t *p = data;
  crc = ~crc;
  while (size--) {
    crc ^= *p++;
    for (unsigned int bit = 0; bit < 8; bit++) {
      crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
  }
  return ~crc;
}

/* Accept exactly the canonical, single application package emitted by NCS
 * and validated by scripts/package_ota.py. Never write an unknown image ID. */
static int parse_prefix(void)
{
  const uint8_t *p = header.prefix;
  uint32_t n = cursor;
  if (n < 56) { return 0; }
  uint32_t ota_len = le16(p + 6);
  uint32_t fields = le16(p + 8);
  if (le32(p) != 0x0beef11e || le16(p + 4) != 0x100 ||
      !((ota_len == 56 && fields == 0) || (ota_len == 60 && fields == 4)) ||
      le16(p + 10) != header.id.manufacturer || le16(p + 12) != header.id.image_type ||
      le32(p + 14) != header.id.version || le32(p + 52) != header.id.file_size) {
    return -EINVAL;
  }
  if (n < ota_len + 8) { return 0; }
  if ((fields && (le16(p + 56) != 1 || le16(p + 58) != 1)) ||
      le16(p + ota_len) != 0 ||
      le32(p + ota_len + 2) != header.id.file_size - ota_len - 6) {
    return -EINVAL;
  }
  uint32_t cbor_len = le16(p + ota_len + 6);
  uint32_t prefix_len = ota_len + 8 + cbor_len;
  if (cbor_len < 17 || cbor_len > 21 || prefix_len > OTA_PREFIX_MAX ||
      prefix_len >= header.id.file_size) { return -EINVAL; }
  if (n < prefix_len) { return 0; }
  static const uint8_t fixed[] = {0xa1, 0x63, 'i', 'm', 'g', 0x81, 0xa2,
                                0x62, 'i', 'd', 0, 0x64, 's', 'i', 'z', 'e'};
  const uint8_t *cbor = p + ota_len + 8;
  if (memcmp(cbor, fixed, sizeof(fixed))) { return -EINVAL; }
  uint32_t size = cbor[16];
  uint32_t extra = size < 24 ? 0 : size == 24 ? 1 : size == 25 ? 2 : size == 26 ? 4 : 99;
  if (cbor_len != 17 + extra) { return -EINVAL; }
  if (extra) {
    size = 0;
    for (uint32_t i = 0; i < extra; i++) { size = (size << 8) | cbor[17 + i]; }
    if ((extra == 1 && size < 24) || (extra == 2 && size <= 255) ||
        (extra == 4 && size <= 65535)) { return -EINVAL; }
  }
  if (size < 32 || size > OTA_IMAGE_CAPACITY ||
      size != header.id.file_size - prefix_len) { return -EINVAL; }
  header.prefix_size = prefix_len;
  header.image_size = size;
  return 1;
}

static int committed_write(uint32_t offset, const void *record, size_t size)
{
  int err = store->write(true, offset, record, size - 4);
  if (!err) {
    const uint32_t commit = COMMITTED;
    err = store->write(true, offset + size - 4, &commit, 4);
  }
  return err;
}

static int load_progress(void)
{
  int err = store->read(true, 0, &saved, sizeof(saved));
  if (err) { return err; }
  if (saved.magic != JOURNAL_MAGIC || saved.commit != COMMITTED ||
      saved.crc != crc32(0, &saved, offsetof(struct journal_header, crc)) ||
      memcmp(&saved.id, &header.id, sizeof(header.id)) ||
      saved.prefix_size > OTA_PREFIX_MAX || saved.image_size > OTA_IMAGE_CAPACITY) {
    return 0;
  }
  struct checkpoint point;
  for (uint32_t pos = sizeof(saved); pos + sizeof(point) <= OTA_PAGE_SIZE;
       pos += sizeof(point)) {
    err = store->read(true, pos, &point, sizeof(point));
    if (err) { return err; }
    /* Include partially written records in the allocation cursor. */
    if (point.bytes != ERASED || point.data_crc != ERASED || point.crc != ERASED ||
        point.commit != ERASED) { journal_next = pos + sizeof(point); }
    if (point.commit == COMMITTED &&
        point.crc == crc32(0, &point, offsetof(struct checkpoint, crc)) &&
        point.bytes > saved_bytes && point.bytes % OTA_PAGE_SIZE == 0 &&
        point.bytes <= saved.image_size) {
      saved_bytes = point.bytes;
      saved_crc = point.data_crc;
    }
  }
  if (!saved_bytes) { return 0; }
  uint32_t crc = 0;
  for (uint32_t pos = 0; pos < saved_bytes; pos += OTA_PAGE_SIZE) {
    err = store->read(false, pos, page, sizeof(page));
    if (err) { return err; }
    crc = crc32(crc, page, sizeof(page));
  }
  if (crc != saved_crc) { saved_bytes = 0; }
  return 0;
}

int ota_transfer_start(const struct ota_storage *storage, const struct ota_image_id *id)
{
  active = false;
  if (!storage || !storage->read || !storage->write || !storage->erase || !id ||
      id->file_size < 94 || id->file_size > OTA_IMAGE_CAPACITY + OTA_PREFIX_MAX) {
    return -EINVAL;
  }
  store = storage;
  memset(&header, 0, sizeof(header));
  header.magic = JOURNAL_MAGIC;
  header.id = *id;
  cursor = image_bytes = page_fill = data_crc = saved_bytes = saved_crc = 0;
  journal_next = sizeof(header);
  prefix_ready = checked = resuming = false;
  int err = load_progress();
  if (!err) { active = true; }
  return err;
}

static int prepare_image(void)
{
  if (saved_bytes && saved.prefix_size == header.prefix_size &&
      saved.image_size == header.image_size &&
      !memcmp(saved.prefix, header.prefix, header.prefix_size)) {
    image_bytes = saved_bytes;
    data_crc = saved_crc;
    cursor = header.prefix_size + saved_bytes;
    resuming = true;
    return 0;
  }
  saved_bytes = 0;
  int err = store->erase(true, 0, OTA_PAGE_SIZE);
  /* Remove any stale MCUboot upgrade trailer before changing slot contents. */
  if (!err) { err = store->erase(false, 0x74000, OTA_PAGE_SIZE); }
  header.crc = crc32(0, &header, offsetof(struct journal_header, crc));
  header.commit = COMMITTED;
  if (!err) { err = committed_write(0, &header, sizeof(header)); }
  journal_next = sizeof(header);
  return err;
}

static int flush_page(bool checkpoint)
{
  uint32_t position = image_bytes - page_fill;
  int err = store->erase(false, position, OTA_PAGE_SIZE);
  if (!err) { err = store->write(false, position, page, sizeof(page)); }
  if (err || !checkpoint) { return err; }
  if (journal_next + sizeof(struct checkpoint) > OTA_PAGE_SIZE) { return -ENOSPC; }
  data_crc = crc32(data_crc, page, sizeof(page));
  struct checkpoint point = { .bytes = image_bytes, .data_crc = data_crc, .commit = COMMITTED };
  point.crc = crc32(0, &point, offsetof(struct checkpoint, crc));
  err = committed_write(journal_next, &point, sizeof(point));
  journal_next += sizeof(point);
  return err;
}

static int receive_data(uint32_t offset, const uint8_t *data, size_t size)
{
  if (!active || checked || !data || !size || offset != cursor ||
      size > header.id.file_size - cursor) { return -EINVAL; }
  while (size && !prefix_ready) {
    if (cursor == OTA_PREFIX_MAX) { return -EINVAL; }
    header.prefix[cursor++] = *data++;
    size--;
    int ret = parse_prefix();
    if (ret < 0) { return ret; }
    if (ret == 1) {
      prefix_ready = true;
      int err = prepare_image();
      if (err) { return err; }
      if (resuming) {
        /* Ignore this block's image bytes: the next request jumps to the
         * durable checkpoint, after revalidating every byte of the prefix. */
        return 0;
      }
    }
  }
  while (size) {
    uint32_t count = OTA_PAGE_SIZE - page_fill;
    if (count > size) { count = size; }
    memcpy(page + page_fill, data, count);
    page_fill += count;
    image_bytes += count;
    cursor += count;
    data += count;
    size -= count;
    if (image_bytes - page_fill == 0 && page_fill >= 32) {
      if (le32(page) != 0x96f3b83d ||
          ((uint32_t)page[20] << 24 | (uint32_t)page[21] << 16 | le16(page + 22)) !=
            header.id.version) { return -EINVAL; }
    }
    if (page_fill == OTA_PAGE_SIZE) {
      int err = flush_page(true);
      if (err) { return err; }
      page_fill = 0;
    }
  }
  return 0;
}

int ota_transfer_receive(uint32_t offset, const uint8_t *data, size_t size)
{
  int err = receive_data(offset, data, size);
  if (err) { active = false; }
  return err;
}

uint32_t ota_transfer_offset(void) { return cursor; }

int ota_transfer_check(void)
{
  if (!active || !prefix_ready || cursor != header.id.file_size ||
      image_bytes != header.image_size) { return -EINVAL; }
  if (checked) { return 0; }
  if (page_fill) {
    memset(page + page_fill, 0xff, OTA_PAGE_SIZE - page_fill);
    int err = flush_page(false);
    if (err) { active = false; return err; }
    page_fill = 0;
  }
  checked = true;
  return 0;
}

int ota_transfer_discard(void)
{
  active = checked = false;
  return store ? store->erase(true, 0, OTA_PAGE_SIZE) : 0;
}

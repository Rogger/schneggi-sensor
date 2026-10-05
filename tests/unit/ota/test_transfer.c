#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "app_ota_transfer.h"

static uint8_t journal[OTA_PAGE_SIZE], image[0x75000];
static uint8_t package[OTA_IMAGE_CAPACITY + OTA_PREFIX_MAX];
static struct ota_image_id id;
static unsigned int operations, fail_operation;
static uint32_t prefix_size;
static bool partial_failure;

static uint8_t *memory(bool metadata, uint32_t offset, size_t size)
{
  assert(offset + size <= (metadata ? sizeof(journal) : sizeof(image)));
  return (metadata ? journal : image) + offset;
}
static int read_data(bool metadata, uint32_t offset, void *data, size_t size)
{
  memcpy(data, memory(metadata, offset, size), size);
  return 0;
}
static int write_data(bool metadata, uint32_t offset, const void *data, size_t size)
{
  assert(offset % 4 == 0 && size % 4 == 0);
  bool fail = ++operations == fail_operation;
  size_t written = fail ? (partial_failure ? size / 2 : 0) : size;
  uint8_t *dest = memory(metadata, offset, size);
  const uint8_t *src = data;
  for (size_t i = 0; i < written; i++) {
    assert((dest[i] & src[i]) == src[i]); /* No rewriting bits from 0 to 1. */
    dest[i] &= src[i];
  }
  return fail ? -EIO : 0;
}
static int erase_data(bool metadata, uint32_t offset, size_t size)
{
  assert(offset % OTA_PAGE_SIZE == 0 && size % OTA_PAGE_SIZE == 0);
  bool fail = ++operations == fail_operation;
  size_t erased = fail ? (partial_failure ? size / 2 : 0) : size;
  memset(memory(metadata, offset, size), 0xff, erased);
  return fail ? -EIO : 0;
}
static const struct ota_storage storage = { read_data, write_data, erase_data };
static void put16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }

static void fixture(uint32_t size, bool optional)
{
  memset(journal, 0xff, sizeof(journal));
  memset(image, 0x00, sizeof(image)); /* Old application/trailer already in slot. */
  memset(package, 0, sizeof(package));
  operations = fail_operation = 0;
  id = (struct ota_image_id){ 0x02000004, 0, 0xfff1, 0x8102 };
  uint32_t ota_len = optional ? 60 : 56;
  put32(package, 0x0beef11e);
  put16(package + 4, 0x100);
  put16(package + 6, ota_len);
  put16(package + 8, optional ? 4 : 0);
  put16(package + 10, id.manufacturer);
  put16(package + 12, id.image_type);
  put32(package + 14, id.version);
  if (optional) { put16(package + 56, 1); put16(package + 58, 1); }
  static const uint8_t cbor[] = {0xa1, 0x63, 'i', 'm', 'g', 0x81, 0xa2,
                               0x62, 'i', 'd', 0, 0x64, 's', 'i', 'z', 'e'};
  uint8_t *dfu = package + ota_len + 6;
  memcpy(dfu + 2, cbor, sizeof(cbor));
  uint32_t extra = size <= 255 ? 1 : size <= 65535 ? 2 : 4;
  dfu[18] = extra == 1 ? 24 : extra == 2 ? 25 : 26;
  for (uint32_t i = 0; i < extra; i++) { dfu[19 + i] = size >> (8 * (extra - 1 - i)); }
  put16(dfu, 17 + extra);
  prefix_size = ota_len + 8 + 17 + extra;
  id.file_size = prefix_size + size;
  put32(package + 52, id.file_size);
  put32(package + ota_len + 2, size + 19 + extra);
  for (uint32_t i = 0; i < size; i++) { package[prefix_size + i] = (i * 17 + 3) % 251; }
  put32(package + prefix_size, 0x96f3b83d);
  package[prefix_size + 20] = 2;
  package[prefix_size + 21] = 0;
  put16(package + prefix_size + 22, 4);
}

static int feed_until(uint32_t end, uint32_t block)
{
  while (ota_transfer_offset() < end) {
    uint32_t offset = ota_transfer_offset();
    size_t size = end - offset;
    if (size > block) { size = block; }
    int err = ota_transfer_receive(offset, package + offset, size);
    if (err) { return err; }
  }
  return 0;
}
static uint32_t restart(void)
{
  fail_operation = 0;
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(feed_until(prefix_size, 1) == 0);
  return ota_transfer_offset();
}
static void finish(void)
{
  assert(feed_until(id.file_size, 64) == 0);
  assert(ota_transfer_check() == 0);
  assert(ota_transfer_check() == 0);
  assert(!memcmp(image, package + prefix_size, id.file_size - prefix_size));
  for (uint32_t i = 0x74000; i < sizeof(image); i++) { assert(image[i] == 0xff); }
}

int main(int argc, char **argv)
{
  if (argc == 2) {
    /* Optional integration check against a real generated Zigbee artifact. */
    FILE *file = fopen(argv[1], "rb");
    assert(file);
    size_t size = fread(package, 1, sizeof(package), file);
    assert(feof(file) && fclose(file) == 0);
    memset(journal, 0xff, sizeof(journal));
    memset(image, 0, sizeof(image));
    id = (struct ota_image_id){
      .version = package[14] | (uint32_t)package[15] << 8 |
                 (uint32_t)package[16] << 16 | (uint32_t)package[17] << 24,
      .file_size = size,
      .manufacturer = package[10] | (uint16_t)package[11] << 8,
      .image_type = package[12] | (uint16_t)package[13] << 8,
    };
    uint32_t ota_size = package[6] | (uint16_t)package[7] << 8;
    prefix_size = ota_size + 8 + package[ota_size + 6] +
                  ((uint32_t)package[ota_size + 7] << 8);
    assert(ota_transfer_start(&storage, &id) == 0);
    assert(feed_until(prefix_size + 12345, 64) == 0);
    assert(restart() == prefix_size + 12288);
    finish();
    puts("Generated Zigbee artifact resumed and reconstructed successfully");
    return 0;
  }
  /* Headers split at every byte and real block sizes; small, boundary and
   * maximum-capacity application images, including the old hardware range. */
  const uint32_t sizes[] = {32, 4095, 4096, 4097, 12001, OTA_IMAGE_CAPACITY};
  for (unsigned int i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
    fixture(sizes[i], i % 2);
    assert(ota_transfer_start(&storage, &id) == 0);
    assert(feed_until(prefix_size, 1) == 0);
    finish();
    assert(ota_transfer_discard() == 0);
    assert(restart() == prefix_size);
  }

  /* A reset at any byte in a transfer resumes only complete committed pages.
   * Simulate many resets without clearing the persistent NOR storage. */
  fixture(12001, false);
  assert(ota_transfer_start(&storage, &id) == 0);
  for (uint32_t stop = prefix_size + 1; stop < id.file_size; stop += 97) {
    assert(feed_until(stop, 64) == 0);
    uint32_t expected = prefix_size + (stop - prefix_size) / OTA_PAGE_SIZE * OTA_PAGE_SIZE;
    assert(restart() == expected);
  }
  finish();
  /* Power loss after CHECK redownloads the final partial page. */
  assert(restart() == prefix_size + 8192);
  finish();
  /* Typical 64-byte radio blocks cross the prefix/image boundary. The bytes
   * past that boundary are ignored when jumping to the saved checkpoint. */
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(feed_until(prefix_size + 1, 64) == 0);
  assert(ota_transfer_offset() == prefix_size + 8192);
  finish();

  /* Every mutating operation can fail before or halfway through a write/erase.
   * Reboot immediately, without cleanup, and verify the reconstructed image. */
  fixture(12001, false);
  assert(ota_transfer_start(&storage, &id) == 0);
  finish();
  unsigned int operation_count = operations;
  for (unsigned int partial = 0; partial < 2; partial++) {
    for (unsigned int cut = 1; cut <= operation_count; cut++) {
      fixture(12001, false);
      partial_failure = partial;
      fail_operation = cut;
      assert(ota_transfer_start(&storage, &id) == 0);
      int err = feed_until(id.file_size, 64);
      if (!err) { err = ota_transfer_check(); }
      assert(err == -EIO);
      restart();
      finish();
    }
  }

  /* CRC-damaged data or metadata cannot trigger a skip over corrupt bytes. */
  fixture(12001, false);
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(feed_until(prefix_size + 9000, 64) == 0);
  image[30] ^= 1;
  assert(restart() == prefix_size);
  finish();
  journal[40] ^= 1;
  assert(restart() == prefix_size);
  finish();

  /* A changed prefix (same version/size), or a different version starts fresh. */
  fixture(12001, false);
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(feed_until(prefix_size + 9000, 64) == 0);
  package[25] ^= 1; /* Comment is part of the compared prefix. */
  assert(restart() == prefix_size);
  assert(feed_until(prefix_size + 9000, 64) == 0);
  id.version++;
  put32(package + 14, id.version);
  put16(package + prefix_size + 22, 5);
  assert(restart() == prefix_size);
  finish();

  /* Cold-boot initialization must preserve the saved journal, while a leave
   * before any new START must erase it. Failed erases are reported for retry. */
  fixture(12001, false);
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(feed_until(prefix_size + 9000, 64) == 0);
  uint8_t saved_journal[OTA_PAGE_SIZE];
  memcpy(saved_journal, journal, sizeof(journal));
  assert(ota_transfer_init(&storage) == 0);
  assert(!memcmp(saved_journal, journal, sizeof(journal)));
  assert(ota_transfer_check() == -EINVAL);
  fail_operation = operations + 1;
  assert(ota_transfer_discard() == -EIO);
  fail_operation = 0;
  assert(ota_transfer_discard() == 0);
  for (uint32_t i = 0; i < sizeof(journal); i++) { assert(journal[i] == 0xff); }
  assert(restart() == prefix_size);
  finish();
  assert(ota_transfer_init(NULL) == -EINVAL);
  assert(ota_transfer_discard() == -ENODEV);

  /* Reject incompatible/truncated/oversized/noncanonical packages and gaps. */
  for (unsigned int mutation = 0; mutation < 8; mutation++) {
    fixture(12001, false);
    switch (mutation) {
    case 0: package[0] ^= 1; break;
    case 1: package[10] ^= 1; break;
    case 2: package[12] ^= 1; break;
    case 3: package[14] ^= 1; break;
    case 4: package[52] ^= 1; break;
    case 5: package[56] = 1; break;
    case 6: package[56 + 8 + 10] = 1; break; /* Image id */
    case 7: package[prefix_size + 20] = 1; break;
    }
    assert(ota_transfer_start(&storage, &id) == 0);
    assert(feed_until(id.file_size, 64) == -EINVAL);
    assert(ota_transfer_check() == -EINVAL);
  }
  fixture(12001, false);
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(ota_transfer_receive(1, package, 1) == -EINVAL);
  assert(ota_transfer_check() == -EINVAL);
  assert(ota_transfer_discard() == 0);
  assert(ota_transfer_receive(0, package, 1) == -EINVAL);
  id.file_size = UINT32_MAX;
  assert(ota_transfer_start(&storage, &id) == -EINVAL);
  /* A rejected final block must never later pass CHECK just because it
   * advanced the cursor to the advertised size before detecting bad data. */
  fixture(32, false);
  package[prefix_size] ^= 1;
  assert(ota_transfer_start(&storage, &id) == 0);
  assert(feed_until(id.file_size, 64) == -EINVAL);
  assert(ota_transfer_check() == -EINVAL);
  puts("OTA page resume and power-cut tests passed");
  return 0;
}

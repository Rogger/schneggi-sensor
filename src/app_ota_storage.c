#include "app_ota_storage.h"
#include <errno.h>
#include <pm_config.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/storage/flash_map.h>

static const struct flash_area *journal_area, *image_area;

static const struct flash_area *area(bool journal)
{
  return journal ? journal_area : image_area;
}

static int read_data(bool journal, uint32_t offset, void *data, size_t size)
{
  return flash_area_read(area(journal), offset, data, size);
}

static int write_data(bool journal, uint32_t offset, const void *data, size_t size)
{
  return flash_area_write(area(journal), offset, data, size);
}

static int erase_data(bool journal, uint32_t offset, size_t size)
{
  return flash_area_erase(area(journal), offset, size);
}

const struct ota_storage app_ota_storage = {
  .read = read_data, .write = write_data, .erase = erase_data,
};

int app_ota_storage_init(void)
{
  int err = flash_area_open(PM_RESERVED_ID, &journal_area);
  if (!err) { err = flash_area_open(PM_MCUBOOT_SECONDARY_ID, &image_area); }
  if (err) { return err; }
  struct flash_pages_info info;
  err = flash_get_page_info_by_offs(image_area->fa_dev, image_area->fa_off, &info);
  if (err) { return err; }
  if (info.size != OTA_PAGE_SIZE || journal_area->fa_size != OTA_PAGE_SIZE ||
      image_area->fa_size != 0x75000 || image_area->fa_off % OTA_PAGE_SIZE ||
      journal_area->fa_off % OTA_PAGE_SIZE ||
      flash_area_align(image_area) != 4 || flash_area_align(journal_area) != 4) {
    return -EINVAL;
  }
  err = flash_get_page_info_by_offs(journal_area->fa_dev, journal_area->fa_off, &info);
  if (err) { return err; }
  if (info.size != OTA_PAGE_SIZE) { return -EINVAL; }
  return 0;
}

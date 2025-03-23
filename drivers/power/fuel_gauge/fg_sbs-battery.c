// Copyright 2024 The FydeOS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Author: Yang Tsao<yang@fydeos.io>

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <i2c.h>
#include <power/fuel_gauge.h>
#include <power/pmic.h>

DECLARE_GLOBAL_DATA_PTR;

#if DEBUG
#define SBS_DBG(args...) \
  do { \
     printf(args); \
  } while (0)
#else
#define SBS_DBG(args...)
#endif

/* battery status value bits */
#define BATTERY_INITIALIZED   0x80
#define BATTERY_DISCHARGING   0x40
#define BATTERY_FULL_CHARGED    0x20
#define BATTERY_FULL_DISCHARGED   0x10

#define SBS_CURRENT_REG 0x0A
#define SBS_VOLTAGE_REG 0x09
#define SBS_CAPACITY_REG 0x0D
#define SBS_TEMP_REG 0x08
#define SBS_STATUS_REG 0x16

struct sbs_info {
  struct udevice *dev;
  u16 i2c_retry_count;
};

static int sbs_read_int(struct sbs_info *sbs, u8 reg)
{
  u16 val;
  int ret = 0;
	int retries = sbs->i2c_retry_count;
  while (retries >0) {
  	ret = dm_i2c_read(sbs->dev, reg, (u8 *)&val, 2);
		if (!ret)
			break;
		retries--;
  }
  if (ret) {
    SBS_DBG("faile to read dev:%p, reg:%#x, ret:%d\n", sbs->dev, reg, ret);
    return -EINVAL;
  }
  return val;
}

static int sbs_get_current(struct sbs_info *sbs)
{
	return sbs_read_int(sbs, SBS_CURRENT_REG);
}

static int sbs_update_get_current(struct udevice *dev)
{
  struct sbs_info *sbs = dev_get_priv(dev);

  return sbs_get_current(sbs);
}

static bool sbs_check_charge(struct sbs_info *sbs)
{
  int type = sbs_read_int(sbs, SBS_STATUS_REG);
	int current;
	if (type < 0)
		return false;
	if (type & BATTERY_DISCHARGING)
		return false;
	current = sbs_read_int(sbs, SBS_CURRENT_REG);
  if (current < 0)
		return false;
	if (current == 0 && (type & BATTERY_FULL_CHARGED))
		return false;
  return true;
}

static bool sbs_update_get_chrg_online(struct udevice *dev)
{
  struct sbs_info *sbs = dev_get_priv(dev);

  return sbs_check_charge(sbs);
}

static int sbs_get_vol(struct sbs_info *sbs)
{
	return sbs_read_int(sbs, SBS_VOLTAGE_REG);
}

static int sbs_update_get_voltage(struct udevice *dev)
{
  struct sbs_info *sbs = dev_get_priv(dev);

  return sbs_get_vol(sbs);
}

static int sbs_get_temperature(struct udevice *dev, int *temp)
{
	struct sbs_info *sbs = dev_get_priv(dev);
	int bat_tmp = sbs_read_int(sbs, SBS_TEMP_REG);
	if (bat_tmp == -EINVAL)
		return bat_tmp;
	bat_tmp = bat_tmp / 10 - 273;
	SBS_DBG("sbs temp:%d in CEL\n", bat_tmp);
	*temp = bat_tmp;
	return 0;
}

static int sbs_get_soc(struct sbs_info *sbs)
{
	return sbs_read_int(sbs, SBS_CAPACITY_REG);
}

static int sbs_update_get_soc(struct udevice *dev)
{
  struct sbs_info *sbs = dev_get_priv(dev);

  return sbs_get_soc(sbs);
}

static int sbs_capability(struct udevice *dev)
{
  return FG_CAP_FUEL_GAUGE;
}

static const struct udevice_id sbs_ids[] = {
  { .compatible = "sbs,sbs-battery" },
  { }
};

static struct dm_fuel_gauge_ops sbs_fg_ops = {
  .capability = sbs_capability,
  .get_soc = sbs_update_get_soc,
  .get_voltage = sbs_update_get_voltage,
  .get_current = sbs_update_get_current,
  .get_temperature = sbs_get_temperature,
  .get_chrg_online = sbs_update_get_chrg_online,
};

static int sbs_init(struct sbs_info *sbs)
{
	int ret = sbs_read_int(sbs, SBS_STATUS_REG);
	if (ret < 0) {
		SBS_DBG("battery not present\n");
		return 0;
	}
	sbs_get_temperature(sbs->dev, &ret);
	if (ret < -100 || ret > 100)
		printf("invalid temp:%d\n", ret);
	else
		printf("invalid temp:%d\n", ret);
	return 0;
}

static int sbs_fg_probe(struct udevice *dev)
{
  struct sbs_info *sbs = dev_get_priv(dev);

  sbs->dev = dev;
  printf("sbs driver version-20240712\n");
  sbs_init(sbs);
  printf("sbs vol: %d, soc: %d\n",
         sbs_get_vol(sbs), sbs_get_soc(sbs));

  return 0;
}

static int sbs_ofdata_to_platdata(struct udevice *dev)
{
  struct sbs_info *sbs = dev_get_priv(dev);
	sbs->i2c_retry_count = (u16) dev_read_u32_default(dev, "sbs,i2c-retry-count", 10);
	return 0;
}

U_BOOT_DRIVER(sbs_fg) = {
  .name = "sbs-battery_fg",
  .id = UCLASS_FG,
  .of_match = sbs_ids,
  .probe = sbs_fg_probe,
  .ofdata_to_platdata = sbs_ofdata_to_platdata,
  .ops = &sbs_fg_ops,
  .priv_auto_alloc_size = sizeof(struct sbs_info),
};

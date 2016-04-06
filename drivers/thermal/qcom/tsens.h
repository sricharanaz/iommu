/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __QCOM_TSENS_H__
#define __QCOM_TSENS_H__

#define ONE_PT_CALIB		0x1
#define ONE_PT_CALIB2		0x2
#define TWO_PT_CALIB		0x3

struct tsens_device;

struct tsens_sensor {
	struct tsens_device		*tmdev;
	struct thermal_zone_device	*tzd;
	int				offset;
	int				id;
	int				hw_id;
	u32				slope;
	u32				status;
};

struct tsens_ops {
	/* mandatory callbacks */
	int (*init)(struct tsens_device *);
	int (*calibrate)(struct tsens_device *);
	int (*get_temp)(struct tsens_device *, int, int *);
	/* optional callbacks */
	int (*enable)(struct tsens_device *, int);
	void (*disable)(struct tsens_device *);
	int (*suspend)(struct tsens_device *);
	int (*resume)(struct tsens_device *);
	int (*get_trend)(struct tsens_device *, int, long *);
};

/* Registers to be saved/restored across a context loss */
struct tsens_context {
	int	threshold;
	int	control;
};

struct tsens_device {
	struct device			*dev;
	u32				num_sensors;
	struct regmap			*map;
	struct regmap_field		*status_field;
	struct tsens_context		ctx;
	bool				trdy;
	const struct tsens_ops		*ops;
	struct tsens_sensor		sensor[0];
};

char *qfprom_read(struct device *, const char *);
void compute_intercept_slope(struct tsens_device *, u32 *, u32 *, u32);
int init_common(struct tsens_device *);
int get_temp_common(struct tsens_device *, int, int *);

extern const struct tsens_ops ops_8960, ops_8916, ops_8974;

#endif /* __QCOM_TSENS_H__ */

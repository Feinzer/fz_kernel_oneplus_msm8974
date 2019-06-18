/*
 * Copyright 2019 Daniel Hernández <danhernan23i@gmail.com>
 *                Sultan Alsawaf <sultan@kerneltoast.com>
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

/*
 * GPU Boost Driver
 * A Simple boosting driver for the msm-adreno-tz GPU governor.
 */

#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/msm_adreno_devfreq.h>
#include <linux/input.h>
#include <linux/state_notifier.h>
#include <linux/slab.h>
#include "governor.h"

#include <linux/gpu_boost.h>

#define TAG "gpu_boost: "

static struct devfreq *tz_devfreq_g;
static struct work_struct boost_work;
static struct delayed_work unboost_work;
static struct work_struct reboost_work;
static struct work_struct boost_max_work;
static bool gpu_boost_running;
static bool reboost_running;

/*
 * Automatic Boost modes toggle
 * 0 - Disabled
 * 1 - Input Boost
 * 2 - Load Boost
 */
unsigned int boost_mode;
module_param(boost_mode, uint, 0644);

/*
 * Threshold on which the GPU will boost
 * to the desired frequency for x amount
 * of time.
 */
static unsigned long load_threshold;
module_param(load_threshold, ulong, 0644);

/*
 * Frequency to which the GPU will boost
 * to the desired frequency for x amount
 * of time.
 */
static unsigned long boost_freq;
module_param(boost_freq, ulong, 0644);

/*
 * Duration the GPU boost will last
 * in miliseconds.
 */
static unsigned long boost_duration;
module_param(boost_duration, ulong, 0644);

void set_devfreq_g(struct devfreq *devfreq)
{
    tz_devfreq_g = devfreq;
}
EXPORT_SYMBOL(set_devfreq_g);

static void gpu_update_devfreq(struct devfreq *devfreq)
{
	mutex_lock(&devfreq->lock);
	update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
}

static void gpu_boost_max_worker(struct work_struct *work)
{
	struct devfreq *devfreq = tz_devfreq_g;

	devfreq->min_freq = devfreq->max_freq;

	gpu_update_devfreq(devfreq);

	schedule_delayed_work(&unboost_work, msecs_to_jiffies(boost_duration));
}

static void gpu_boost_worker(struct work_struct *work)
{
	struct devfreq *devfreq = tz_devfreq_g;

	devfreq->min_freq = boost_freq;

	gpu_update_devfreq(devfreq);

	schedule_delayed_work(&unboost_work, msecs_to_jiffies(boost_duration));
}

static void gpu_unboost_worker(struct work_struct *work)
{
	struct devfreq *devfreq = tz_devfreq_g;

	/* Use lowest frequency */
	devfreq->min_freq =
		devfreq->profile->freq_table[devfreq->profile->max_state - 1];

	gpu_update_devfreq(devfreq);

	gpu_boost_running = false;
}

static void gpu_reboost_worker(struct work_struct *work)
{
	if (cancel_delayed_work_sync(&unboost_work))
		schedule_delayed_work(&unboost_work,
			msecs_to_jiffies(boost_duration));

	reboost_running = false;
}

static void do_gpu_boost(void)
{
	if (state_suspended)
		return;

	if (!boost_freq || !boost_duration)
		return;

	if (!tz_devfreq_g)
		return;

	if (reboost_running)
		return;

	if (gpu_boost_running) {
		reboost_running = true;
		schedule_work(&reboost_work);
		return;
	}

	gpu_boost_running = true;
	queue_work(system_highpri_wq, &boost_work);
}

/* Function to Boost to GPU to its maximum frequency in custom events */
void gpu_boost_kick_max(void)
{
	if (state_suspended)
		return;

	if (!boost_freq || !boost_duration)
		return;

	if (!tz_devfreq_g)
		return;

	if (reboost_running)
		return;

	if (gpu_boost_running) {
		reboost_running = true;
		schedule_work(&reboost_work);
		return;
	}

	gpu_boost_running = true;
	queue_work(system_highpri_wq, &boost_max_work);
}
EXPORT_SYMBOL(gpu_boost_kick_max);

/* Function to Boost to GPU to the default settings in custom events */
void gpu_boost_kick(void)
{
	do_gpu_boost();
}
EXPORT_SYMBOL(gpu_boost_kick);

int gpu_load_boost_event(struct devfreq_dev_status stats)
{
	if (!load_threshold || boost_mode != 2 || !stats.total_time)
		return 0;

	if ((stats.busy_time * 100 / stats.total_time) < load_threshold)
		return 0;

	do_gpu_boost();
	return 1;
}
EXPORT_SYMBOL(gpu_load_boost_event);

static void gpu_ib_input_event(struct input_handle *handle,
                unsigned int type, unsigned int code, int value)
{
	if (boost_mode != 1)
        	return;
	do_gpu_boost();
}

static int gpu_ib_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "gpu_ib_handle";

	ret = input_register_handle(handle);
	if (ret)
		goto err2;

	ret = input_open_device(handle);
	if (ret)
		goto err1;

	return 0;

err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return ret;
}

static void gpu_ib_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id gpu_ib_ids[] = {
	/* multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	/* touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) },
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) },
	},
	{ },
};

static struct input_handler gpu_ib_input_handler = {
	.event		= gpu_ib_input_event,
	.connect	= gpu_ib_input_connect,
	.disconnect	= gpu_ib_input_disconnect,
	.name		= "gpu_ib_handler",
	.id_table	= gpu_ib_ids,
};

void gpu_boost_start(void)
{
	int ret;

	INIT_WORK(&boost_work, gpu_boost_worker);
	INIT_DELAYED_WORK(&unboost_work, gpu_unboost_worker);
	INIT_WORK(&reboost_work, gpu_reboost_worker);
	INIT_WORK(&boost_max_work, gpu_boost_max_worker);

	ret = input_register_handler(&gpu_ib_input_handler);
	if (ret)
		pr_err(TAG "failed to register input handler\n");
}
EXPORT_SYMBOL(gpu_boost_start);

void gpu_boost_stop(void)
{
		cancel_work_sync(&boost_work);
		cancel_work_sync(&reboost_work);
		cancel_delayed_work_sync(&unboost_work);
		cancel_work_sync(&boost_max_work);
		tz_devfreq_g = NULL;
}
EXPORT_SYMBOL(gpu_boost_stop);

static int __init gpu_boost_init(void)
{
    pr_info(TAG "starting service");
    return 0;
}
subsys_initcall(gpu_boost_init);

static void __exit gpu_boost_exit(void)
{
    return;
}
module_exit(gpu_boost_exit);

MODULE_AUTHOR("Daniel Hernández <danhernan23i@gmail.com>");
MODULE_LICENSE("GPLv2");

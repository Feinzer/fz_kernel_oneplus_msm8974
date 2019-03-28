/*
 * 2019 (C) Victor Bo <eremitein@xda aka zerovoid@4pda>
 * 
 * This software is licensed under the terms of the GNU General Public
 * License version 3, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 */

/*
 * Adreno Real Idler - fix power algorithm, an efficient workaround
 * for Adreno's GPU overheads
 *
 * Since Adreno GPU tends to *not* use the lowest frequency even on idle,
 * Adreno Real Idler replaces power algorithm
 * 
 * The higher frequencies are not touched, so high-demanding games
 * will (most likely) not suffer from worsened performance
 *
 */

#include <linux/module.h>
#include <linux/adreno_real_idler.h>

#define ADRENO_REAL_IDLER_MAJOR_VERSION 1
#define ADRENO_REAL_IDLER_MINOR_VERSION 0

/* Switch to on/off adreno_real_idler */
bool adreno_idler_active = true;
module_param_named(adreno_idler_active, adreno_idler_active, bool, 0664);

static int __init adreno_idler_init(void)
{
	pr_info("adreno_real_idler: version %d.%d by eremitein\n",
		 ADRENO_REAL_IDLER_MAJOR_VERSION,
		 ADRENO_REAL_IDLER_MINOR_VERSION);
	return 0;
}
subsys_initcall(adreno_idler_init);

static void __exit adreno_idler_exit(void)
{
	return;
}
module_exit(adreno_idler_exit);

MODULE_AUTHOR("Victor Bo <eremitein@xda>");
MODULE_DESCRIPTION("'adreno_real_idler - a powersaver for Adreno GPU");
MODULE_LICENSE("GPL");

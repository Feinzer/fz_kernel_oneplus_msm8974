#ifndef _GPU_BOOST_H
#define _GPU_BOOST_H

#include <linux/devfreq.h>

extern void gpu_boost_start(void);
extern void gpu_boost_stop(void);
extern void register_boost_devfreq(struct devfreq *devfreq);
extern int gpu_load_boost_event(struct devfreq_dev_status stats);

#endif /* _GPU_BOOST_H */

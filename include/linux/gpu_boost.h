#ifndef _GPU_BOOST_H
#define _GPU_BOOST_H

#include <linux/devfreq.h>

extern void gpu_boost_start(void);
extern void gpu_boost_stop(void);
extern void set_devfreq_g(struct devfreq *devfreq);
extern void gpu_boost_kick(void);
extern void gpu_boost_kick_max(void);
extern int gpu_load_boost_event(struct devfreq_dev_status stats);
extern unsigned int boost_mode;

#endif /* _GPU_BOOST_H */

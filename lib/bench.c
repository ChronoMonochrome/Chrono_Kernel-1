/*
 * Copyright (C) 2015 Shilin Victor <chrono.monochrome@gmail.com>
 */

#include <linux/module.h>   // included for all kernel modules
#include <linux/init.h>     // included for __init and __exit macros
#include <linux/time.h>     // ktime_to_us
#include <linux/hrtimer.h>  // ktime_get
 
 
const int LOOP_BOUND = 200000000;
 
static int __init bench_init(void)
{
    s64 time;
 
    int I, J, K;
 
    J = 2;
    K = 3;
    time = ktime_to_us(ktime_get());
 
    for (I = 1; I <= LOOP_BOUND; I++) {
        J = J + K;
        K = J + K;
        J = K - J;
        K = K - J - J;
    }
    
    pr_err("[Bench] time = %lld us\n", (long long)(ktime_to_us(ktime_get()) - time));
 
    return 0;    // Non-zero return means that the module couldn't be loaded.
}
 
static void __exit bench_exit(void)
{
}
 
module_init(bench_init);
module_exit(bench_exit);
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Shilin Victor");
MODULE_DESCRIPTION("A simple benchmark");

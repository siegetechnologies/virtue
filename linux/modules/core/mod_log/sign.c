#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/types.h>
#include <crypto/sha256_base.h>
#include <crypto/sha.h>
#include <crypto/hmac.h>
#include <crypto/hash.h>
#include <crypto/akcipher.h>
#include <linux/mutex.h>

#include <linux/printk.h>



#include "../../include/mod_allocator.h"
#include "../../include/mod_log.h"
#include "log.h"

//base64 decoding code... should probably be moved somewhere else

#ifdef SIGNIT
char encode[64] ={0};
char decode[256] = {0};

void gen_encodetable(void){
	int i = 0;
	char s;
	for(s = 'A'; s <= 'Z'; s++) encode[i++] =s;
	for(s = 'a'; s <= 'z'; s++) encode[i++] =s;
	for(s = '0'; s <= '9'; s++) encode[i++] =s;
	encode[i++] = '+';
	encode[i++] = '/';
}
void gen_decodetable(void){
	int i = 0;
	int s;
	for(s = 'A'; s <= 'Z'; s++) decode[s] =i++;
	for(s = 'a'; s <= 'z'; s++) decode[s] =i++;
	for(s = '0'; s <= '9'; s++) decode[s] =i++;
	decode['+'] = i++;
	decode['/'] = i++;
}

//you need to manually free *out
//todo move this to something else
ssize_t base64_parse(char * in, uint8_t ** out){
	size_t i;
	uint8_t *status;
	size_t inlen = strlen(in);
	size_t inlen_padded = (inlen + 3) & ~3;
	size_t outlen = (inlen_padded/4) * 3;
	if(inlen != inlen_padded){
		//Warn for not using padding.
		printk("Warning, input base64 string not padded to 4 chars\n");
	}
	status = *out = maven_alloc(outlen);

	//decode chunks of 4
	for(i = 0; i+3 < inlen; i+=4){
		uint8_t b[4]  = {decode[(int)in[i]], decode[(int)in[i+1]], decode[(int)in[i+2]], decode[(int)in[i+3]]};
		status[0] = b[0] << 2 | b[1] >> 4;
		status[1] = b[1] << 4 | b[2] >> 2;
		status[2] = b[2] << 6 | b[3];
		status+=3;
	}

	//decode extra because someone forgot padding
	if(i < inlen){
		uint8_t b[4]  ={0};
		if(i < inlen)	b[0] = decode[(int)in[i]];
		if(i+1 < inlen)	b[1] = decode[(int)in[i+1]];
		if(i+2 < inlen)	b[2] = decode[(int)in[i+1]];
		if(i+3 < inlen)	b[3] = decode[(int)in[i+1]];
		status[0] = b[0] << 2 | b[1] >> 4;
		status[1] = b[1] << 4 | b[2] >> 2;
		status[2] = b[2] << 6 | b[3];
		status+=3;
	}
	//figure out how many bytes i can chop off the end (0-2)
	//count how much padding (or not) was added, should be 0-2 for a non messed up key
	for(i = inlen; i > 0 && in[i-1] == '='; i--);
	i = inlen_padded - i;
	//messed up key might return negative numbers, but thats OK, since negative numbers is error

	return outlen - i;
}



struct crypto_akcipher *tfm;

#endif //SIGNIT

struct crypto_shash *sfm;

static atomic64_t signed_msg_num = ATOMIC64_INIT(0);

struct mutex prevdiglock;
int sign_init(void){
	//wait for randomness
	if(wait_for_random_bytes()){
		printk(KERN_WARNING "mod_log/sign wait_for_random_bytes failed");
		return -1;
	}
	//not terribly portable... i think this wont work on sparc
	//TODO make portable
	get_random_bytes(&signed_msg_num, sizeof(atomic64_t));

	//create crypto context
//	tfm = crypto_alloc_akcipher( "rsa", 0, 0);
//	tfm = crypto_alloc_akcipher( "rsa", 0, CRYPTO_ALG_ASYNC);
//	tfm = crypto_alloc_akcipher( "pkcs1pad(rsa,sha256)", 0, CRYPTO_ALG_ASYNC);

#ifdef SIGNIT
	gen_encodetable();
	gen_decodetable();

	tfm = crypto_alloc_akcipher( "pkcs1pad(rsa,sha256)", 0, 0);
	if(IS_ERR(tfm)){
		int error = PTR_ERR(tfm);
		printk(KERN_INFO "Mod_log/sign : Failed to setup profile for signing with error %d", error);
		return error;
	}
#endif
	sfm = crypto_alloc_shash("sha256", 0, CRYPTO_ALG_ASYNC);
	if(IS_ERR(sfm)){
		int error = PTR_ERR(sfm);
		printk(KERN_INFO "Mod_log/sign : Failed to setup profile for hashing with error %d", error);
		return error;
	}
	mutex_init(&prevdiglock);

	return 0;
}

void sign_shutdown(void){
	mutex_destroy(&prevdiglock);
	#ifdef SIGNIT
	crypto_free_akcipher(tfm);
	#endif
	crypto_free_shash(sfm);
}

#ifdef SIGNIT
int sign_parse_priv_raw(uint8_t * key, size_t len){
	int res;
	BUG_ON(!key);
	BUG_ON(len < 1);
	if(( res = crypto_akcipher_set_priv_key(tfm, key, len))){
		printk(KERN_WARNING "Setting crypto priv key failed! %d", res);
		return res;
	}
	return 0;
}
int sign_parse_pub_raw(uint8_t * key, size_t len){
	int res;
	BUG_ON(!key);
	BUG_ON(len < 1);
	if(( res = crypto_akcipher_set_pub_key(tfm, key, len))){
		printk(KERN_WARNING "Setting crypto pub key failed! %d", res);
		return res;
	}
	return 0;
}
int sign_parse_priv(char * b64key){
	ssize_t keylen;
	uint8_t * key = 0;
	int res = 0;
	keylen = base64_parse(b64key, &key);
	BUG_ON(!key);
	res = sign_parse_priv_raw(key, keylen);
	maven_free(key);
	return res;
}
int sign_parse_pub(char * b64key){
	ssize_t keylen;
	uint8_t * key = 0;
	int res = 0;
	keylen = base64_parse(b64key, &key);
	BUG_ON(!key);
	res = sign_parse_pub_raw(key, keylen);
	maven_free(key);
	return res;
}


//have to allocate dynamically (for now), because crypto_akcipher_reqsize(tfm)

static inline struct akcipher_request * maven_akcipher_request_alloc(struct crypto_akcipher *tfm){
	struct akcipher_request *req;
	//guessing 8
	req = maven_alloc_aligned(sizeof(*req) + crypto_akcipher_reqsize(tfm), 64);
	if(likely(req))
		akcipher_request_set_tfm(req, tfm);
	return req;
}

//will sign a SHA256 digest.
//hash must be size SHA256_DIGEST_SIZE or larger
int hash_sign(uint8_t * hash, void * digest, size_t digsize){
	int res;
	struct crypto_wait wait;
	struct scatterlist src, dst;
	struct akcipher_request * req;
	req = maven_akcipher_request_alloc(tfm);
	ALLOC_FAIL_CHECK(req);
	crypto_init_wait(&wait);
	sg_init_one(&src, hash, SHA256_DIGEST_SIZE);
	sg_init_one(&dst, digest, digsize);
	akcipher_request_set_crypt(req, &src, &dst, SHA256_DIGEST_SIZE, digsize);
	akcipher_request_set_callback(req, CRYPTO_TFM_REQ_MAY_BACKLOG, crypto_req_done, &wait);

	res = crypto_wait_req(crypto_akcipher_sign(req), &wait);

	printk("Res was %i\n", res);
	printk("siz was %i\n", req->dst_len);

	maven_free(req);
	return res;
}
#endif //SIGNIT

//hashes a message using sha256

int msg_hash(struct partial_msg *msg, void * digest){
	int res;
	size_t i;
	SHASH_DESC_ON_STACK(desc, sfm);
	desc->tfm = sfm;
	desc->flags = CRYPTO_TFM_REQ_MAY_SLEEP;
	res = crypto_shash_init(desc);
	if(res){
		printk( KERN_WARNING "log_sign: Error in shash_init\n");
		return 0;
	}

	//update piecewise for every entry
	for(i = 0; i < msg->entries_filled; i++){
		struct iovec * iov = msg->entries + i;
		res = crypto_shash_update(desc, iov->iov_base, iov->iov_len);
		if(res){
			printk(KERN_WARNING "log_sign: Error in shash_update\n");
			return 0;
		}
	}

	//get the finality
	res = crypto_shash_final(desc, digest);
	if(res){
		printk( KERN_WARNING "log_sign: Error in shash_final\n");
		return 0;
	}
	return 0;
}

//todo change this from a dynamic allocated thing to static on stack
struct sign_hash_dig {
	uint64_t size;
	uint8_t dig[0];
};
//todo use sha_buf from mod_sensors?
struct hash_dig {
	uint64_t size;
	uint8_t dig[SHA256_DIGEST_SIZE];
};

struct hash_dig prevdig = {0};


int msg_sign(struct partial_msg *msg){
	struct hash_dig d = {SHA256_DIGEST_SIZE, {0}};
//	uint8_t msghash[SHA256_DIGEST_SIZE];
	struct sign_hash_dig * digest = 0;
	BUG_ON(!msg);

	//add the nonce/seq number on
//	uint64_t seq;
//	seq = atomic64_fetch_add(1, &signed_msg_num);
//	msg_additem(msg, "sign_seq", &seq, TYPE_INT);

	mutex_lock(&prevdiglock);
	msg_additem(msg, "sign_prev", &prevdig, TYPE_BYTES);
	//hash item
	if(msg_hash(msg, d.dig)){
		mutex_unlock(&prevdiglock);
		printk(KERN_WARNING "Error hashing\n");
		goto cleanitup;
	}
//	print_hex_dump(KERN_DEBUG, "dig", DUMP_PREFIX_OFFSET, 16, 1, d.dig, SHA256_DIGEST_SIZE, 0);
//	memcpy(&prevdig, &d, sizeof(struct hash_dig));
	prevdig = d;
	mutex_unlock(&prevdiglock);


#ifdef SIGNIT
	sz = crypto_akcipher_maxsize(tfm);
	digest = maven_alloc_aligned(sz + sizeof(struct sign_hash_dig), 64);
	ALLOC_FAIL_CHECK(digest);
	digest->size = sz;

	//sign the hash
	if(hash_sign(d->dig, digest->dig, sz))
		goto cleanitup;


	if(msg->entries_filled >= msg->entries_cap){
		//TODO handle when there isnt enough space to add stuff?
		//What should the behavior here be? I assume we still want to send the packet...
		//i guess lets just have it fail as usual and "drop" the added item
		//or just drop it here and warn?
		//or dynamically re-allocate?
	}

	msg_additem(msg, "sign_sig", digest, TYPE_BYTES);

#endif
//	print_hex_dump(KERN_DEBUG, "dig", DUMP_PREFIX_OFFSET, 16, 1, digest->dig, sz, 0);

	cleanitup:
		if(digest) maven_free(digest);
		return 0;
}

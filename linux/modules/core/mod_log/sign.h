#ifndef SIGNHEADER
#define SIGNHEADER


int sign_init(void);

int sign_parse_priv(char * bkey);
int sign_parse_pub(char * bkey);

int sign_parse_priv_raw(uint8_t * key, size_t len);
int sign_parse_pub_raw(uint8_t * key, size_t len);
int msg_sign(struct partial_msg *msg);

#endif

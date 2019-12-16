
typedef struct sha_buf{
	uint64_t sz;
	uint8_t buf[32];
} sha_buf;
int init_crypto(void);
int cleanup_crypto(void);
int hash_sct( sha_buf *addrbuf, sha_buf *textbuf );
int hash_idt( sha_buf *idt_vec_buf, sha_buf *idt_code_buf );

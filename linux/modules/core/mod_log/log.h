#ifndef LOGHEADER
#define LOGHEADER

struct partial_msg {
	struct iovec* entries;
	size_t entries_cap;
	size_t entries_filled;
	size_t len;
};

#endif

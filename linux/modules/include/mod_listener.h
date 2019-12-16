#ifndef LISTENERHEADER
#define LISTENERHEADER

	typedef void (*listenercallback_t)(char * cmd, struct api_response *out);

	typedef struct endpoint_s {
		char * name;
		listenercallback_t cab;
		struct endpoint_s * next;
	} endpoint_t;

//register an endpoint for callback
//directly uses *ep, so dont go freeing it until you unregister it!
//best to just make them static
int endpoint_register(endpoint_t *ep);


//unregister it
int endpoint_unregister(endpoint_t *ep);


#endif

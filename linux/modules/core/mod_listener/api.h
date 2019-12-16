#ifndef API_H
#define API_H

struct api_response;
struct uuid_t;

int parse_api( char* msg, struct api_response* out_resp );
typedef void apicmd(char*, struct api_response*);
void transducer_init( void );
void set_transducer_uuid( char* args, struct api_response *out );


#endif //API_H

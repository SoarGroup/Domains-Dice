/** THIS IS AN AUTOMATICALLY GENERATED FILE.  DO NOT MODIFY
 * BY HAND!!
 *
 * Generated by lcm-gen
 **/

#include <stdint.h>
#include <stdlib.h>
#include <lcm/lcm_coretypes.h>
#include <lcm/lcm.h>

#ifndef _tag_detection_list_t_h
#define _tag_detection_list_t_h

#ifdef __cplusplus
extern "C" {
#endif

#include "tag_detection_t.h"
typedef struct _tag_detection_list_t tag_detection_list_t;
struct _tag_detection_list_t
{
    int64_t    utime;
    int32_t    width;
    int32_t    height;
    int32_t    ndetections;
    tag_detection_t *detections;
};
 
tag_detection_list_t   *tag_detection_list_t_copy(const tag_detection_list_t *p);
void tag_detection_list_t_destroy(tag_detection_list_t *p);

typedef struct _tag_detection_list_t_subscription_t tag_detection_list_t_subscription_t;
typedef void(*tag_detection_list_t_handler_t)(const lcm_recv_buf_t *rbuf, 
             const char *channel, const tag_detection_list_t *msg, void *user);

int tag_detection_list_t_publish(lcm_t *lcm, const char *channel, const tag_detection_list_t *p);
tag_detection_list_t_subscription_t* tag_detection_list_t_subscribe(lcm_t *lcm, const char *channel, tag_detection_list_t_handler_t f, void *userdata);
int tag_detection_list_t_unsubscribe(lcm_t *lcm, tag_detection_list_t_subscription_t* hid);
int tag_detection_list_t_subscription_set_queue_capacity(tag_detection_list_t_subscription_t* subs, 
                              int num_messages);


int  tag_detection_list_t_encode(void *buf, int offset, int maxlen, const tag_detection_list_t *p);
int  tag_detection_list_t_decode(const void *buf, int offset, int maxlen, tag_detection_list_t *p);
int  tag_detection_list_t_decode_cleanup(tag_detection_list_t *p);
int  tag_detection_list_t_encoded_size(const tag_detection_list_t *p);

// LCM support functions. Users should not call these
int64_t __tag_detection_list_t_get_hash(void);
int64_t __tag_detection_list_t_hash_recursive(const __lcm_hash_ptr *p);
int     __tag_detection_list_t_encode_array(void *buf, int offset, int maxlen, const tag_detection_list_t *p, int elements);
int     __tag_detection_list_t_decode_array(const void *buf, int offset, int maxlen, tag_detection_list_t *p, int elements);
int     __tag_detection_list_t_decode_array_cleanup(tag_detection_list_t *p, int elements);
int     __tag_detection_list_t_encoded_array_size(const tag_detection_list_t *p, int elements);
int     __tag_detection_list_t_clone_array(const tag_detection_list_t *p, tag_detection_list_t *q, int elements);

#ifdef __cplusplus
}
#endif

#endif

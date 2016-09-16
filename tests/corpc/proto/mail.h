/*
 * mail.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_tests_corpc_proto_mail_h__
#define __cuttle_tests_corpc_proto_mail_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <cuttle/corpc/channel.h>
#include "corpc-msg.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef
struct mail {
  char * text;
} mail;

struct mail_init_args {
  const char * text;
};

void mail_init(struct mail * mail, const struct mail_init_args * args);
void mail_cleanup(struct mail * mail);

bool corpc_pack_mail(const struct mail * mail, corpc_msg * msg);
bool corpc_unpack_mail(const corpc_msg * msg, struct mail * mail);

bool corpc_stream_write_mail(corpc_stream * st, const struct mail * mail);
bool corpc_stream_read_mail(corpc_stream * st, struct mail * mail);



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_tests_corpc_proto_mail_h__ */

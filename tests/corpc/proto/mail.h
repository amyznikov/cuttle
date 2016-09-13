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
#include <cuttle/corpc/corpc-msg.h>
#include <cuttle/corpc/channel.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef
struct mail {
  char * from;
  char * to;
  char * body;
} mail;


void mail_init(struct mail * mail);
void mail_cleanup(struct mail * mail);

bool corpc_pack_mail(const struct mail * mail, corpc_msg * msg);
bool corpc_unpack_mail(const corpc_msg * msg, struct mail * mail);

bool corpc_stream_write_mail(corpc_stream * st, const struct mail * mail);
bool corpc_stream_read_mail(corpc_stream * st, struct mail * mail);



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_tests_corpc_proto_mail_h__ */

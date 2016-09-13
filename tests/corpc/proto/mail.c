/*
 * mail.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

#include "mail.h"


void mail_init(struct mail * mail)
{
  memset(mail, 0, sizeof(*mail));
}

void mail_cleanup(struct mail * mail)
{
  free(mail->from);
  free(mail->to);
  free(mail->body);
  mail_init(mail);
}

bool corpc_pack_mail(const struct mail * mail, corpc_msg * msg)
{
  return true;
}

bool corpc_unpack_mail(const corpc_msg * msg, struct mail * mail)
{
  return true;
}


bool corpc_stream_write_mail(corpc_stream * st, const struct mail * mail)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_mail(mail, &msg) ) {
    fok = corpc_stream_write(st, &msg);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_mail(corpc_stream * st, struct mail * mail)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_stream_read(st, &msg) ) {
    fok = corpc_unpack_mail(&msg, mail);
  }

  corpc_msg_clean(&msg);

  return fok;

}

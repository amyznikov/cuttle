/*
 * mail.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

#include "mail.h"


void mail_init(struct mail * mail, const struct mail_init_args * args)
{
  mail->text =  args && args->text ? strdup(args->text) : NULL;
}

void mail_cleanup(struct mail * mail)
{
  free(mail->text), mail->text = NULL;
}

bool corpc_pack_mail(const struct mail * mail, corpc_msg * msg)
{
  msg->size = strlen(msg->data = strdup(mail->text)) + 1;
  return true;
}

bool corpc_unpack_mail(const corpc_msg * msg, struct mail * mail)
{
  mail->text = strdup(msg->data);
  return true;
}


bool corpc_stream_write_mail(corpc_stream * st, const struct mail * mail)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_mail(mail, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_mail(corpc_stream * st, struct mail * mail)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_mail(&msg, mail);
  }

  corpc_msg_clean(&msg);

  return fok;

}

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

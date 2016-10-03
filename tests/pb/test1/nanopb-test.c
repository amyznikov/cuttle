/*
 * nanopb-test.c
 *
 *  Created on: Sep 25, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <math.h>
#include <stdio.h>
#include <cuttle/debug.h>
#include <cuttle/hexbits.h>
#include "alltypes.corpc.h"


static void dump_msg(const char * note, struct all_types_nested_message * msg)
{
  CF_DEBUG("<%s>: has_x=%d x=%d has_s=%d s=%s z.size=%zu",
      note, msg->has_x, msg->x, msg->has_s, msg->s, ccarray_size(&msg->z));
  CF_DEBUG("</%s>\n\n",note);
}



int main(/*int argc, char *argv[]*/)
{
  struct all_types_nested_message nestsed_src = {

    .has_x  = true,
    .x = M_PI,

    .has_s = true,
    .s = strdup("this is nested_message"),
  };

  struct all_types_nested_message nested_dst = {
    .x = 0,
    .s = NULL,
  };

  void * buf = NULL;
  size_t size;

  cf_set_loglevel(CF_LOG_DEBUG);
  cf_set_logfilename("stderr");


  dump_msg("srcmsg", &nestsed_src);

  if ( !(size = cf_pb_pack_all_types_nested_message(&nestsed_src, &buf)) ) {
    CF_FATAL("cf_pb_pack_all_types_nested_message() fails: %s", strerror(errno));
    goto end;
  }
  CF_DEBUG("pack size = %zu", size);



  if ( !cf_pb_unpack_all_types_nested_message(&nested_dst, buf, size) ) {
    CF_FATAL("cf_pb_unpack_all_types_nested_message() fails: %s", strerror(errno));
    goto end;
  }

  dump_msg("dstmsg", &nested_dst);

end:

  free(buf);

  return 0;
}


/*
 * size_t cf_pb_pack_all_types(const struct all_types * all_types, void ** buf)
{
  return cf_pb_pack(all_types, all_types_fields, buf);
}
 *
 */

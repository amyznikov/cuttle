/*
 * resolve.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

#include <cuttle/dns/resolve.h>
#include <cuttle/dns/dns.h>
#include <cuttle/debug.h>
#include <errno.h>


static const char * cf_dns_status_str(int status)
{
  switch ( status ) {
  case DNS_ENOBUFS :
    return "ENOBUFS";
  case DNS_EILLEGAL :
    return "EILLEGAL";
  case DNS_EORDER :
    return "EORDER";
  case DNS_ESECTION :
    return "ESECTION";
  case DNS_EUNKNOWN :
    return "EUNKNOWN";
  case DNS_EADDRESS :
    return "EADDRESS";
  case DNS_ENOQUERY :
    return "ENOQUERY";
  case DNS_ENOANSWER :
    return "ENOANSWER";
  case DNS_EFETCHED :
    return "EFETCHED";
  case DNS_ESERVICE :
    return "ESERVICE";
  case DNS_EFAIL :
    return "EFAIL";
  case DNS_ENONAME :
    return "ENONAME";
  }
  return "UNKNOWN";
}

static int cf_dns_status2errno(int status)
{
  switch ( status ) {
    case DNS_ENOBUFS :
      case DNS_EILLEGAL :
      case DNS_EORDER :
      case DNS_ESECTION :
      case DNS_EUNKNOWN :
      case DNS_EADDRESS :
      case DNS_ENOQUERY :
      case DNS_ENOANSWER :
      case DNS_EFETCHED :
      case DNS_ESERVICE :
      case DNS_EFAIL :
      case DNS_ENONAME :
      return ENODATA;

    default :
      break;
  }
  return status;
}

const char * cf_resolve_strerror(int status)
{
  return dns_strerror(status);
}


static struct dns_resolv_conf * getresconf(int * error)
{
  struct dns_resolv_conf * resconf = NULL;

  *error = 0;

  if (!(resconf = dns_resconf_open(error))) {
    goto end;
  }

  if ( (*error = dns_resconf_loadpath(resconf, "/etc/resolv.conf")) ) {
    goto end;
  }

  if ( (*error = dns_nssconf_loadpath(resconf, "/etc/nsswitch.conf")) ) {
    goto end;
  }

end:

  if ( *error && resconf ) {
    dns_resconf_close(resconf);
    resconf = NULL;
  }
  return resconf;
}


static struct dns_hosts * gethosts(int * error)
{
  return dns_hosts_local(error);
}



static struct dns_resolver * create_resolver(int * error)
{
  struct dns_resolver * R = NULL;
  struct dns_resolv_conf * resconf = NULL;
  struct dns_hosts * hosts = NULL;
  struct dns_hints * hints = NULL;

  *error = 0;


  if ( !(resconf = getresconf(error)) ) {
    goto end;
  }

  if ( !(hosts = gethosts(error)) ) {
    goto end;
  }

  if ( !(hints = dns_hints_local(resconf, error)) ) {
    goto end;
  }

  CF_DEBUG("resconf=%p hosts=%p hints=%p", resconf, hosts, hints);
  if ( !(R = dns_res_open(resconf, hosts, hints, NULL, dns_opts(), error)) ) {
    goto end;
  }

end : ;

  if ( hints ) {
    dns_hints_close(hints);
  }

  if ( hosts ) {
    dns_hosts_close(hosts);
  }

  if ( resconf ) {
    dns_resconf_close(resconf);
  }

  if ( *error && R ) {
    dns_res_close(R), R = NULL;
  }

  return R;
}


//  struct addrinfo ai_hints = {
//    .ai_family = PF_UNSPEC,
//    .ai_socktype = SOCK_STREAM,
//    .ai_flags = AI_CANONNAME
//  };
int cf_resolve_submit(struct cf_dns_query ** restrict pq, const char * name, const struct addrinfo * restrict hints)
{
  struct dns_resolver * R = NULL;
  struct dns_addrinfo * ai = NULL;
  int error = 0;

  if ( !(R = create_resolver(&error)) ) {
    goto end;
  }

  CF_DEBUG("dns_ai_open(%s)", name);
  if ( !(ai = dns_ai_open(name, NULL, DNS_T_A, hints, R, &error)) ) {
    CF_CRITICAL("dns_ai_open(%s) fails", name);
    goto end;
  }

end : ;

  if ( R ) {
    dns_res_close(R);
  }

  CF_DEBUG("error=%d", error);

  if ( error && ai ) {
    CF_DEBUG("dns_ai_close()");
    dns_ai_close(ai), ai = NULL;
    errno = cf_dns_status2errno(error);
  }

  * pq = (struct cf_dns_query *)ai;

  return error;
}


int cf_resolve_pollfd(const struct cf_dns_query * q)
{
  return dns_ai_pollfd((struct dns_addrinfo*) q);
}

int cf_resolve_fetch(const struct cf_dns_query * q, struct addrinfo ** ent)
{
  int status = dns_ai_nextent(ent, (struct dns_addrinfo*) q);
  errno = status < 0 ? cf_dns_status2errno(status) : status;

  CF_DEBUG("dns_ai_nextent(): status=%d (%s)(%s)(%s) DNS_EBASE=%d", status, cf_dns_status_str(status),
      dns_strerror(status), strerror(errno), DNS_EBASE);


  return status;
}

void cf_resolve_destroy(struct cf_dns_query ** q)
{
  if ( q && *q ) {
    dns_ai_close((struct dns_addrinfo *) *q);
    *q = NULL;
  }
}






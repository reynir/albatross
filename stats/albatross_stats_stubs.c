// (c) 2017, 2018 Hannes Mehnert, all rights reserved

#include <caml/mlvalues.h>
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/fail.h>
#include <caml/unixsupport.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <net/if.h>

#define Val32 caml_copy_int32
#define Val64 caml_copy_int64

#ifdef __FreeBSD__
#include <net/if_mib.h>
#include <vmmapi.h>

CAMLprim value vmmanage_sysctl_kinfo_proc (value pid_r) {
  CAMLparam1(pid_r);
  CAMLlocal4(res, res1, res2, time);
  int name[4];
  int error;
  size_t len;
  struct kinfo_proc p;
  struct rusage ru;

  len = sizeof(p);
  name[0] = CTL_KERN;
  name[1] = KERN_PROC;
  name[2] = KERN_PROC_PID;
  name[3] = Int_val(pid_r);

  error = sysctl(name, nitems(name), &p, &len, NULL, 0);
  if (error < 0)
    uerror("sysctl ctl_kern.kern_proc.kern_proc_pid", Nothing);
  if (p.ki_start.tv_usec < 0 || p.ki_start.tv_usec > 999999999)
    uerror("sysctl ctl_kern.kern_proc.kern_proc_pid", Nothing);

  res1 = caml_alloc(8, 0);
  Store_field (res1, 0, Val64(p.ki_size));
  Store_field (res1, 1, Val64(p.ki_rssize));
  Store_field (res1, 2, Val64(p.ki_tsize));
  Store_field (res1, 3, Val64(p.ki_dsize));
  Store_field (res1, 4, Val64(p.ki_ssize));
  Store_field (res1, 5, Val64(p.ki_runtime));
  Store_field (res1, 6, Val_int(p.ki_cow));
  time = caml_alloc(2, 0);
  Store_field (time, 0, Val64(p.ki_start.tv_sec));
  Store_field (time, 1, Val_int(p.ki_start.tv_usec));
  Store_field (res1, 7, time);

  ru = p.ki_rusage;
  if (ru.ru_utime.tv_usec < 0 || ru.ru_utime.tv_usec > 999999999 ||
      ru.ru_stime.tv_usec < 0 || ru.ru_stime.tv_usec > 999999999)
    uerror("sysctl ctl_kern.kern_proc.kern_proc_pid", Nothing);

  res2 = caml_alloc(16, 0);
  time = caml_alloc(2, 0);
  Store_field (time, 0, Val64(ru.ru_utime.tv_sec));
  Store_field (time, 1, Val_int(ru.ru_utime.tv_usec));
  Store_field (res2, 0, time);
  time = caml_alloc(2, 0);
  Store_field (time, 0, Val64(ru.ru_stime.tv_sec));
  Store_field (time, 1, Val_int(ru.ru_stime.tv_usec));
  Store_field (res2, 1, time);
  Store_field (res2, 2, Val64(ru.ru_maxrss));
  Store_field (res2, 3, Val64(ru.ru_ixrss));
  Store_field (res2, 4, Val64(ru.ru_idrss));
  Store_field (res2, 5, Val64(ru.ru_isrss));
  Store_field (res2, 6, Val64(ru.ru_minflt));
  Store_field (res2, 7, Val64(ru.ru_majflt));
  Store_field (res2, 8, Val64(ru.ru_nswap));
  Store_field (res2, 9, Val64(ru.ru_inblock));
  Store_field (res2, 10, Val64(ru.ru_oublock));
  Store_field (res2, 11, Val64(ru.ru_msgsnd));
  Store_field (res2, 12, Val64(ru.ru_msgrcv));
  Store_field (res2, 13, Val64(ru.ru_nsignals));
  Store_field (res2, 14, Val64(ru.ru_nvcsw));
  Store_field (res2, 15, Val64(ru.ru_nivcsw));

  res = caml_alloc(2, 0);
  Store_field (res, 1, res1);
  Store_field (res, 0, res2);

  CAMLreturn(res);
}

CAMLprim value vmmanage_vmmapi_open (value name) {
  CAMLparam1(name);
  struct vmctx *ctx;
  const char *devname;

  if (! caml_string_is_c_safe(name)) caml_raise_not_found();

  devname = String_val(name);
  ctx = vm_open(devname);
  if (ctx == NULL) uerror("vm_open", Nothing);
  CAMLreturn((value)ctx);
}

CAMLprim value vmmanage_vmmapi_close (value octx) {
  struct vmctx *ctx = (struct vmctx*)octx;

  close(vm_get_device_fd(ctx));
  free(ctx);
  return Val_unit;
}

CAMLprim value vmmanage_vmmapi_statnames (value octx) {
  CAMLparam0();
  CAMLlocal2(res, tmp);
  struct vmctx *ctx = (struct vmctx*)octx;
  int i, num_stats;
  uint64_t *s;
  const char *desc;

  s = vm_get_stats(ctx, 0, NULL, &num_stats);
  if (s != NULL) {
    for (i = 0; i < num_stats; i++) {
      desc = vm_get_stat_desc(ctx, i);
      tmp = caml_alloc(2, 0);
      Store_field (tmp, 0, caml_copy_string(desc));
      Store_field (tmp, 1, res);
      res = tmp;
    }
  }
  CAMLreturn(res);
}

CAMLprim value vmmanage_vmmapi_stats (value octx) {
  CAMLparam0();
  CAMLlocal2(res, tmp);
  int i, num_stats;
  uint64_t *stats;
  struct vmctx *ctx = (struct vmctx*)octx;

  stats = vm_get_stats(ctx, 0, NULL, &num_stats);
  if (stats != NULL) {
    for (i = 0; i < num_stats; i++) {
      tmp = caml_alloc(2, 0);
      Store_field (tmp, 0, Val64(stats[i]));
      Store_field (tmp, 1, res);
      res = tmp;
    }
  }
  CAMLreturn(res);
}

CAMLprim value vmmanage_sysctl_ifcount (value unit) {
  CAMLparam1(unit);
  int data = 0;
  size_t dlen = 0;
  int name[5];

  name[0] = CTL_NET;
  name[1] = PF_LINK;
  name[2] = NETLINK_GENERIC;
  name[3] = IFMIB_SYSTEM;
  name[4] = IFMIB_IFCOUNT;
  dlen = sizeof(data);

  if (sysctl(name, nitems(name), &data, &dlen, NULL, 0) != 0)
    uerror("sysctl", Nothing);

  CAMLreturn(Val_long(data));
}

CAMLprim value vmmanage_sysctl_ifdata (value num) {
  CAMLparam1(num);
  CAMLlocal1(res);
  size_t datalen;
  int name[6];
  struct ifmibdata data;

  name[0] = CTL_NET;
  name[1] = PF_LINK;
  name[2] = NETLINK_GENERIC;
  name[3] = IFMIB_IFDATA;
  name[4] = Int_val(num);
  name[5] = IFDATA_GENERAL;
  datalen = sizeof(data);

  if (sysctl(name, nitems(name), &data, &datalen, NULL, 0) != 0)
    uerror("sysctl", Nothing);

  res = caml_alloc(18, 0);
  Store_field(res, 0, caml_copy_string(data.ifmd_name));
  Store_field(res, 1, Val32(data.ifmd_flags));
  Store_field(res, 2, Val32(data.ifmd_snd_len));
  Store_field(res, 3, Val32(data.ifmd_snd_maxlen));
  Store_field(res, 4, Val32(data.ifmd_snd_drops));
  Store_field(res, 5, Val32(data.ifmd_data.ifi_mtu));
  Store_field(res, 6, Val64(data.ifmd_data.ifi_baudrate));
  Store_field(res, 7, Val64(data.ifmd_data.ifi_ipackets));
  Store_field(res, 8, Val64(data.ifmd_data.ifi_ierrors));
  Store_field(res, 9, Val64(data.ifmd_data.ifi_opackets));
  Store_field(res, 10, Val64(data.ifmd_data.ifi_oerrors));
  Store_field(res, 11, Val64(data.ifmd_data.ifi_collisions));
  Store_field(res, 12, Val64(data.ifmd_data.ifi_ibytes));
  Store_field(res, 13, Val64(data.ifmd_data.ifi_obytes));
  Store_field(res, 14, Val64(data.ifmd_data.ifi_imcasts));
  Store_field(res, 15, Val64(data.ifmd_data.ifi_omcasts));
  Store_field(res, 16, Val64(data.ifmd_data.ifi_iqdrops));
  Store_field(res, 17, Val64(data.ifmd_data.ifi_oqdrops));

  CAMLreturn(res);
}
#else /* FreeBSD */

/* stub symbols for OS currently not supported */

CAMLprim value vmmanage_sysctl_kinfo_proc (value pid_r) {
  CAMLparam1(pid_r);
  uerror("sysctl_kinfo_proc", Nothing);
}

CAMLprim value vmmanage_sysctl_ifcount (value unit) {
  CAMLparam1(unit);
  uerror("sysctl_ifcount", Nothing);
}

CAMLprim value vmmanage_sysctl_ifdata (value num) {
  CAMLparam1(num);
  uerror("sysctl_ifdata", Nothing);
}

CAMLprim value vmmanage_vmmapi_open (value name) {
  CAMLparam1(name);
  uerror("vmmapi_open", Nothing);
}

CAMLprim value vmmanage_vmmapi_close (value name) {
  CAMLparam1(name);
  uerror("vmmapi_close", Nothing);
}

CAMLprim value vmmanage_vmmapi_stats (value name) {
  CAMLparam1(name);
  uerror("vmmapi_stats", Nothing);
}

CAMLprim value vmmanage_vmmapi_statnames (value name) {
  CAMLparam1(name);
  uerror("vmmapi_statnames", Nothing);
}

#endif

#ifdef __linux__
#include <asm/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

static int vmmanage_rtnetlink_send(int rtnetlink_sock, const char *ifname) {
  struct {
    struct nlmsghdr nh;
    struct ifinfomsg ifi;
    char attrbuf[512];
  } req;
  struct rtattr *rta;

  memset(&req, 0, sizeof(req));
  req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
  req.nh.nlmsg_flags = NLM_F_REQUEST;
  req.nh.nlmsg_type = RTM_GETLINK;
  req.ifi.ifi_family = AF_UNSPEC;
  rta = (struct rtattr *)((char *) &req + NLMSG_ALIGN(req.nh.nlmsg_len));
  rta->rta_type = IFLA_IFNAME;
  rta->rta_len = RTA_LENGTH(strlen(ifname));
  req.nh.nlmsg_len = NLMSG_ALIGN(req.nh.nlmsg_len) + rta->rta_len;
  memcpy(RTA_DATA(rta), ifname, strlen(ifname) + 1);
  int r;
  r = send(rtnetlink_sock, &req, req.nh.nlmsg_len, 0);
  return r != req.nh.nlmsg_len;
}

CAMLprim value vmmanage_rtnetlink_name(value name) {
  CAMLparam1(name);
  CAMLlocal1(res);
  int rtnetlink_sock;
  int r;

  rtnetlink_sock = socket(AF_NETLINK, SOCK_DGRAM, NETLINK_ROUTE);
  if (rtnetlink_sock == -1)
    uerror("socket", Nothing);
  r = vmmanage_rtnetlink_send(rtnetlink_sock, String_val(name));
  if (r) {
    uerror("rtnetlink", Nothing);
  }

  int len;
  char buf[8192];
  struct iovec iov = { buf, sizeof(buf) };
  struct sockaddr_nl sa;
  struct nlmsghdr *nh;
  struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
  len = recvmsg(rtnetlink_sock, &msg, 0);

  struct nlmsgerr *nlmsgerr;
  struct rtattr *rta;
  int *mtu = NULL;
  char *ifname = NULL;
  struct rtnl_link_stats64 *stats = NULL;
  struct ifinfomsg *ifi = NULL;

  /* XXX: I think we only ever get one message in response? */
  for (nh = (struct nlmsghdr *) buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
    switch (nh->nlmsg_type) {
      case NLMSG_ERROR:
        nlmsgerr = NLMSG_DATA(nh);
        errno = -nlmsgerr->error;
      case NLMSG_DONE:
        uerror("rtnetlink", Nothing);
        break;
      }
    if (nh->nlmsg_type == RTM_NEWLINK) {
      int attr_len;
      ifi = (struct ifinfomsg *) NLMSG_DATA(nh);
      rta = IFLA_RTA(ifi);
      attr_len = IFLA_PAYLOAD(nh);
      for (; RTA_OK(rta, attr_len); rta = RTA_NEXT(rta, attr_len)) {
        switch (rta->rta_type) {
          case IFLA_IFNAME:
            ifname = RTA_DATA(rta);
            break;
          case IFLA_STATS64:
            stats = RTA_DATA(rta);
            break;
          case IFLA_MTU:
            mtu = RTA_DATA(rta);
            break;
        }
      }
    }
  }
  if (mtu == NULL || ifname == NULL || stats == NULL || ifi == NULL)
    uerror("rtnetlink", Nothing);
  res = caml_alloc(18, 0);
  Store_field(res, 0, caml_copy_string(ifname));
  Store_field(res, 1, Val32(ifi->ifi_flags));
  Store_field(res, 2, Val32(0)); /* send_length */
  Store_field(res, 3, Val32(0)); /* max_send_length */
  Store_field(res, 4, Val32(0)); /* send_drops */
  Store_field(res, 5, Val32(*mtu));
  Store_field(res, 6, Val64(0)); /* baudrate */
  Store_field(res, 7, Val64(stats->rx_packets));
  Store_field(res, 8, Val64(stats->rx_errors));
  Store_field(res, 9, Val64(stats->tx_packets));
  Store_field(res, 10, Val64(stats->tx_errors));
  Store_field(res, 11, Val64(stats->collisions));
  Store_field(res, 12, Val64(stats->rx_bytes));
  Store_field(res, 13, Val64(stats->tx_bytes));
  Store_field(res, 14, Val64(stats->multicast)); /* input_mcast */
  Store_field(res, 15, Val64(0)); /* output_mcast */
  Store_field(res, 16, Val64(stats->rx_dropped));
  Store_field(res, 16, Val64(stats->tx_dropped));

  CAMLreturn(res);
}
#endif /* Linux */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFSIZE 8192

int main(void)
{
	struct sockaddr_nl addr;
	int sock, len;
	char buf[BUFSIZE];
	struct nlmsghdr *nlh;

	if((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE))== -1) {
		perror("couldn't open NETLINK_ROUTE socket");
		exit(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.nl_family = AF_NETLINK;
	addr.nl_groups = RTMGRP_IPV4_ROUTE | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE;
	
	if(bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
		perror("couldn't bind");
		exit(-1);
	}

	nlh = (struct nlmsghdr *)buf;
	while((len=recv(sock, nlh, BUFSIZE, 0)) > 0) {
		while((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {
			struct ifaddrmsg *ifa;
			struct rtattr *rth;
			struct rtmsg *rtMsg;
			int rtl;
			char name[IFNAMSIZ], ipstr_buf[256];

			int rtlen;

		       	ifa = (struct ifaddrmsg *)NLMSG_DATA(nlh);
			rth = IFA_RTA(ifa);
			rtl = IFA_PAYLOAD(nlh);
			rtlen = RTM_PAYLOAD(nlh);

			printf("get new message, type = (%d)\n", nlh->nlmsg_type);
			switch (nlh->nlmsg_type) {
			case RTM_NEWADDR:
				printf("new addr discovered\n");
				while(rtl && RTA_OK(rth, rtl)) {
					if(rth->rta_type == IFA_LOCAL || rth->rta_type == IFA_ADDRESS) {
						/* XXX Cliff: alias is not being handled in 'name' */
						if_indextoname(ifa->ifa_index, name);
						printf("RTM_NEWADDR: %s is now %s\n", name,
							inet_ntop(ifa->ifa_family, RTA_DATA(rth), ipstr_buf, rtlen));
					}
					rth = RTA_NEXT(rth, rtl);
				}
				break;
			case RTM_NEWROUTE:
				rtMsg = (struct rtmsg *)NLMSG_DATA(nlh);
				if( (rtMsg->rtm_table != RT_TABLE_MAIN) || !((rtMsg->rtm_family == AF_INET) || (rtMsg->rtm_family == AF_INET6)))
					break;

				printf("new route discovered\n");
				rth = (struct rtattr *)RTM_RTA(rtMsg);

				for(;RTA_OK(rth,rtlen);rth = RTA_NEXT(rth,rtl))
				{
					switch(rth->rta_type)
					{
					case RTA_OIF:
						if_indextoname(*(int *)RTA_DATA(rth), name);
						printf("RTM_NEWROUTE: device name=%s\n", name);
						break;

					case RTA_GATEWAY:
						printf("RTM_NEWROUTE: gateway=%s\n",inet_ntop (ifa->ifa_family, RTA_DATA(rth), ipstr_buf, rtlen));
						break;

					case RTA_PREFSRC:
						printf("RTM_NEWROUTE: presrc=%s\n",inet_ntop (ifa->ifa_family, RTA_DATA(rth), ipstr_buf, rtlen));
						break;

					case RTA_DST:
						printf("RTM_NEWROUTE: dst=%s\n",inet_ntop (ifa->ifa_family, RTA_DATA(rth), ipstr_buf, rtlen));
						break;
					default:
						printf("RTM_NEWROUTE: unknown rta_type=%d found\n", rth->rta_type);
						break;
					}
				}
				break;

			case RTM_DELADDR:
				printf("RTM_DELADDR: an address is deleted\n");
				break;

			case RTM_DELROUTE:
				printf("RTM_DELROUTE: a route is deleted\n");
				break;


			default:
				printf("NLMSG_TYPE %d message discovered.\n", nlh->nlmsg_type);
				break;
			}

			nlh= NLMSG_NEXT(nlh, len);
		}
	}
	return 0;
}

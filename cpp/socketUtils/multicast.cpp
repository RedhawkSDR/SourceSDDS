/*
 * This file is protected by Copyright. Please refer to the COPYRIGHT file
 * distributed with this source distribution.
 *
 * This file is part of REDHAWK rh.SourceSDDS.
 *
 * REDHAWK rh.SourceSDDS is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * REDHAWK rh.SourceSDDS is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see http://www.gnu.org/licenses/.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string>
#include "multicast.h"
#include "SourceNicUtils.h"
#include <ossie/debug.h>

static multicast_t multicast_open_ (const char* iface, const char* group, int port, std::string& chosen_iface, LOGGER _log)
{
  unsigned int ii;

  multicast_t multicast = { socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) };
  VERIFY_ERR(multicast.sock >= 0, "create socket", _log);
  int one = 1;
  VERIFY_ERR(setsockopt(multicast.sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0, "reuse address", _log);

  /* Enumerate all the devices. */
  struct ifconf devs = {0};
  devs.ifc_len = 512*sizeof(struct ifreq);
  devs.ifc_buf = (char*)malloc(devs.ifc_len);
  VERIFY_ERR(devs.ifc_buf != 0, "memory allocation", _log);
  VERIFY_ERR(ioctl(multicast.sock, SIOCGIFCONF, &devs) >= 0, "enum devices", _log);
  for (ii = 0; ii < devs.ifc_len/sizeof(struct ifreq); ii++) {
	  bool any = (!*iface);
	  bool any_interface_vlan_match = false;
	  if(*iface && iface[0] == '.'){
		  size_t len_dev = strlen(devs.ifc_req[ii].ifr_ifrn.ifrn_name);
		  size_t len_iface = strlen(iface);
		  if(len_dev >= len_iface && !strcmp(devs.ifc_req[ii].ifr_ifrn.ifrn_name + len_dev-len_iface,iface))
			  any_interface_vlan_match = true;
	  }
	  bool interface_exact_match = (strcmp(iface, devs.ifc_req[ii].ifr_ifrn.ifrn_name) == 0);
	  if (any || any_interface_vlan_match || interface_exact_match) {
		  try{
			  struct ifreq dev = devs.ifc_req[ii];
			  VERIFY_ERR(ioctl(multicast.sock, SIOCGIFFLAGS, &dev) >= 0, "get flags", _log);
			  VERIFY(dev.ifr_flags & IFF_UP, "interface up", _log);
			  VERIFY(!(dev.ifr_flags & IFF_LOOPBACK), "not loopback", _log);
			  VERIFY(dev.ifr_flags & IFF_MULTICAST, "must be multicast", _log);
			  if(any) {
				  struct ip_mreq mreqn;
				  memset(&mreqn, 0, sizeof(mreqn));
				  VERIFY_ERR(inet_aton(group, &mreqn.imr_multiaddr), "convert string to group", _log);
				  mreqn.imr_interface.s_addr =  (INADDR_ANY);
				  VERIFY_ERR(setsockopt(multicast.sock, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof(struct ip_mreq)) == 0, "set device", _log);
				  multicast.addr.sin_family = AF_INET;
				  multicast.addr.sin_addr.s_addr = htonl(INADDR_ANY);
				  multicast.addr.sin_port = htons(port);
				  VERIFY_ERR(bind(multicast.sock, (struct sockaddr*)&multicast.addr, sizeof(struct sockaddr_in)) == 0, "socket bind", _log);
				  if (!((mreqn.imr_multiaddr.s_addr & 0x000000FF) < 224) || ((mreqn.imr_multiaddr.s_addr & 0x000000FF) > 239)) {
					  VERIFY_ERR(setsockopt(multicast.sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(struct ip_mreq)) == 0, "igmp join", _log);
				  }
			  }
			  else {
				  VERIFY_ERR(ioctl(multicast.sock, SIOCGIFINDEX, &dev) == 0, "get index", _log);
				  struct ip_mreqn mreqn;
				  memset(&mreqn, 0, sizeof(mreqn));
				  VERIFY_ERR(inet_aton(group, &mreqn.imr_multiaddr), "convert string to group", _log);
				  mreqn.imr_address = ((struct sockaddr_in*)(&dev.ifr_addr))->sin_addr;
				  mreqn.imr_ifindex = dev.ifr_ifindex;
				  VERIFY_ERR(setsockopt(multicast.sock, IPPROTO_IP, IP_MULTICAST_IF, &mreqn, sizeof(struct ip_mreqn)) == 0, "set device", _log);
				  multicast.addr.sin_family = AF_INET;
				  multicast.addr.sin_addr.s_addr = mreqn.imr_multiaddr.s_addr;
				  multicast.addr.sin_port = htons(port);
				  VERIFY_ERR(bind(multicast.sock, (struct sockaddr*)&multicast.addr, sizeof(struct sockaddr_in)) == 0, "socket bind", _log);
				  if (!((multicast.addr.sin_addr.s_addr & 0x000000FF) < 224) || ((multicast.addr.sin_addr.s_addr & 0x000000FF) > 239)) {
					  VERIFY_ERR(setsockopt(multicast.sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreqn, sizeof(struct ip_mreqn)) == 0, "igmp join", _log);
				  }
			  }

			  char* vlan = strchr(dev.ifr_ifrn.ifrn_name, '.');
			  if (vlan==NULL)
			      chosen_iface = dev.ifr_ifrn.ifrn_name;
			  else
			      chosen_iface.assign(&dev.ifr_ifrn.ifrn_name[0], vlan-&dev.ifr_ifrn.ifrn_name[0]);

			  free(devs.ifc_buf);
			  return multicast;
		  }catch(...){};
	  }
  }

  RH_WARN(_log, "Could not find the interface requested, closing socket");

  /* If we get here, we've failed. */
  close(multicast.sock);
  free(devs.ifc_buf);
  multicast.sock = -1;

  return multicast;
}


multicast_t multicast_client (const char* iface, const char* group, int port, std::string& chosen_iface, LOGGER _log) throw (BadParameterError)
{
  if (!_log) {
    _log = rh_logger::Logger::getLogger("SourceSDDS_utils");
    RH_DEBUG(_log, "multicast_client method passed null logger; creating logger "<<_log->getName());
  } else {
    RH_DEBUG(_log, "multicast_client method passed valid logger "<<_log->getName());
  }
  multicast_t client = multicast_open_(iface, group, port, chosen_iface, _log);
  return client;
}


ssize_t multicast_receive (multicast_t client, void* buffer, size_t bytes)
{
  return recv(client.sock, buffer, bytes, 0);
}


multicast_t multicast_server (const char* iface, const char* group, int port, std::string& chosen_iface, LOGGER _log)
{
  if (!_log) {
    _log = rh_logger::Logger::getLogger("SourceSDDS_utils");
    RH_DEBUG(_log, "multicast_server method passed null logger; creating logger "<<_log->getName());
  } else {
    RH_DEBUG(_log, "multicast_server method passed valid logger "<<_log->getName());
  }
  multicast_t server = multicast_open_(iface, group, port, chosen_iface, _log);
  if (server.sock != -1) {
    uint8_t ttl = 32;
    VERIFY_ERR(setsockopt(server.sock, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) == 0, "set ttl", _log);
  }
  return server;
}


ssize_t multicast_transmit (multicast_t server, const void* buffer, size_t bytes)
{
  return sendto(server.sock, buffer, bytes, 0, (struct sockaddr*)&server.addr, sizeof(server.addr));
}


void multicast_close (multicast_t socket)
{
  close(socket.sock);
}


int multicast_poll_in (multicast_t client, int timeout)
{
  struct pollfd pfd;
  pfd.fd = client.sock;
  pfd.events = POLLIN;
  return poll(&pfd, 1, timeout);
}

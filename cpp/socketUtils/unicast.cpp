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
#include <iostream>
#include "unicast.h"
#include <ossie/debug.h>
#include <errno.h>

/* it is probably desirable to convert to C++ and throw exceptions instead. */
static inline void verify_ (int condition, const char* message, const char* condtext, const char* file, int line, LOGGER _log) {
    if (!condition) {
	  int errsv = errno;
	  std::stringstream ss;
	  ss << "unicast: Verify failed [" << errsv << ":" << strerror(errsv) << "] " << file << " at line " << line << ": " << message << " (" << condtext << ")";
	RH_ERROR(_log, ss.str());
    throw(BadParameterError3(ss.str()));
  }
}
#define verify(CONDITION, MESSAGE, LOGG) verify_(CONDITION, MESSAGE, #CONDITION, __FILE__, __LINE__, LOGG)

/* it is probably desirable to convert to C++ and throw exceptions instead. */
static inline void verify_warn_ (int condition, const char* message, const char* condtext, const char* file, int line, LOGGER _log) {
  if (!condition) {
	  int errsv = errno;
	  std::stringstream ss;
	  ss << "unicast: Verify failed [" << errsv << ":" << strerror(errsv) << "] " << file << " at line " << line << ": " << message << " (" << condtext << ")";
	RH_WARN(_log, ss.str());
    throw(BadParameterError3(ss.str()));
  }
}
#define verify_warn(CONDITION, MESSAGE, LOGG) verify_warn_(CONDITION, MESSAGE, #CONDITION, __FILE__, __LINE__, LOGG)

/* it is probably desirable to convert to C++ and throw exceptions instead. */
static inline void verify_info_ (int condition, const char* message, const char* condtext, const char* file, int line, LOGGER _log) {
  if (!condition) {
	  int errsv = errno;
	  std::stringstream ss;
	  ss << "unicast: Verify failed [" << errsv << ":" << strerror(errsv) << "] " << file << " at line " << line << ": " << message << " (" << condtext << ")";
	RH_INFO(_log, ss.str());
    throw(BadParameterError3(ss.str()));
  }
}
#define verify_info(CONDITION, MESSAGE, LOGG) verify_info_(CONDITION, MESSAGE, #CONDITION, __FILE__, __LINE__, LOGG)

/* it is probably desirable to convert to C++ and throw exceptions instead. */
static inline void verify_debug_ (int condition, const char* message, const char* condtext, const char* file, int line, LOGGER _log) {
  if (!condition) {
	  int errsv = errno;
	  std::stringstream ss;
	  ss << "unicast: Verify failed [" << errsv << ":" << strerror(errsv) << "] " << file << " at line " << line << ": " << message << " (" << condtext << ")";
	RH_DEBUG(_log, ss.str());
    throw(BadParameterError3(ss.str()));
  }
}
#define verify_debug(CONDITION, MESSAGE, LOGG) verify_debug_(CONDITION, MESSAGE, #CONDITION, __FILE__, __LINE__, LOGG)

static unicast_t unicast_open_ (const char* iface, const char* ip, int port, std::string& chosen_iface, LOGGER _log)
{
  unsigned int ii;

  unicast_t unicast = { socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP) };
  verify(unicast.sock >= 0, "unicast_open_: create socket", _log);
  int one = 1;
  verify(setsockopt(unicast.sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) == 0, "unicast_open_: reuse address", _log);

  /* Enumerate all the devices. */
  struct ifconf devs = {0};
  devs.ifc_len = 512*sizeof(struct ifreq);
  devs.ifc_buf = (char*)malloc(devs.ifc_len);
  verify(devs.ifc_buf != 0, "unicast_open_: memory allocation", _log);
  verify(ioctl(unicast.sock, SIOCGIFCONF, &devs) >= 0, "unicast_open_: enum devices", _log);

  bool any = (!*iface);
  bool any_ip = (inet_addr(ip) == 0);
  for (ii = 0; ii<devs.ifc_len/sizeof(struct ifreq); ii++) {
	  bool any_interface_vlan_match = false;
	  if(*iface && iface[0] == '.'){
		  size_t len_dev = strlen(devs.ifc_req[ii].ifr_ifrn.ifrn_name);
		  size_t len_iface = strlen(iface);
		  if(len_dev >= len_iface && !strcmp(devs.ifc_req[ii].ifr_ifrn.ifrn_name + len_dev-len_iface,iface))
			  any_interface_vlan_match = true;
	  }
	  bool ip_interface_match = any_ip || (inet_addr(ip) == ((struct sockaddr_in*)(&devs.ifc_req[ii].ifr_addr))->sin_addr.s_addr);
	  bool interface_exact_match = (strcmp(iface, devs.ifc_req[ii].ifr_ifrn.ifrn_name) == 0);
	  RH_DEBUG(_log, "unicast_open_: "<<ii<<": "<<devs.ifc_req[ii].ifr_ifrn.ifrn_name<<"  "<<ip<<":"<<port<<"  any="<<any<<" any+vlan="<<any_interface_vlan_match<<" ip="<<ip_interface_match<<" exact="<<interface_exact_match);

	  // If device meets IP address/interface/vlan requirements, try to bind
	  if (ip_interface_match && (any || any_interface_vlan_match || interface_exact_match)) {
		  try{
			  // We found an interface that meets IP/VLAN/interface specified
			  // Now make sure it meets all other requirements
			  struct ifreq dev = devs.ifc_req[ii];
			  verify_info(ioctl(unicast.sock, SIOCGIFFLAGS, &dev) >= 0, "unicast_open_: get flags", _log);
			  verify_info(dev.ifr_flags & IFF_UP, "unicast_open_: interface up", _log);
			  //verify_info(!(dev.ifr_flags & IFF_LOOPBACK), "unicast_open_: not loopback", _log);
			  verify_info(ioctl(unicast.sock, SIOCGIFINDEX, &dev) == 0, "unicast_open_: get index", _log);

			  // Now we bind...
			  unicast.addr.sin_family = AF_INET;
			  unicast.addr.sin_addr.s_addr = inet_addr(ip);//mreqn.imr_multiaddr.s_addr;
			  unicast.addr.sin_port = htons(port);
			  verify_info(bind(unicast.sock, (struct sockaddr*)&unicast.addr, sizeof(struct sockaddr)) == 0, "unicast_open_: bind", _log);

			  // Success! report back interface name
			  char* vlan = strchr(dev.ifr_ifrn.ifrn_name, '.');
			  if (any && any_ip)
				  chosen_iface = "ALL";
			  else if (vlan==NULL)
			      chosen_iface = dev.ifr_ifrn.ifrn_name;
			  else
			      chosen_iface.assign(&dev.ifr_ifrn.ifrn_name[0], vlan-&dev.ifr_ifrn.ifrn_name[0]);

			  free(devs.ifc_buf);
			  return unicast;
		  }catch(...){};
	  }
  }
  RH_WARN(_log, "unicast_open_: Could not find the interface requested, closing socket");

  /* If we get here, we've failed. */
  close(unicast.sock);
  free(devs.ifc_buf);
  unicast.sock = -1;

  return unicast;
}


unicast_t unicast_client (const char* iface, const char* group, int port, std::string& chosen_iface, LOGGER _log) throw (BadParameterError)
{
  if (!_log) {
    _log = rh_logger::Logger::getLogger("SourceSDDS_utils");
    RH_DEBUG(_log, "unicast_client method passed null logger; creating logger "<<_log->getName());
  } else {
    RH_DEBUG(_log, "unicast_client method passed valid logger "<<_log->getName());
  }
  unicast_t client = unicast_open_(iface, group, port, chosen_iface, _log);
  return client;
}


ssize_t unicast_receive (unicast_t client, void* buffer, size_t bytes, unsigned int to_in_msecs)
{
  size_t bytes_read = 0;
  int flags = unicast_poll_in(client, to_in_msecs); //0 is MSG_DONTWAIT
  if (flags)
  	bytes_read = recv(client.sock, buffer, bytes, flags);
  return bytes_read;
}


unicast_t unicast_server (const char* iface, const char* group, int port, std::string& chosen_iface, LOGGER _log)
{
  if (!_log) {
    _log = rh_logger::Logger::getLogger("SourceSDDS_utils");
    RH_DEBUG(_log, "unicast_server method passed null logger; creating logger "<<_log->getName());
  } else {
    RH_DEBUG(_log, "unicast_server method passed valid logger "<<_log->getName());
  }
  unicast_t server = unicast_open_(iface, group, port, chosen_iface, _log);
  if (server.sock != -1) {
    uint8_t ttl = 32;
    verify(setsockopt(server.sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0, "set ttl", _log);
  }
  return server;
}


ssize_t unicast_transmit (unicast_t server, const void* buffer, size_t bytes)
{
  return sendto(server.sock, buffer, bytes, 0, (struct sockaddr*)&server.addr, sizeof(server.addr));
}


void unicast_close (unicast_t socket)
{
  close(socket.sock);
}


int unicast_poll_in (unicast_t client, int to_in_msecs)
{
  int _flags = 0;
  if( to_in_msecs > 0){
  	struct pollfd pfd;
  	pfd.fd = client.sock;
  	pfd.events = POLLIN;
  	int rval = poll(&pfd, 1, (int)to_in_msecs);

	if (rval > 0)
		_flags = MSG_DONTWAIT;
	else if( rval < 0)
	{
		throw BadParameterError3("Error configuring poll");
	}
	else
	{
	   //use the val returned
	}
  }
  else if( to_in_msecs == 0){
	  _flags = MSG_DONTWAIT;
  }
  else
  {
	_flags = MSG_WAITALL;
  }
  return _flags;
}

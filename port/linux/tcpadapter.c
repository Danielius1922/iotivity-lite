/****************************************************************************
 *
 * Copyright 2018 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"),
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/

#include "tcpadapter.h"
#include "ipadapter.h"
#include "ipcontext.h"
#include "tcpsession.h"
#include "port/oc_assert.h"
#include "port/oc_log_internal.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef OC_TCP

#define OC_TCP_LISTEN_BACKLOG 3

static int
configure_tcp_socket(int sock, struct sockaddr_storage *sock_info)
{
  if (bind(sock, (struct sockaddr *)sock_info, sizeof(*sock_info)) == -1) {
    OC_ERR("binding socket %d", errno);
    return -1;
  }
  if (listen(sock, OC_TCP_LISTEN_BACKLOG) == -1) {
    OC_ERR("listening socket %d", errno);
    return -1;
  }
  return 0;
}

static int
get_assigned_tcp_port(int sock, struct sockaddr_storage *sock_info)
{

  socklen_t socklen = sizeof(*sock_info);
  if (getsockname(sock, (struct sockaddr *)sock_info, &socklen) == -1) {
    OC_ERR("obtaining socket information %d", errno);
    return -1;
  }
  return 0;
}

static int
tcp_create_socket(int domain, struct sockaddr_storage *sock_info)
{
  int sock = socket(domain, SOCK_STREAM, IPPROTO_TCP);
  if (sock < 0) {
    OC_ERR("failed to create TCP socket");
    return -1;
  }

  if (configure_tcp_socket(sock, sock_info) < 0) {
    OC_ERR("set socket option in socket");
    close(sock);
    return -1;
  }

  if (get_assigned_tcp_port(sock, sock_info) < 0) {
    OC_ERR("get port for socket");
    close(sock);
    return -1;
  }
  return sock;
}

#ifdef OC_IPV4
static void
tcp_ipv4_addr_init(struct sockaddr_storage *addr)
{
  memset(addr, 0, sizeof(struct sockaddr_storage));
  struct sockaddr_in *l = (struct sockaddr_in *)addr;
  l->sin_family = AF_INET;
  l->sin_addr.s_addr = INADDR_ANY;
  l->sin_port = 0;
}

static int
tcp_connectivity_ipv4_init(ip_context_t *dev)
{
  OC_DBG("Initializing TCP adapter IPv4 for device %zd", dev->device);

  tcp_ipv4_addr_init(&dev->tcp.server4);
#ifdef OC_SECURITY
  tcp_ipv4_addr_init(&dev->tcp.secure4);
#endif /* OC_SECURITY */

  dev->tcp.server4_sock = tcp_create_socket(AF_INET, &dev->tcp.server4);
  if (dev->tcp.server4_sock < 0) {
    OC_ERR("failed to create TCP IPv4 server socket");
    return -1;
  }
  OC_DBG("created tcp server4_sock (fd=%d)", dev->tcp.server4_sock);
  dev->tcp.port4 = ntohs(((struct sockaddr_in *)&dev->tcp.server4)->sin_port);

#ifdef OC_SECURITY
  dev->tcp.secure4_sock = tcp_create_socket(AF_INET, &dev->tcp.secure4);
  if (dev->tcp.secure4_sock < 0) {
    OC_ERR("failed to create TCP IPv4 secure socket");
    return -1;
  }
  OC_DBG("created tcp secure4_sock (fd=%d)", dev->tcp.secure4_sock);
  dev->tcp.tls4_port =
    ntohs(((struct sockaddr_in *)&dev->tcp.secure4)->sin_port);
#endif /* OC_SECURITY */

  OC_DBG("Successfully initialized TCP adapter IPv4 for device %zd",
         dev->device);

  return 0;
}
#endif /* OC_IPV4 */

static void
tcp_addr_init(struct sockaddr_storage *addr)
{
  memset(addr, 0, sizeof(struct sockaddr_storage));
  struct sockaddr_in6 *l = (struct sockaddr_in6 *)addr;
  l->sin6_family = AF_INET6;
  l->sin6_addr = in6addr_any;
  l->sin6_port = 0;
}

int
tcp_connectivity_init(ip_context_t *dev)
{
  OC_DBG("Initializing TCP adapter for device %zd", dev->device);

  tcp_addr_init(&dev->tcp.server);
#ifdef OC_SECURITY
  tcp_addr_init(&dev->tcp.secure);
#endif /* OC_SECURITY */

  dev->tcp.server_sock = tcp_create_socket(AF_INET6, &dev->tcp.server);
  if (dev->tcp.server_sock < 0) {
    OC_ERR("failed to create TCP IPv6 server socket");
    return -1;
  }
  OC_DBG("created tcp server_sock (fd=%d)", dev->tcp.server_sock);
  dev->tcp.port = ntohs(((struct sockaddr_in *)&dev->tcp.server)->sin_port);

#ifdef OC_SECURITY
  dev->tcp.secure_sock = tcp_create_socket(AF_INET6, &dev->tcp.secure);
  OC_DBG("created tcp secure_sock (fd=%d)", dev->tcp.secure_sock);
  if (dev->tcp.secure_sock < 0) {
    OC_ERR("failed to create TCP IPv6 secure socket");
    return -1;
  }
  dev->tcp.tls_port = ntohs(((struct sockaddr_in *)&dev->tcp.secure)->sin_port);
#endif /* OC_SECURITY */

#ifdef OC_IPV4
  if (tcp_connectivity_ipv4_init(dev) != 0) {
    OC_ERR("Could not initialize IPv4 for TCP");
  }
#endif /* OC_IPV4 */

  if (pthread_mutex_init(&dev->tcp.cfds_mutex, NULL) != 0) {
    oc_abort("error initializing TCP connection mutex");
  }
  FD_ZERO(&dev->tcp.cfds);

  if (pipe(dev->tcp.connect_pipe) < 0) {
    OC_ERR("Could not initialize connection pipe");
    return -1;
  }
  if (oc_set_fd_flags(dev->tcp.connect_pipe[0], O_NONBLOCK, 0) < 0) {
    OC_ERR("Could not set non-blocking connect_pipe[0]");
    return -1;
  }

  OC_DBG("=======tcp port info.========");
  OC_DBG("  ipv6 port   : %u", dev->tcp.port);
#ifdef OC_SECURITY
  OC_DBG("  ipv6 secure : %u", dev->tcp.tls_port);
#endif /* OC_SECURITY */
#ifdef OC_IPV4
  OC_DBG("  ipv4 port   : %u", dev->tcp.port4);
#ifdef OC_SECURITY
  OC_DBG("  ipv4 secure : %u", dev->tcp.tls4_port);
#endif /* OC_SECURITY */
#endif /* OC_IPV4 */

  OC_DBG("Successfully initialized TCP adapter for device %zd", dev->device);
  return 0;
}

void
tcp_connectivity_shutdown(ip_context_t *dev)
{
  close(dev->tcp.server_sock);
#ifdef OC_IPV4
  close(dev->tcp.server4_sock);
#endif /* OC_IPV4 */

#ifdef OC_SECURITY
  close(dev->tcp.secure_sock);
#ifdef OC_IPV4
  close(dev->tcp.secure4_sock);
#endif /* OC_IPV4 */
#endif /* OC_SECURITY */

  close(dev->tcp.connect_pipe[0]);
  close(dev->tcp.connect_pipe[1]);

  tcp_session_shutdown(dev);

  pthread_mutex_destroy(&dev->tcp.cfds_mutex);
  OC_DBG("tcp_connectivity_shutdown for device %zd", dev->device);
}

void
tcp_add_socks_to_rfd_set(ip_context_t *dev)
{
  FD_SET(dev->tcp.server_sock, &dev->rfds);
#ifdef OC_SECURITY
  FD_SET(dev->tcp.secure_sock, &dev->rfds);
#endif /* OC_SECURITY */

#ifdef OC_IPV4
  FD_SET(dev->tcp.server4_sock, &dev->rfds);
#ifdef OC_SECURITY
  FD_SET(dev->tcp.secure4_sock, &dev->rfds);
#endif /* OC_SECURITY */
#endif /* OC_IPV4 */
  FD_SET(dev->tcp.connect_pipe[0], &dev->rfds);
}

static adapter_receive_state_t
tcp_receive_signal_message(const tcp_context_t *dev)
{
  char data[32];
  do {
    ssize_t len = read(dev->connect_pipe[0], data, sizeof(data));
    if (len < 0) {
      if (errno == EINTR) {
        continue;
      }
      OC_ERR("read error! %d", (int)errno);
      return ADAPTER_STATUS_ERROR;
    }
    return ADAPTER_STATUS_RECEIVE;
  } while (true);
}

adapter_receive_state_t
tcp_receive_signal(const tcp_context_t *dev)
{
  tcp_session_handle_signal();
  return tcp_receive_signal_message(dev);
}

#endif /* OC_TCP */

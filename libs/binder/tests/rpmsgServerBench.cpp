/*
 * Copyright (C) 2023 Xiaomi Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <netpacket/rpmsg.h>
#include <nuttx/config.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <syslog.h>
#include <unistd.h>

struct rpsock_arg_s {
    int fd;
    bool nonblock;
    bool skippoll;
};

static void* rpsock_thread(pthread_addr_t pvarg)
{
    struct rpsock_arg_s* args = (struct rpsock_arg_s*)pvarg;
    struct pollfd pfd;
    char buf[255];
    ssize_t ret;
    while (1) {
        if (args->nonblock && !args->skippoll) {
            memset(&pfd, 0, sizeof(struct pollfd));
            pfd.fd = args->fd;
            pfd.events = POLLIN;
            ret = poll(&pfd, 1, -1);
            if (ret < 0) {
                syslog(LOG_ERR, "[server] poll failed errno %d\n", errno);
                break;
            }
        }
        args->skippoll = false;
        ret = recv(args->fd, buf, sizeof(buf), 0);
        if (ret == 0 || (ret < 0 && errno == ECONNRESET)) {
            // recv data normal exit. Calling syslog can be a performance drain, so we don't perform a call to do this
            break;
        } else if (ret < 0 && errno == EAGAIN) {
            // just Call again. Calling syslog can be a performance drain, so we don't perform a call to do this
            continue;
        } else if (ret < 0) {
            syslog(LOG_ERR, "[server] recv data failed ret %d, errno %d\n", ret, errno);
            break;
        }
        int snd = ret;
        char* tmp = buf;
        while (snd > 0) {
            if (args->nonblock) {
                memset(&pfd, 0, sizeof(struct pollfd));
                pfd.fd = args->fd;
                pfd.events = POLLOUT;
                ret = poll(&pfd, 1, -1);
                if (ret < 0) {
                    syslog(LOG_ERR, "[server] poll failure: %d\n", errno);
                    break;
                }
            }
            ret = send(args->fd, tmp, snd, 0);
            if (ret > 0) {
                tmp += ret;
                snd -= ret;
            } else if (ret == 0) {
                // Send data normal exit. Calling syslog can be a performance drain, so we don't perform a call to do this
                break;
            } else if (errno == EAGAIN) {
                // just Call again. Calling syslog can be a performance drain, so we don't perform a call to do this
            } else {
                syslog(LOG_ERR, "[server] send data failed errno %d\n", errno);
                break;
            }
        }
    }

    free(args);
    return NULL;
}

static int rpsock_stream_server(void)
{
    bool nonblock = true;

    int listensd = socket(PF_RPMSG, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (listensd < 0) {
        syslog(LOG_ERR, "[server] socket failure: %d\n", errno);
        return -1;
    }
    /* Bind the socket to a local address */
    struct sockaddr_rpmsg myaddr;
    myaddr.rp_family = AF_RPMSG;
    strncpy(myaddr.rp_name, "cpc", RPMSG_SOCKET_NAME_SIZE);
    myaddr.rp_name[RPMSG_SOCKET_NAME_SIZE - 1] = '\0';
    myaddr.rp_cpu[0] = '\0';
    int ret = bind(listensd, (struct sockaddr*)&myaddr, sizeof(myaddr));
    if (ret < 0) {
        close(listensd);
        return ret;
    }
    /* Listen for connections on the bound socket */
    ret = listen(listensd, 16);
    if (ret < 0) {
        close(listensd);
        return ret;
    }
    while (1) {
        if (nonblock) {
            struct pollfd pfd;
            memset(&pfd, 0, sizeof(struct pollfd));
            pfd.fd = listensd;
            pfd.events = POLLIN;
            ret = poll(&pfd, 1, -1);
            if (ret < 0) {
                syslog(LOG_ERR, "[server] poll failure: %d\n", errno);
                break;
            }
        }

        socklen_t addrlen;
        int newfd = accept(listensd, (struct sockaddr*)&myaddr, &addrlen);
        if (newfd < 0) {
            ret = newfd;
            break;
        }
        struct rpsock_arg_s* args = (struct rpsock_arg_s*)malloc(sizeof(struct rpsock_arg_s));
        if (!args) {
            ret = errno;
            break;
        }
        args->fd = newfd;
        args->nonblock = nonblock;
        args->skippoll = false;
        pthread_t thread;
        pthread_create(&thread, NULL, rpsock_thread, (pthread_addr_t)args);
        pthread_detach(thread);
    }

    close(listensd);
    return ret;
}

extern "C" int main(int argc, char* argv[])
{
    return rpsock_stream_server();
}

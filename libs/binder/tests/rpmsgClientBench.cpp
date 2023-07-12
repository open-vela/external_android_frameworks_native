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

#include <benchmark/benchmark.h>
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

static struct pollfd pfd;
static int sockfd;

void transData(int bufsize, char* inbuf, char* outbuf)
{
    bool nonblock = true;
    size_t sendsize = 0;
    int ret;
    char* tmp = outbuf;
    int snd = bufsize;

    while (snd > 0) {
        if (nonblock) {
            memset(&pfd, 0, sizeof(struct pollfd));
            pfd.fd = sockfd;
            pfd.events = POLLOUT;
            ret = poll(&pfd, 1, -1);
            if (ret < 0) {
                syslog(LOG_ERR, "[client] poll failure: %d\n", errno);
                break;
            }
        }

        ret = send(sockfd, tmp, snd, 0);
        if (ret < 0) {
            continue;
        }
        sendsize += ret;
        tmp += ret;
        snd -= ret;
    }

    size_t recvsize = 0;
    tmp = inbuf;
    while (1) {
        if (nonblock) {
            memset(&pfd, 0, sizeof(struct pollfd));
            pfd.fd = sockfd;
            pfd.events = POLLIN;
            ret = poll(&pfd, 1, -1);
            if (ret < 0) {
                syslog(LOG_ERR, "[client] poll failure: %d\n", errno);
                break;
            }
        }

        ssize_t act = recv(sockfd, tmp, bufsize, 0);
        if (act == -EAGAIN) {
            continue;
        } else if (act < 0) {
            syslog(LOG_ERR, "[client] recv data failed %d\n", act);
        }
        recvsize += act;
        tmp += act;
        if (recvsize >= sendsize) {
            break;
        }
    }
}

static int rpsock_stream_client()
{
    sockfd = socket(PF_RPMSG, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "[client] socket failure %d\n", errno);
        return 0;
    }

    /* Connect the socket to the server */
    struct sockaddr_rpmsg myaddr;
    myaddr.rp_family = AF_RPMSG;
    strncpy(myaddr.rp_name, "cpc", RPMSG_SOCKET_NAME_SIZE);
    myaddr.rp_name[RPMSG_SOCKET_NAME_SIZE - 1] = '\0';
    strncpy(myaddr.rp_cpu, "ap", RPMSG_SOCKET_CPU_SIZE);
    myaddr.rp_cpu[RPMSG_SOCKET_CPU_SIZE - 1] = '\0';

    int ret = connect(sockfd, (struct sockaddr*)&myaddr, sizeof(myaddr));
    if (ret < 0 && errno == EINPROGRESS) {
        memset(&pfd, 0, sizeof(struct pollfd));
        pfd.fd = sockfd;
        pfd.events = POLLOUT;
        ret = poll(&pfd, 1, -1);
        if (ret < 0) {
            syslog(LOG_ERR, "[client] poll failure: %d\n", errno);
        }
    } else if (ret < 0) {
        syslog(LOG_ERR, "[client] connect failure: %d\n", errno);
    }

    return -errno;
}

void BM_throughputSendAndRecv(benchmark::State& state)
{
    int64_t bufsize = int64_t(state.range(0));
    char* outbuf = (char*)malloc(bufsize);
    memset(outbuf, 'A', bufsize);
    char* inbuf = (char*)malloc(bufsize);

    while (state.KeepRunning()) {
        transData(bufsize, inbuf, outbuf);
    }

    free(outbuf);
    free(inbuf);
}
BENCHMARK(BM_throughputSendAndRecv)
    ->ArgsProduct({ { 64, 128, 256, 512, 1024, 2048, 4096, 8182 } });

extern "C" int main(int argc, char** argv)
{
    ::benchmark::Initialize(&argc, argv);

    rpsock_stream_client();

    ::benchmark::RunSpecifiedBenchmarks();

    return 0;
}

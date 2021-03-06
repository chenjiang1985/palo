// Modifications copyright (C) 2017, Baidu.com, Inc.
// Copyright 2017 The Apache Software Foundation

// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include "util/network_util.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <limits.h>
#include <ifaddrs.h>

#include <sstream>

#include <boost/foreach.hpp>

namespace palo {

static const std::string LOCALHOST("127.0.0.1");

Status get_hostname(std::string* hostname) {
    char name[HOST_NAME_MAX];
    int ret = gethostname(name, HOST_NAME_MAX);

    if (ret != 0) {
        std::stringstream ss;
        ss << "Could not get hostname: errno: " << errno;
        return Status(ss.str());
    }

    *hostname = std::string(name);
    return Status::OK;
}

Status hostname_to_ip_addrs(const std::string& name, std::vector<std::string>* addresses) {
    addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4 addresses only
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* addr_info;

    if (getaddrinfo(name.c_str(), NULL, &hints, &addr_info) != 0) {
        std::stringstream ss;
        ss << "Could not find IPv4 address for: " << name;
        return Status(ss.str());
    }

    addrinfo* it = addr_info;

    while (it != NULL) {
        char addr_buf[64];
        const char* result =
            inet_ntop(AF_INET, &((sockaddr_in*)it->ai_addr)->sin_addr, addr_buf, 64);

        if (result == NULL) {
            std::stringstream ss;
            ss << "Could not convert IPv4 address for: " << name;
            freeaddrinfo(addr_info);
            return Status(ss.str());
        }

        addresses->push_back(std::string(addr_buf));
        it = it->ai_next;
    }

    freeaddrinfo(addr_info);
    return Status::OK;
}

bool find_first_non_localhost(const std::vector<std::string>& addresses, std::string* addr) {
    BOOST_FOREACH(const std::string & candidate, addresses) {
        if (candidate != LOCALHOST) {
            *addr = candidate;
            return true;
        }
    }

    return false;
}

Status get_local_ip(std::string* local_ip) {
    ifaddrs* if_addrs = nullptr;
    if (getifaddrs(&if_addrs)) {
        std::stringstream ss;
        char buf[64];
        ss << "getifaddrs failed because " << strerror_r(errno, buf, sizeof(buf));
        return Status(ss.str());
    }

    for (ifaddrs* if_addr = if_addrs; if_addr != nullptr; if_addr = if_addr->ifa_next) {
        if (!if_addr->ifa_addr) {
            continue;
        }
        if (if_addr->ifa_addr->sa_family == AF_INET) { // check it is IP4
            // is a valid IP4 Address
            void* tmp_addr = &((struct sockaddr_in *)if_addr->ifa_addr)->sin_addr;
            char addr_buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, tmp_addr, addr_buf, INET_ADDRSTRLEN);
            if (LOCALHOST == addr_buf) {
                continue;
            }
            local_ip->assign(addr_buf);
            break;
        } else if (if_addr->ifa_addr->sa_family == AF_INET6) { // check it is IP6
            // is a valid IP6 Address
            void* tmp_addr = &((struct sockaddr_in6 *)if_addr->ifa_addr)->sin6_addr;
            char addr_buf[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, tmp_addr, addr_buf, sizeof(addr_buf));
            local_ip->assign(addr_buf);
            break;
        }
    }

    if (if_addrs != nullptr) {
        freeifaddrs(if_addrs);
    }

    return Status::OK;
}

TNetworkAddress make_network_address(const std::string& hostname, int port) {
    TNetworkAddress ret;
    ret.__set_hostname(hostname);
    ret.__set_port(port);
    return ret;
}

}

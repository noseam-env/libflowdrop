/*
 * This file is part of libflowdrop.
 *
 * For license and copyright information please follow this link:
 * https://github.com/noseam-env/libflowdrop/blob/master/LEGAL
 */

#include "flowdrop/flowdrop.hpp"
#include <set>
#include "knot/dnssd.h"
#include "discovery.hpp"
#include "specification.hpp"
#include "curl/curl.h"
#include "logger.h"

size_t curl_write_function(void *contents, size_t size, size_t nmemb, std::string *output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char *>(contents), totalSize);
    return totalSize;
}

void discovery::announce(const std::string &id, int port, const std::function<bool()> &isStopped) {
    std::unordered_map<std::string, std::string> txt;
    txt["v"] = std::to_string(flowdrop_version);
    registerService(id.c_str(), flowdrop_reg_type, flowdrop_dns_domain, port, txt, isStopped);
}

discovery::IPType convert(IPType ipType) {
    return ipType == IPv6 ? discovery::IPv6 : discovery::IPv4;
}

void discovery::resolveAndQuery(const std::string &id, const resolveCallback &callback) {
    resolveService(id.c_str(), flowdrop_reg_type, flowdrop_dns_domain, [callback](const std::optional<ResolveReply> &replyOpt) {
        if (!replyOpt.has_value()) {
            callback(std::nullopt);
            return;
        }
        const ResolveReply &reply = replyOpt.value();
        std::unordered_map<std::string, std::string> txt = reply.txt;
        if (txt[flowdrop_txt_key_version] != std::to_string(flowdrop_version)) {
            callback(std::nullopt);
            return;
        }
        unsigned short port = reply.port;
        if (reply.ip.has_value()) {
            const IPAddress &ip = reply.ip.value();
            callback({{convert(ip.type), ip.value, port}});
            return;
        }
        if (!reply.hostName.has_value()) {
            throw std::runtime_error("ip and hostName cannot be irrelevant at the same time");
        }
        const char *hostName = reply.hostName.value().c_str();
        //bool fullyResolved = false;
        // ipv6 not working for unknown reasons
        queryIPv4Address(hostName, [&callback, &port](const std::optional<IPAddress> &ipOpt){
            if (!ipOpt.has_value()) {
                callback(std::nullopt);
                return;
            }
            const IPAddress &ip = ipOpt.value();
            callback({{convert(ip.type), ip.value, port}});
        });
    });
}

void flowdrop::discover(const flowdrop::discoverCallback &callback, const std::function<bool()> &isStopped) {
    std::set<std::string> foundServices;

    findService(flowdrop_reg_type, flowdrop_dns_domain, [callback, &foundServices](const FindReply &findReply) {
        auto it = foundServices.find(findReply.serviceName);
        if (it != foundServices.end()) {
            return;
        }
        foundServices.insert(findReply.serviceName);

        Logger::log(Logger::LEVEL_DEBUG, "found: " + std::string(findReply.serviceName) + " " + findReply.regType + " " + findReply.replyDomain);

        discovery::resolveAndQuery(findReply.serviceName, [callback](const std::optional<discovery::Remote> &remoteOpt) {
            if (!remoteOpt.has_value()) return;
            const discovery::Remote &remote = remoteOpt.value();
            Logger::log(Logger::LEVEL_DEBUG, "fully resolved: " + remote.ip + " " + std::to_string(remote.port));

            curl_global_init(CURL_GLOBAL_NOTHING);

            CURL *curl = curl_easy_init();
            if (!curl) {
                Logger::log(Logger::LEVEL_ERROR, "Failed to initialize curl");
                return;
            }

            std::string host = remote.ip;
            if (remote.ipType == discovery::IPv6) {
                host = "[" + host + "]";
            }

            std::string url = "http://" + host + ":" + std::to_string(remote.port) + "/device_info";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

            std::string response;
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_function);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

            CURLcode res = curl_easy_perform(curl);
            if (res != CURLE_OK) {
                Logger::log(Logger::LEVEL_DEBUG, "Failed to execute GET request: " + std::string(curl_easy_strerror(res)));
                curl_easy_cleanup(curl);
                curl_global_cleanup();
                return;
            }

            curl_easy_cleanup(curl);
            curl_global_cleanup();

            json jsonData;
            try {
                jsonData = json::parse(response);
            } catch (const std::exception &) {
                return;
            }
            DeviceInfo deviceInfo;
            from_json(jsonData, deviceInfo);

            callback(deviceInfo);
        });
    }, isStopped);
}

void flowdrop::discover(const flowdrop::discoverCallback &callback) {
    flowdrop::discover(callback, [](){ return false; });
}

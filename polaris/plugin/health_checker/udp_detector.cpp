//  Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//  https://opensource.org/licenses/BSD-3-Clause
//
//  Unless required by applicable law or agreed to in writing, software distributed
//  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
//  language governing permissions and limitations under the License.
//

#include "plugin/health_checker/udp_detector.h"

#include <stddef.h>

#include "logger.h"
#include "plugin/health_checker/health_checker.h"
#include "plugin/plugin_manager.h"
#include "polaris/config.h"
#include "polaris/model.h"
#include "utils/netclient.h"
#include "utils/time_clock.h"
#include "utils/utils.h"

namespace polaris {

UdpHealthChecker::UdpHealthChecker() { timeout_ms_ = 0; }

UdpHealthChecker::~UdpHealthChecker() {}

ReturnCode UdpHealthChecker::Init(Config* config, Context* /*context*/) {
  static const char kUdpSendPackageKey[]        = "send";
  static const char kUdpSendPackageDefault[]    = "";
  static const char kUdpReceivePackageKey[]     = "receive";
  static const char kUdpReceivePackageDefault[] = "";

  std::string send_package = config->GetStringOrDefault(kUdpSendPackageKey, kUdpSendPackageDefault);
  if (send_package.empty()) {
    POLARIS_LOG(LOG_ERROR, "health checker[%s] config %s should not be empty",
                kPluginUdpHealthChecker, kUdpSendPackageKey);
    return kReturnInvalidConfig;
  }
  if (!Utils::HexStringToBytes(send_package, &send_package_)) {
    POLARIS_LOG(LOG_ERROR, "health checker[%s] config %s hexstring to bytes failed",
                kPluginUdpHealthChecker, kUdpSendPackageKey);
    return kReturnInvalidConfig;
  }
  std::string receive_package =
      config->GetStringOrDefault(kUdpReceivePackageKey, kUdpReceivePackageDefault);
  if (!receive_package.empty()) {
    if (!Utils::HexStringToBytes(receive_package, &receive_package_)) {
      POLARIS_LOG(LOG_ERROR, "health checker[%s] config %s hexstring to bytes failed",
                  kPluginUdpHealthChecker, kUdpReceivePackageKey);
      return kReturnInvalidConfig;
    }
  }
  timeout_ms_ = config->GetMsOrDefault(HealthCheckerConfig::kTimeoutKey,
                                       HealthCheckerConfig::kTimeoutDefault);
  return kReturnOk;
}

ReturnCode UdpHealthChecker::DetectInstance(Instance& instance, DetectResult& detect_result) {
  uint64_t start_time_ms    = Time::GetCurrentTimeMs();
  detect_result.detect_type = kPluginUdpHealthChecker;
  if (send_package_.empty()) {
    detect_result.return_code = kReturnInvalidConfig;
    detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
    return kReturnInvalidConfig;
  }
  std::string host = instance.GetHost();
  int port         = instance.GetPort();
  std::string udp_response;
  int retcode = 0;
  if (receive_package_.empty()) {
    retcode = NetClient::UdpSendRecv(host, port, timeout_ms_, send_package_, NULL);
  } else {
    retcode = NetClient::UdpSendRecv(host, port, timeout_ms_, send_package_, &udp_response);
  }
  if (retcode < 0) {
    detect_result.return_code = kReturnNetworkFailed;
    detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
    return kReturnNetworkFailed;
  }
  if (!receive_package_.empty() && receive_package_ != udp_response) {
    detect_result.return_code = kReturnServerError;
    detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
    return kReturnServerError;
  }
  detect_result.return_code = kReturnOk;
  detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
  return kReturnOk;
}

}  // namespace polaris

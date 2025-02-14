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

#ifndef POLARIS_CPP_POLARIS_PLUGIN_HEALTH_CHECKER_HEALTH_CHECKER_H_
#define POLARIS_CPP_POLARIS_PLUGIN_HEALTH_CHECKER_HEALTH_CHECKER_H_

#include <stdint.h>

#include <vector>

#include "polaris/context.h"
#include "polaris/defs.h"

namespace polaris {

class Config;
class LocalRegistry;
class HealthChecker;

namespace HealthCheckerConfig {
static const char kChainWhenKey[]       = "when";
static const char kChainWhenNever[]     = "never";
static const char kChainWhenAlways[]    = "always";
static const char kChainWhenOnRecover[] = "on_recover";

static const char kChainPluginListKey[]     = "chain";
static const char kChainPluginListDefault[] = "tcp";

static const char kCheckerIntervalKey[]        = "interval";
static const uint64_t kDetectorIntervalDefault = 10 * 1000;  // 探活默认时间间隔10s

static const char kTimeoutKey[]       = "timeout";  // 超时时间毫秒
static const uint64_t kTimeoutDefault = 500;        // 默认500ms
}  // namespace HealthCheckerConfig

class HealthCheckerChainImpl : public HealthCheckerChain {
public:
  HealthCheckerChainImpl(const ServiceKey& service_key, LocalRegistry* local_registry);

  virtual ~HealthCheckerChainImpl();

  virtual ReturnCode Init(Config* config, Context* context);

  virtual ReturnCode DetectInstance(CircuitBreakerChain& circuit_breaker_chain);

  virtual std::vector<HealthChecker*> GetHealthCheckers();

private:
  ServiceKey service_key_;
  uint64_t health_check_ttl_ms_;  // 服务探测周期ms
  uint64_t last_detect_time_ms_;  //  上一次探测时间
  std::string when_;
  LocalRegistry* local_registry_;
  std::vector<HealthChecker*> health_checker_list_;
};

}  // namespace polaris

#endif  //  POLARIS_CPP_POLARIS_PLUGIN_HEALTH_CHECKER_HEALTH_CHECKER_H_

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

#include "plugin/circuit_breaker/error_rate.h"

#include <math.h>
#include <stddef.h>

#include <utility>

#include "plugin/circuit_breaker/circuit_breaker.h"
#include "polaris/config.h"
#include "utils/time_clock.h"
#include "utils/utils.h"

namespace polaris {

ErrorRateCircuitBreaker::ErrorRateCircuitBreaker() {
  request_volume_threshold_      = 0;
  error_rate_threshold_          = 0;
  metric_stat_time_window_       = 0;
  metric_num_buckets_            = 0;
  sleep_window_                  = 0;
  metric_expired_time_           = 0;
  metric_bucket_time_            = 0;
  request_count_after_half_open_ = 0;
  success_count_after_half_open_ = 0;
  pthread_rwlock_init(&rwlock_, 0);
}

ErrorRateCircuitBreaker::~ErrorRateCircuitBreaker() {
  pthread_rwlock_destroy(&rwlock_);
  std::map<std::string, ErrorRateStatus>::iterator it;
  for (it = error_rate_map_.begin(); it != error_rate_map_.end(); ++it) {
    pthread_mutex_destroy(&it->second.lock);
    delete[] it->second.buckets;
  }
  error_rate_map_.clear();
}

ReturnCode ErrorRateCircuitBreaker::Init(Config* config, Context* /*context*/) {
  request_volume_threshold_ =
      config->GetIntOrDefault(CircuitBreakerConfig::kRequestVolumeThresholdKey,
                              CircuitBreakerConfig::kRequestVolumeThresholdDefault);
  error_rate_threshold_ =
      config->GetFloatOrDefault(CircuitBreakerConfig::kErrorRateThresholdKey,
                                CircuitBreakerConfig::kErrorRateThresholdDefault);
  metric_stat_time_window_ =
      config->GetMsOrDefault(CircuitBreakerConfig::kMetricStatTimeWindowKey,
                             CircuitBreakerConfig::kMetricStatTimeWindowDefault);
  metric_num_buckets_ = config->GetIntOrDefault(CircuitBreakerConfig::kMetricNumBucketsKey,
                                                CircuitBreakerConfig::kMetricNumBucketsDefault);
  sleep_window_       = config->GetMsOrDefault(CircuitBreakerConfig::kHalfOpenSleepWindowKey,
                                         CircuitBreakerConfig::kHalfOpenSleepWindowDefault);
  request_count_after_half_open_ =
      config->GetIntOrDefault(CircuitBreakerConfig::kRequestCountAfterHalfOpenKey,
                              CircuitBreakerConfig::kRequestCountAfterHalfOpenDefault);
  success_count_after_half_open_ =
      config->GetIntOrDefault(CircuitBreakerConfig::kSuccessCountAfterHalfOpenKey,
                              CircuitBreakerConfig::kSuccessCountAfterHalfOpenDefault);
  metric_expired_time_ = config->GetMsOrDefault(CircuitBreakerConfig::kMetricExpiredTimeKey,
                                                CircuitBreakerConfig::kMetricExpiredTimeDefault);

  // 校验配置有效性
  if (request_volume_threshold_ <= 0) {
    request_volume_threshold_ = CircuitBreakerConfig::kRequestVolumeThresholdDefault;
  }
  if (error_rate_threshold_ <= 0 || error_rate_threshold_ >= 1) {
    error_rate_threshold_ = CircuitBreakerConfig::kErrorRateThresholdDefault;
  }
  if (metric_stat_time_window_ <= 0) {
    metric_stat_time_window_ = CircuitBreakerConfig::kMetricStatTimeWindowDefault;
  }
  if (metric_num_buckets_ <= 0) {
    metric_num_buckets_ = CircuitBreakerConfig::kMetricNumBucketsDefault;
  }
  metric_bucket_time_ =
      static_cast<int>(ceilf(static_cast<float>(metric_stat_time_window_) / metric_num_buckets_));
  if (sleep_window_ <= 0) {
    sleep_window_ = CircuitBreakerConfig::kHalfOpenSleepWindowDefault;
  }
  if (request_count_after_half_open_ <= 0) {
    request_count_after_half_open_ = CircuitBreakerConfig::kRequestCountAfterHalfOpenDefault;
  }
  if (success_count_after_half_open_ <= 0) {
    success_count_after_half_open_ = CircuitBreakerConfig::kSuccessCountAfterHalfOpenDefault;
  } else if (success_count_after_half_open_ > request_count_after_half_open_) {
    success_count_after_half_open_ = request_count_after_half_open_;
  }
  if (metric_expired_time_ <= 0) {
    metric_expired_time_ = CircuitBreakerConfig::kMetricExpiredTimeDefault;
  }
  return kReturnOk;
}

ReturnCode ErrorRateCircuitBreaker::RealTimeCircuitBreak(
    const InstanceGauge& instance_gauge, InstancesCircuitBreakerStatus* /*instances_status*/) {
  // 错误率熔断使用定时接口进行熔断状态改变，实时接口只统计
  uint64_t current_time = Time::GetCurrentTimeMs();
  ErrorRateStatus& error_rate_status =
      GetOrCreateErrorRateStatus(instance_gauge.instance_id, current_time);

  uint64_t bucket_time = current_time / metric_bucket_time_;
  int bucket_index     = bucket_time % metric_num_buckets_;
  // 判断是不是还是上一轮的数据，是的话清空掉
  if (bucket_time != error_rate_status.buckets[bucket_index].bucket_time) {
    pthread_mutex_lock(&error_rate_status.lock);
    if (bucket_time != error_rate_status.buckets[bucket_index].bucket_time) {
      error_rate_status.buckets[bucket_index].total_count = 0;
      error_rate_status.buckets[bucket_index].error_count = 0;
      error_rate_status.buckets[bucket_index].bucket_time = bucket_time;
    }
    pthread_mutex_unlock(&error_rate_status.lock);
  }
  ATOMIC_INC(&error_rate_status.buckets[bucket_index].total_count);
  if (instance_gauge.call_ret_status != kCallRetOk) {
    ATOMIC_INC(&error_rate_status.buckets[bucket_index].error_count);
  }
  return kReturnOk;
}

ReturnCode ErrorRateCircuitBreaker::TimingCircuitBreak(
    InstancesCircuitBreakerStatus* instances_status) {
  uint64_t current_time         = Time::GetCurrentTimeMs();
  uint64_t last_end_bucket_time = current_time / metric_bucket_time_ - metric_num_buckets_;
  std::map<std::string, ErrorRateStatus>::iterator it;
  pthread_rwlock_rdlock(&rwlock_);
  for (it = error_rate_map_.begin(); it != error_rate_map_.end(); ++it) {
    ErrorRateStatus& error_rate_status = it->second;
    // 熔断状态
    if (error_rate_status.status == kCircuitBreakerOpen) {
      if (instances_status->AutoHalfOpenEnable() &&
          error_rate_status.last_update_time + sleep_window_ <= current_time &&
          instances_status->TranslateStatus(it->first, kCircuitBreakerOpen,
                                            kCircuitBreakerHalfOpen)) {
        error_rate_status.last_update_time = current_time;
        error_rate_status.status           = kCircuitBreakerHalfOpen;
        error_rate_status.ClearBuckets(metric_num_buckets_);
      }
      continue;
    }
    // 计算数据
    int total_req = 0, err_req = 0;
    error_rate_status.BucketsCount(metric_num_buckets_, last_end_bucket_time, total_req, err_req);

    if (error_rate_status.status == kCircuitBreakerClose) {
      // 达到熔断条件：请求数达标且错误率达标，request_volume_threshold_大于0能确保total_req大于0才作为除数
      if (total_req >= request_volume_threshold_ &&
          (static_cast<float>(err_req) / total_req >= error_rate_threshold_) &&
          instances_status->TranslateStatus(it->first, kCircuitBreakerClose, kCircuitBreakerOpen)) {
        error_rate_status.last_update_time = current_time;
        error_rate_status.status           = kCircuitBreakerOpen;
        // 熔断后不会使用数据判断是否进入半开，这里不用清空数据
      }
      continue;
    }
    // 半开状态
    if (error_rate_status.status == kCircuitBreakerHalfOpen) {
      // 达到恢复条件
      if (total_req - err_req >= success_count_after_half_open_) {
        if (instances_status->TranslateStatus(it->first, kCircuitBreakerHalfOpen,
                                              kCircuitBreakerClose)) {
          error_rate_status.last_update_time = current_time;
          error_rate_status.status           = kCircuitBreakerClose;
          error_rate_status.ClearBuckets(metric_num_buckets_);
        }
      } else if (err_req > request_count_after_half_open_ - success_count_after_half_open_ ||
                 error_rate_status.last_access_time + 100 * sleep_window_ <= current_time) {
        // 达到重新熔断条件
        if (instances_status->TranslateStatus(it->first, kCircuitBreakerHalfOpen,
                                              kCircuitBreakerOpen)) {
          error_rate_status.last_update_time = current_time;
          error_rate_status.status           = kCircuitBreakerOpen;
          error_rate_status.ClearBuckets(metric_num_buckets_);
        }
      }
    }
  }
  pthread_rwlock_unlock(&rwlock_);
  CheckAndExpiredMetric(instances_status);
  return kReturnOk;
}

ErrorRateStatus& ErrorRateCircuitBreaker::GetOrCreateErrorRateStatus(const std::string instance_id,
                                                                     uint64_t current_time) {
  pthread_rwlock_rdlock(&rwlock_);
  std::map<std::string, ErrorRateStatus>::iterator it = error_rate_map_.find(instance_id);
  if (it != error_rate_map_.end()) {
    it->second.last_access_time = current_time;
    pthread_rwlock_unlock(&rwlock_);
    return it->second;
  }
  pthread_rwlock_unlock(&rwlock_);

  pthread_rwlock_wrlock(&rwlock_);
  it = error_rate_map_.find(instance_id);  // double check
  if (it != error_rate_map_.end()) {
    it->second.last_access_time = current_time;
    pthread_rwlock_unlock(&rwlock_);
    return it->second;
  }
  ErrorRateStatus& error_rate_status = error_rate_map_[instance_id];
  error_rate_status.buckets          = new ErrorRateBucket[metric_num_buckets_];
  for (int i = 0; i < metric_num_buckets_; i++) {
    error_rate_status.buckets[i].bucket_time = 0;
  }
  error_rate_status.status           = kCircuitBreakerClose;
  error_rate_status.last_access_time = current_time;
  error_rate_status.last_update_time = 0;
  pthread_mutex_init(&error_rate_status.lock, NULL);

  pthread_rwlock_unlock(&rwlock_);
  return error_rate_status;
}

void ErrorRateCircuitBreaker::CheckAndExpiredMetric(
    InstancesCircuitBreakerStatus* instances_status) {
  uint64_t current_time = Time::GetCurrentTimeMs();
  std::map<std::string, ErrorRateStatus>::iterator it;
  pthread_rwlock_wrlock(&rwlock_);
  for (it = error_rate_map_.begin(); it != error_rate_map_.end();) {
    if (it->second.last_access_time + metric_expired_time_ <= current_time) {
      pthread_mutex_destroy(&it->second.lock);
      instances_status->TranslateStatus(it->first, kCircuitBreakerOpen, kCircuitBreakerClose);
      instances_status->TranslateStatus(it->first, kCircuitBreakerHalfOpen, kCircuitBreakerClose);
      delete[] it->second.buckets;
      error_rate_map_.erase(it++);
    } else {
      ++it;
    }
  }
  pthread_rwlock_unlock(&rwlock_);
}

}  // namespace polaris

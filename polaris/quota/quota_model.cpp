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

#include "quota/quota_model.h"

#include <stddef.h>

namespace polaris {

QuotaRequest::QuotaRequest() { impl_ = new QuotaRequestImpl(); }

QuotaRequest::~QuotaRequest() {
  if (impl_ != NULL) {
    delete impl_;
    impl_ = NULL;
  }
}

void QuotaRequest::SetServiceNamespace(const std::string& service_namespace) {
  impl_->service_key_.namespace_ = service_namespace;
}

void QuotaRequest::SetServiceName(const std::string& service_name) {
  impl_->service_key_.name_ = service_name;
}

void QuotaRequest::SetSubset(const std::map<std::string, std::string>& subset) {
  impl_->subset_ = subset;
}

void QuotaRequest::SetLabels(const std::map<std::string, std::string>& labels) {
  impl_->labels_ = labels;
}

void QuotaRequest::SetAcquireAmount(int amount) { impl_->acquire_amount_ = amount; }

void QuotaRequest::SetTimeout(uint64_t timeout) { impl_->timeout_ = timeout; }

QuotaResponse::QuotaResponse(QuotaResponseImpl* impl) : impl_(impl) {}

QuotaResponse::~QuotaResponse() {
  if (impl_ != NULL) {
    delete impl_;
    impl_ = NULL;
  }
}

QuotaResultCode QuotaResponse::GetResultCode() const { return impl_->result_code_; }

uint64_t QuotaResponse::GetWaitTime() const { return impl_->wait_time_; }

const QuotaResultInfo& QuotaResponse::GetQuotaResultInfo() const { return impl_->info_; }

QuotaResponse* QuotaResponseImpl::CreateResponse(QuotaResultCode result_code, uint64_t wait_time) {
  QuotaResponseImpl* impl = new QuotaResponseImpl();
  impl->result_code_      = result_code;
  impl->wait_time_        = wait_time;
  impl->info_.all_quota_  = 0;
  impl->info_.duration_   = 0;
  impl->info_.left_quota_ = 0;
  return new QuotaResponse(impl);
}

QuotaResponse* QuotaResponseImpl::CreateResponse(QuotaResultCode result_code,
                                                 const QuotaResultInfo& info) {
  QuotaResponseImpl* impl = new QuotaResponseImpl();
  impl->result_code_      = result_code;
  impl->wait_time_        = 0;
  impl->info_             = info;
  return new QuotaResponse(impl);
}

///////////////////////////////////////////////////////////////////////////////
LimitCallResult::LimitCallResult() {
  impl_                 = new LimitCallResultImpl();
  impl_->result_type_   = kLimitCallResultOk;
  impl_->response_time_ = 0;
  impl_->response_code_ = 0;
}

LimitCallResult::~LimitCallResult() {
  if (impl_ != NULL) {
    delete impl_;
    impl_ = NULL;
  }
}

void LimitCallResult::SetServiceNamespace(const std::string& service_namespace) {
  impl_->service_key_.namespace_ = service_namespace;
}

void LimitCallResult::SetServiceName(const std::string& service_name) {
  impl_->service_key_.name_ = service_name;
}

void LimitCallResult::SetSubset(const std::map<std::string, std::string>& subset) {
  impl_->subset_ = subset;
}

void LimitCallResult::SetLabels(const std::map<std::string, std::string>& labels) {
  impl_->labels_ = labels;
}

void LimitCallResult::SetResponseResult(LimitCallResultType result_type) {
  impl_->result_type_ = result_type;
}

void LimitCallResult::SetResponseTime(uint64_t response_time) {
  impl_->response_time_ = response_time;
}

void LimitCallResult::SetResponseCode(int response_code) { impl_->response_code_ = response_code; }

}  // namespace polaris

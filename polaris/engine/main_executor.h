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

#ifndef POLARIS_CPP_POLARIS_ENGINE_MAIN_EXECUTOR_H_
#define POLARIS_CPP_POLARIS_ENGINE_MAIN_EXECUTOR_H_

#include "engine/executor.h"

namespace polaris {

class Context;

/// @brief 主任务
///
/// 执行如下任务：
///   - 发送ReportClient请求给Discover Server并更新ClientLocation
class MainExecutor : public Executor {
public:
  explicit MainExecutor(Context* context);

  virtual ~MainExecutor() {}

  // 获取线程名字
  virtual const char* GetName() { return "main_task"; }

  virtual void SetupWork();

  // 定期执行ReportClient
  static void TimingReportClient(MainExecutor* main_executor);

private:
  int init_retry_times_;
};

}  // namespace polaris

#endif  //  POLARIS_CPP_POLARIS_ENGINE_MAIN_EXECUTOR_H_

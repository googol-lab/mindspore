/**
 * Copyright 2020 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_FP32_REVERSE_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_FP32_REVERSE_H_

#include <vector>
#include "src/lite_kernel.h"

#include "include/context.h"

#define REVERSE_STRIDE_MAX_SIZE 4

using mindspore::lite::Context;

namespace mindspore::kernel {
class ReverseCPUKernel : public LiteKernel {
 public:
  ReverseCPUKernel(OpParameter *parameter, const std::vector<lite::tensor::Tensor *> &inputs,
                   const std::vector<lite::tensor::Tensor *> &outputs, const Context *ctx)
      : LiteKernel(parameter, inputs, outputs), ctx_(ctx), thread_count_(ctx->threadNum) {}
  ~ReverseCPUKernel() {
    if (tmp_ != nullptr) {
      free(tmp_);
      tmp_ = nullptr;
    }
  }

  int Init() override;
  int ReSize() override;
  int Run() override;
  int Stride(int index);
  int DoReverse(int task_id);

 private:
  int thread_count_;
  int thread_sz_count_;
  int thread_sz_stride_;
  int data_size_;
  int strides_[REVERSE_STRIDE_MAX_SIZE];
  int inCount_[REVERSE_STRIDE_MAX_SIZE];
  int outCount_[REVERSE_STRIDE_MAX_SIZE];
  const Context *ctx_;
  int *tmp_ = nullptr;
  float *in_ptr_;
  float *out_ptr_;
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_ARM_FP32_REVERSE_H_

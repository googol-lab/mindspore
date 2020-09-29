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

#ifndef MINDSPORE_LITE_SRC_RUNTIME_KERNEL_OPENCL_KERNEL_RESIZE_H_
#define MINDSPORE_LITE_SRC_RUNTIME_KERNEL_OPENCL_KERNEL_RESIZE_H_

#include <vector>

#include "src/lite_kernel.h"
#include "src/runtime/kernel/opencl/opencl_kernel.h"
#include "nnacl/resize_parameter.h"

namespace mindspore::kernel {
class ResizeOpenCLKernel : public OpenCLKernel {
 public:
  explicit ResizeOpenCLKernel(OpParameter *parameter, const std::vector<lite::Tensor *> &inputs,
                              const std::vector<lite::Tensor *> &outputs)
      : OpenCLKernel(parameter, inputs, outputs) {}
  ~ResizeOpenCLKernel() override{};

  int Init() override;
  int ReSize() override;
  int Run() override;
  int GetImageSize(size_t idx, std::vector<size_t> *img_size) override;
  float getResizeScaleFactor(int input_size, int output_size);

 private:
  cl::Kernel kernel_;
  bool alignCorner;
  bool preserveAspectRatio;
};
}  // namespace mindspore::kernel

#endif  // MINDSPORE_LITE_SRC_RUNTIME_KERNEL_OPENCL_KERNEL_RESIZE_H_

/**
 * Copyright 2019 Huawei Technologies Co., Ltd
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

#include "tools/converter/quantizer/aware_quantizer.h"

#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "schema/inner/model_generated.h"
#include "securec/include/securec.h"
#include "src/common/utils.h"
#include "tools/common/converter_op_utils.h"
#include "tools/common/node_util.h"
#include "tools/common/tensor_util.h"
#include "tools/converter/quantizer/calc_quant_param.h"
#include "src/common/log_adapter.h"

using std::string;
using std::vector;

namespace mindspore::lite::quant {
const std::array<schema::PrimitiveType, 7> AwareQuantizer::propagatedOps = {
  {schema::PrimitiveType_Concat, schema::PrimitiveType_Resize, schema::PrimitiveType_Reshape,
   schema::PrimitiveType_Squeeze, schema::PrimitiveType_RealDiv, schema::PrimitiveType_Activation,
   schema::PrimitiveType_DetectionPostProcess}};

AwareQuantizer::AwareQuantizer(schema::MetaGraphT *graph, const TypeId &inferType) : FbQuantizer(graph) {}

STATUS AwareQuantizer::RemoveFakeQuant() { return RET_OK; }

STATUS AwareQuantizer::GenerateDefaultQuantParam(const schema::MetaGraphT *subGraph) {
  MS_ASSERT(subGraph != nullptr);
  for (const auto &tensor : subGraph->allTensors) {
    if (!tensor->quantParams.empty()) {
      continue;
    }
    std::unique_ptr<schema::QuantParamT> defaultQuantParam(new QuantParamT());
    tensor->quantParams.emplace_back(std::move(defaultQuantParam));
  }
  return RET_OK;
}

STATUS AwareQuantizer::SetAttrToConvolution(const schema::MetaGraphT *subGraph, schema::CNodeT *node) { return RET_OK; }

STATUS AwareQuantizer::GenerateQuantParam() {
  auto *quantParamRegister = QuantParamCalcRegister::GetInstance();

  for (auto iter = graph->nodes.begin(); iter != graph->nodes.end(); iter++) {
    auto &node = *iter;
    MS_ASSERT(node != nullptr);
    if (GetCNodeTType(*node) == schema::PrimitiveType_FakeQuantWithMinMax ||
        GetCNodeTType(*node) == schema::PrimitiveType_FakeQuantWithMinMaxVars) {
      MS_ASSERT(false);
    }
    auto quantParamCalcer = quantParamRegister->GetQuantParamCalcer(GetCNodeTType(*node));
    if (quantParamCalcer == nullptr) {
      MS_LOG(WARNING) << "Can not find QuantParamCalcer for " << node->name.c_str()
                      << ", type: " << GetCNodeTTypeName(*node).c_str() << " set node to QuantNone and skip";
      node->quantType = static_cast<schema::QuantType>(QuantType_QUANT_NONE);
    } else {
      auto status = quantParamCalcer->Calc(graph, *node);
      if (status != RET_OK) {
        MS_LOG(WARNING) << "quantParamCalcer failed: " << status << " node: " << node->name.c_str();
        node->quantType = schema::QuantType_QUANT_NONE;
      } else {
        node->quantType = schema::QuantType_AwareTraining;
      }
    }
  }
  return RET_OK;
}

STATUS AwareQuantizer::DoQuantize() {
  for (auto iter = graph->nodes.begin(); iter != graph->nodes.end(); iter++) {
    auto &node = *iter;
    if (!IsContain(GetInt8OpList(), GetCNodeTType(*node))) {
      continue;
    }
    if (node->quantType != schema::QuantType_AwareTraining) {
      continue;
    }
    STATUS status;
    if (GetCNodeTType(*node) == schema::PrimitiveType_Conv2D ||
        GetCNodeTType(*node) == schema::PrimitiveType_DepthwiseConv2D ||
        GetCNodeTType(*node) == schema::PrimitiveType_DeConv2D ||
        GetCNodeTType(*node) == schema::PrimitiveType_FullConnection ||
        GetCNodeTType(*node) == schema::PrimitiveType_MatMul) {
      auto inputIndexes = node->inputIndex;
      if (inputIndexes.size() < 2) {
        MS_LOG(WARNING) << node->name.c_str() << " node input has invalid inputs tensor count";
        return RET_ERROR;
      }
      // quant weight
      auto &weightTensor = graph->allTensors.at(node->inputIndex.at(1));
      if (!weightTensor->quantParams.empty() && weightTensor->quantParams.at(0)->inited) {
        status = QuantConvWeight(graph, node.get());
        if (status != RET_OK) {
          MS_LOG(WARNING) << "QuantConvWeight failed!";
          return RET_ERROR;
        }
      }
      // quant bias
      if (inputIndexes.size() == 3) {
        auto &biasTensor = graph->allTensors.at(node->inputIndex.at(2));
        if (!biasTensor->quantParams.empty() && biasTensor->quantParams.at(0)->inited) {
          status = QuantConvBias(graph, node.get());
          if (status != RET_OK) {
            MS_LOG(WARNING) << "QuantConvBias failed!";
            return RET_ERROR;
          }
        }
      }
    } else if (GetCNodeTType(*node) == schema::PrimitiveType_DetectionPostProcess) {
      status = QuantDetectionPostProcessConstTensor(graph, node.get());
      if (status != RET_OK) {
        MS_LOG(WARNING) << "QuantDetectionPostProcessConstTensor failed!";
        return RET_ERROR;
      }
    } else if (GetCNodeTType(*node) == schema::PrimitiveType_Add ||
               GetCNodeTType(*node) == schema::PrimitiveType_Scale ||
               GetCNodeTType(*node) == schema::PrimitiveType_Mul) {
      status = QuantArithmeticConstTensor(graph, node.get());
      if (status != RET_OK) {
        MS_LOG(WARNING) << "QuantArithmeticConstTensor failed!";
        return RET_ERROR;
      }
    }
    const auto nodeType = GetCNodeTType(*node);
    auto find = std::find(propagatedOps.begin(), propagatedOps.end(), nodeType);
    if (find != propagatedOps.end()) {
      auto inputTensor = graph->allTensors.at(node->inputIndex[0]).get();
      auto outputTensor = graph->allTensors.at(node->outputIndex[0]).get();
      MS_ASSERT(inputTensor != nullptr);
      MS_ASSERT(outputTensor != nullptr);
      outputTensor->dataType = inputTensor->dataType;
    }
  }
  return RET_OK;
}

STATUS AwareQuantizer::QuantArithmeticConstTensor(const schema::MetaGraphT *graph, schema::CNodeT *node) {
  MS_ASSERT(graph != nullptr);
  MS_ASSERT(node != nullptr);
  for (size_t i = 0; i < node->inputIndex.size(); i++) {
    auto inTensorIdx = node->inputIndex.at(i);
    MS_ASSERT(graph->allTensors.size() > inTensorIdx);
    auto &inTensor = graph->allTensors.at(inTensorIdx);
    MS_ASSERT(inTensor != nullptr);
    if (!inTensor->data.empty()) {
      if (inTensor->dataType == TypeId::kNumberTypeInt8) {
        continue;
      }
      if (inTensor->dataType != TypeId::kNumberTypeFloat32 && inTensor->dataType != TypeId::kNumberTypeFloat &&
          inTensor->dataType != TypeId::kNumberTypeUInt8) {
        MS_LOG(WARNING) << node->name.c_str() << "'s weight data is not float or uint8";
        return RET_ERROR;
      }

      auto quantParam = GetTensorQuantParam(inTensor);
      MS_ASSERT(quantParam != nullptr);
      MS_ASSERT(quantParam->inited);
      auto constTensorShapeSize = GetShapeSize(*(inTensor.get()));
      vector<int8_t> qDatas(constTensorShapeSize);
      void *inData = inTensor->data.data();
      if (inTensor->dataType == TypeId::kNumberTypeFloat ||
          inTensor->dataType == TypeId::kNumberTypeFloat32) {  // normal awareing quant
        auto *weightData = static_cast<float *>(inData);
        for (size_t j = 0; j < constTensorShapeSize; j++) {
          qDatas[j] = QuantizeData<int8_t>(weightData[j], quantParam.get());
        }
      } else {  // tflite awareing quant
        auto *weightData = static_cast<uint8_t *>(inData);
        for (size_t j = 0; j < constTensorShapeSize; j++) {
          qDatas[j] = (int32_t)weightData[j] - 128;
        }
        quantParam->zeroPoint -= 128;
        inTensor->quantParams.clear();
        inTensor->quantParams.emplace_back(quantParam.release());
      }

      ::memcpy(inTensor->data.data(), qDatas.data(), constTensorShapeSize);
      inTensor->dataType = TypeId::kNumberTypeInt8;
    }
  }
  return RET_OK;
}

STATUS AwareQuantizer::QuantDetectionPostProcessConstTensor(const schema::MetaGraphT *subGraph, schema::CNodeT *node) {
  MS_ASSERT(subGraph != nullptr);
  MS_ASSERT(node != nullptr);
  auto &constTensor = subGraph->allTensors.at(node->inputIndex[2]);
  MS_ASSERT(constTensor != nullptr);
  const auto *constData = reinterpret_cast<const float *>(constTensor->data.data());

  if (!constTensor->data.empty() &&
      (constTensor->dataType == TypeId::kNumberTypeFloat || constTensor->dataType == TypeId::kNumberTypeFloat32)) {
    size_t constTensorShapeSize = GetShapeSize(*constTensor);
    std::unique_ptr<QuantParamT> quantParam = GetTensorQuantParam(constTensor);
    if (quantParam == nullptr) {
      MS_LOG(ERROR) << "new QuantParamT failed";
      return RET_NULL_PTR;
    }
    vector<int8_t> qDatas(constTensorShapeSize);
    for (size_t j = 0; j < constTensorShapeSize; j++) {
      float rawData = constData[j];
      qDatas[j] = QuantizeData<int8_t>(rawData, quantParam.get());
    }
    ::memcpy(constTensor->data.data(), qDatas.data(), constTensorShapeSize);
    constTensor->dataType = TypeId::kNumberTypeInt8;
  }
  return RET_OK;
}

STATUS AwareQuantizer::QuantConvBias(const mindspore::schema::MetaGraphT *graph, mindspore::schema::CNodeT *node) {
  MS_ASSERT(graph != nullptr);
  MS_ASSERT(node != nullptr);
  auto inputIndexes = node->inputIndex;
  MS_ASSERT(inputIndexes.size() >= 3);
  MS_ASSERT(graph->allTensors.size() > inputIndexes.at(0));
  MS_ASSERT(graph->allTensors.size() > inputIndexes.at(1));
  MS_ASSERT(graph->allTensors.size() > inputIndexes.at(2));
  auto &biasTensor = graph->allTensors.at(inputIndexes.at(2));
  MS_ASSERT(biasTensor != nullptr);
  if (biasTensor->dataType == TypeId::kNumberTypeInt32) {
    return RET_OK;
  }
  if (biasTensor->dataType != TypeId::kNumberTypeFloat && biasTensor->dataType != TypeId::kNumberTypeFloat32) {
    MS_LOG(ERROR) << "conv " << node->name << "'s bias data is not float";
    return RET_ERROR;
  }
  auto &inputTensor = graph->allTensors.at(inputIndexes.at(0));
  auto &weightTensor = graph->allTensors.at(inputIndexes.at(1));

  MS_ASSERT(inputTensor != nullptr);
  MS_ASSERT(weightTensor != nullptr);
  auto inputScale = inputTensor->quantParams.front()->scale;
  auto weightScale = weightTensor->quantParams.front()->scale;
  auto scale = inputScale * weightScale;
  // set bias quant param
  std::unique_ptr<QuantParamT> biasQuantParam = GetTensorQuantParam(biasTensor);
  if (biasQuantParam == nullptr) {
    MS_LOG(ERROR) << "new QuantParamT failed";
    return RET_ERROR;
  }
  biasQuantParam->inited = true;
  biasQuantParam->scale = scale;
  biasQuantParam->zeroPoint = 0;
  biasQuantParam->numBits = 8;
  biasQuantParam->narrowRange = false;
  biasQuantParam->min = 0.0;
  biasQuantParam->max = 0.0;

  // quant bias data
  auto bShapeSize = GetShapeSize(*(biasTensor.get()));
  std::unique_ptr<int32_t[]> qDatas(new (std::nothrow) int32_t[bShapeSize]);
  if (qDatas == nullptr) {
    MS_LOG(ERROR) << "new qDatas failed";
    return RET_ERROR;
  }
  void *biasData = biasTensor->data.data();
  auto *rawDatas = static_cast<float *>(biasData);
  for (size_t i = 0; i < bShapeSize; ++i) {
    qDatas[i] = (int32_t)std::round(rawDatas[i] / scale);
  }
  biasTensor->dataType = TypeId::kNumberTypeInt32;
  biasTensor->data.clear();
  biasTensor->data.resize(bShapeSize * sizeof(int32_t));
  auto ret =
    memcpy_s(biasTensor->data.data(), bShapeSize * sizeof(int32_t), qDatas.get(), bShapeSize * sizeof(int32_t));
  if (ret != EOK) {
    MS_LOG(ERROR) << "memcpy_s failed: " << ret;
    return RET_ERROR;
  }
  return RET_OK;
}

STATUS AwareQuantizer::QuantConvWeight(const schema::MetaGraphT *subGraph, schema::CNodeT *node) {
  MS_ASSERT(subGraph != nullptr);
  MS_ASSERT(node != nullptr);
  MS_ASSERT(node->quantParam.size() == node->inputIndex.size() + node->outputIndex.size());
  auto inputIndexes = node->inputIndex;
  MS_ASSERT(inputIndexes.size() >= 2);
  MS_ASSERT(subGraph->allTensors.size() > inputIndexes.at(1));
  auto &weightTensor = subGraph->allTensors.at(inputIndexes.at(1));
  if (weightTensor->dataType == TypeId::kNumberTypeInt8) {
    return RET_OK;
  }
  if (weightTensor->dataType != TypeId::kNumberTypeFloat32 && weightTensor->dataType != TypeId::kNumberTypeFloat &&
      weightTensor->dataType != TypeId::kNumberTypeUInt8) {
    MS_LOG(WARNING) << "conv " << node->name.c_str() << "'s weight data is not float or uint8";
    return RET_ERROR;
  }
  size_t wShapeSize = GetShapeSize(*(weightTensor.get()));
  void *oriWeightData = weightTensor->data.data();
  MS_ASSERT(node->quantParam.at(1)->param.front() != nullptr);
  vector<int8_t> qDatas(wShapeSize);
  // todo support perchannel
  auto weightQauntParam = GetTensorQuantParam(weightTensor);
  if (weightTensor->dataType == TypeId::kNumberTypeFloat ||
      weightTensor->dataType == TypeId::kNumberTypeFloat32) {  // normal awareing quant
    auto *weightData = static_cast<float *>(oriWeightData);
    for (size_t j = 0; j < wShapeSize; j++) {
      qDatas[j] = QuantizeData<int8_t>(weightData[j], weightQauntParam.get());
    }
  } else {  // tflite awareing quant
    auto *weightData = static_cast<uint8_t *>(oriWeightData);
    for (size_t j = 0; j < wShapeSize; j++) {
      qDatas[j] = (int32_t)weightData[j] - 128;
    }
    weightQauntParam->zeroPoint -= 128;
    weightTensor->quantParams.clear();
    weightTensor->quantParams.emplace_back(weightQauntParam.release());
  }

  weightTensor->data.resize(wShapeSize * sizeof(uint8_t));
  ::memcpy(weightTensor->data.data(), qDatas.data(), wShapeSize);
  weightTensor->dataType = TypeId::kNumberTypeInt8;
  return RET_OK;
}
STATUS AwareQuantizer::DetermineNodeQuantType() {
  MS_ASSERT(graph != nullptr);
  for (auto &node : graph->nodes) {
    MS_ASSERT(node != nullptr);
    bool canQuant = true;
    for (auto &outTensorIdx : node->outputIndex) {
      MS_ASSERT(graph->allTensors.size() > outTensorIdx);
      auto &outTensor = graph->allTensors.at(outTensorIdx);
      MS_ASSERT(outTensor != nullptr);
      if (outTensor->quantParams.empty() || outTensor->quantParams.front() == nullptr ||
          !outTensor->quantParams.front()->inited) {
        canQuant = false;
        break;
      }
    }

    if (canQuant && IsContain(GetInt8OpList(), GetCNodeTType(*node))) {
      node->quantType = schema::QuantType_AwareTraining;
    } else {
      node->quantType = schema::QuantType_QUANT_NONE;
    }
  }
  return RET_OK;
}
}  // namespace mindspore::lite::quant

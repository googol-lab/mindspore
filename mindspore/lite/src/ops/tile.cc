/**
 * Copyright 2019-2020 Huawei Technologies Co., Ltd
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

#include "src/ops/tile.h"
#include <algorithm>

namespace mindspore {
namespace lite {
#ifdef PRIMITIVE_WRITEABLE
std::vector<int> Tile::GetMultiples() const { return this->primitive_->value.AsTile()->multiples; }

void Tile::SetMultiples(const std::vector<int> &multiples) { this->primitive_->value.AsTile()->multiples = multiples; }

std::vector<int> Tile::GetDims() const { return this->primitive_->value.AsTile()->multiples; }

void Tile::SetDims(const std::vector<int> &dims) { this->primitive_->value.AsTile()->dims = dims; }

int Tile::UnPackAttr(const Primitive &prim, const std::vector<AnfNodePtr> &inputs) {
  if (this->primitive_ == nullptr) {
    this->primitive_ = new (std::nothrow) schema::PrimitiveT;
    if (this->primitive_ == nullptr) {
      MS_LOG(ERROR) << "new primitiveT failed";
      return RET_ERROR;
    }
    this->primitive_->value.type = schema::PrimitiveType_Tile;
  }
  if (this->primitive_->value.type != schema::PrimitiveType_Tile) {
    MS_LOG(ERROR) << "Primitive type is error :" << this->primitive_->value.type;
    return RET_ERROR;
  }
  if (this->primitive_->value.value == nullptr) {
    this->primitive_->value.value = new (std::nothrow) schema::TileT();
    if (this->primitive_->value.value == nullptr) {
      MS_LOG(ERROR) << "new primitiveT value failed";
      return RET_ERROR;
    }
  }
  return RET_OK;
}

#else

std::vector<int> Tile::GetMultiples() const {
  auto fb_vector = this->primitive_->value_as_Tile()->multiples();
  return std::vector<int>(fb_vector->begin(), fb_vector->end());
}

std::vector<int> Tile::GetDims() const {
  auto fb_vector = this->primitive_->value_as_Tile()->dims();
  return std::vector<int>(fb_vector->begin(), fb_vector->end());
}
int Tile::UnPackToFlatBuilder(const schema::Primitive *primitive, flatbuffers::FlatBufferBuilder *fbb) {
  MS_ASSERT(nullptr != primitive);
  MS_ASSERT(nullptr != fbb);
  auto attr = primitive->value_as_Tile();
  if (attr == nullptr) {
    MS_LOG(ERROR) << "value_as_Tile return nullptr";
    return RET_ERROR;
  }
  std::vector<int32_t> multiples;
  if (attr->multiples() != nullptr) {
    for (int i = 0; i < static_cast<int>(attr->multiples()->size()); i++) {
      multiples.push_back(attr->multiples()->data()[i]);
    }
  }
  std::vector<int32_t> dims;
  if (attr->dims() != nullptr) {
    for (int i = 0; i < static_cast<int>(attr->dims()->size()); i++) {
      dims.push_back(attr->dims()->data()[i]);
    }
  }
  auto val_offset = schema::CreateTileDirect(*fbb, &multiples, &dims);
  auto prim_offset = schema::CreatePrimitive(*fbb, schema::PrimitiveType_Tile, val_offset.o);
  fbb->Finish(prim_offset);
  return RET_OK;
}
#endif

int Tile::InferShape(std::vector<Tensor *> inputs_, std::vector<Tensor *> outputs_) {
  MS_ASSERT(this->primitive_ != nullptr);
  auto input = inputs_.front();
  MS_ASSERT(input != nullptr);
  auto output = outputs_.front();
  MS_ASSERT(output != nullptr);
  output->set_data_type(input->data_type());
  output->SetFormat(input->GetFormat());
  if (!GetInferFlag()) {
    return RET_OK;
  }

  MS_ASSERT(tile_prim != nullptr);
  std::vector<int> out_shape;
  std::vector<int> multiples;
  for (size_t i = 0; i < GetMultiples().size(); ++i) {
    multiples.push_back(GetMultiples()[i]);
  }
  for (size_t i = 0; i < input->shape().size(); ++i) {
    int tmp = input->shape()[i] * multiples[i];
    out_shape.push_back(tmp);
  }

  output->set_shape(out_shape);
  return RET_OK;
}
}  // namespace lite
}  // namespace mindspore

#!/usr/bin/env python
# -*- coding:utf-8 -*-
"""
copyright 2019 Huawei Technologies Co., Ltd
 
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
 
http://www.apache.org/licenses/LICENSE-2.0
 
Unless required by applicable law or agreed to in writing, software
distributed under the License == distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
 
matmul
"""
from __future__ import absolute_import
 
import te.lang.cce
import te.platform.cce_params as cce
from te import tvm
from topi import generic
from topi.cce import util
 
from impl.matmul_vector import matmul_vector_cce
from mindspore.ops.op_info_register import op_info_register
 
# General limitation of the size for input shape: 2**31
SHAPE_SIZE_LIMIT = 2147483648
NoneType = type(None)
 
@op_info_register("""{
    "op_name": "CusMatMulCube",
    "imply_type": "TBE",
    "fusion_type": "OPAQUE",
    "async_flag": false,
    "binfile_name": "matmulcube.so",
    "compute_cost": 10,
    "kernel_name": "CusMatMulCube",
    "partial_flag": true,
    "attr": [
        {
            "name": "transpose_a",
            "param_type": "required",
            "type": "bool",
            "value": "all"
        },
        {
            "name": "transpose_b",
            "param_type": "required",
            "type": "bool",
            "value": "all"
        }
    ],
    "inputs": [
        {
            "index": 0,
            "dtype": [
                "float16"
            ],
            "format": [
                "FRACTAL_NZ"
            ],
            "name": "x1",
            "need_compile": false,
            "param_type": "required",
            "shape": "all"
        },
        {
            "index": 1,
            "dtype": [
                "float16"
            ],
            "format": [
                "FRACTAL_NZ"
            ],
            "name": "x2",
            "need_compile": false,
            "param_type": "required",
            "shape": "all"
        },
        {
            "index": 2,
            "dtype": [
                "float16"
            ],
            "format": [
                "DefaultFormat"
            ],
            "name": "x3",
            "need_compile": false,
            "param_type": "optional",
            "shape": "all"
        }
    ],
    "outputs": [
        {
            "index": 0,
            "dtype": [
                "float32"
            ],
            "format": [
                "FRACTAL_NZ"
            ],
            "name": "y",
            "need_compile": false,
            "param_type": "required",
            "shape": "all"
        }
    ]
}""")
 
# pylint: disable=locally-disabled,too-many-arguments, too-many-locals, too-many-statements
@util.check_input_type(dict, dict, (dict, NoneType), dict, bool, bool, str)
def CusMatMulCube(input_x1, input_x2, bias=None, output_y={}, trans_a=False, trans_b=False, kernel_name="matmulcube"):
   return

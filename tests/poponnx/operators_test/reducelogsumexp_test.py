import itertools
import numpy as np
import poponnx
import torch
import pytest
import torch.nn.functional as F
from op_tester import op_tester


def test_reducelogsumexp(op_tester):
    data = np.random.rand(5, 3, 7).astype(np.float32)
    axes_list = [[0], [1], [2], [0, 1], [0, 2], [1, 2], [0, 1, 2]]
    keepdims_list = [False, True]

    def init_builder(builder):
        tensor = builder.addInputTensor(data)
        result = []
        for axes, keepdims in itertools.product(axes_list, keepdims_list):
            out = builder.aiOnnx.reducelogsumexp(
                [tensor],
                axes=axes,
                keepdims=keepdims,
                debugPrefix="test_reduceprod_{0}_{1}".format(axes, keepdims))
            builder.addOutputTensor(out)
            result.append(out)
        return result

    def reference(ref_data):
        result = []
        for axes, keepdims in itertools.product(axes_list, keepdims_list):
            result.append(
                np.log(
                    np.sum(np.exp(data), axis=tuple(axes), keepdims=keepdims)))
        return result

    op_tester.passes = ['PreUniRepl']
    op_tester.run(init_builder, reference, 'infer')


def test_reducelogsumexp_training(op_tester):
    data = np.random.rand(2, 5, 3).astype(np.float32)
    # To check if implementation can handle 0.0 in the input with gradients
    data[1, 1, 1] = 0.0
    axes_list = [[0], [1], [2], [0, 1], [0, 2], [1, 2], [0, 1, 2]]
    keepdims_list = [False, True]

    def init_builder(builder):
        result = []
        axes_reduce = []
        for axes, keepdims in itertools.product(axes_list, keepdims_list):
            tensor = builder.addInputTensor(data)
            out = builder.aiOnnx.reducelogsumexp(
                [tensor],
                axes=axes,
                keepdims=keepdims,
                debugPrefix="test_reduceprod_{0}_{1}".format(axes, keepdims))
            result.append(out)
            result.append(poponnx.reservedGradientPrefix() + tensor)
            axes_reduce.append(range(3 - (0 if keepdims else len(axes))))
        sum = builder.aiOnnx.sum([
            builder.aiOnnx.reducesum([r],
                                     axes=axes,
                                     keepdims=False,
                                     debugPrefix="test_reducesum_all")
            for r, axes in zip(result[0::2], axes_reduce)
        ],
                                 debugPrefix="test_sum")
        reshaped_sum = builder.aiOnnx.unsqueeze([sum],
                                                axes=[0],
                                                debugPrefix="test_reshape")
        builder.addOutputTensor(reshaped_sum)
        result = [
            reshaped_sum,
            poponnx.reservedGradientPrefix() + reshaped_sum
        ] + result
        return result

    def reference(ref_data):
        result = []
        for axes, keepdims in itertools.product(axes_list, keepdims_list):
            tensor = torch.tensor(data, requires_grad=True)
            out = torch.log(
                torch.sum(torch.exp(tensor), dim=axes, keepdim=keepdims))
            result.append(out)
            result.append(tensor)

        sum = torch.unsqueeze(torch.sum(
            torch.stack([torch.sum(r) for r in result[0::2]])),
                              dim=0)

        d__o = ref_data.getOutputTensorGrad(0)
        sum.backward(torch.tensor(d__o))
        result[1::2] = [r.grad for r in result[1::2]]

        result = [sum, sum.grad] + result
        return result

    op_tester.passes = ['OpToIdentity']
    op_tester.run(init_builder, reference, 'train')

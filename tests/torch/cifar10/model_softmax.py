# see model0.py for a more detailed
# description of what's going on.
#
# test note (10/01/2019)
# to check that the test is doing what it is supposed to, we check:
# scaling sum_pt in softmaxx.cpp by 1.001 correctly makes the test fail

import sys
import os

import c10driver
import poponnx
import cmdline
from poponnx.torch import torchwriter
#we require torch in this file to create the torch Module
import torch

args = cmdline.parse()

nInChans = 3
nOutChans = 4
batchSize = 2
batchesPerStep = 3
anchors = {
    "l1LossVal0": poponnx.AnchorReturnType("FINAL"),
}
dataFeed = poponnx.DataFlow(batchesPerStep, anchors)
inputShapeInfo = poponnx.InputShapeInfo()
inputShapeInfo.add("image0",
                   poponnx.TensorInfo("FLOAT", [batchSize, nInChans, 32, 32]))

inNames = ["image0"]
cifarInIndices = {"image0": 0}

outNames = ["probs"]
losses = [poponnx.L1Loss("probs", "l1LossVal0", 0.1)]

willowOptPasses = poponnx.Patterns(poponnx.PatternsLevel.ALL)


class Module0(torch.nn.Module):
    def __init__(self):
        torch.nn.Module.__init__(self)
        self.conv1 = torchwriter.conv3x3(nInChans, nOutChans)
        self.conv3 = torchwriter.conv3x3(nOutChans, nOutChans)
        self.relu = torch.nn.functional.relu
        # for softmax dim -1 is correct for [sample][class],
        # gives class probabilities for each sample.
        self.softmax = torch.nn.Softmax(dim=-1)

    def forward(self, inputs):
        image0 = inputs[0]
        x = self.conv1(image0)
        x = self.relu(x)
        x = self.conv3(x)
        preProbSquared = x + x

        window_size = (int(x.size()[2]), int(x.size()[3]))
        x = torch.nn.functional.avg_pool2d(x, kernel_size=window_size)
        x = torch.squeeze(x)
        # probabilities:
        # Note that for Nll, Pytorch requires logsoftmax input.
        # We do this separately the framework dependant section,
        # torchwriter.py
        probs = self.softmax(x)
        # -> currently no support from pytorch
        # -> for gather or log (pytorch 0.4.1)
        # x = torch.gather(input = x, dim = 1, index= labels)
        # loss = torch.log(x)
        return probs


# Set arbitrary seed so model weights are initialized to the
# same values each time the test is run
torch.manual_seed(1)

torchWriter = torchwriter.PytorchNetWriter(
    inNames=inNames,
    outNames=outNames,
    losses=losses,
    optimizer=poponnx.ConstSGD(0.001),
    inputShapeInfo=inputShapeInfo,
    dataFeed=dataFeed,
    ### Torch specific:
    module=Module0(),
    samplesPerBatch=batchSize)

c10driver.run(torchWriter, willowOptPasses, args.outputdir, cifarInIndices,
              args.device, args.hw_id)
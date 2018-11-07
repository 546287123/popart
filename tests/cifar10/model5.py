# see model0.py for a more detailed
# description of what's going on.

import sys
import os
import torch
import c10driver
import pywillow
import torchwriter

if (len(sys.argv) != 2):
    raise RuntimeError("onnx_net.py <log directory>")

outputdir = sys.argv[1]
if not os.path.exists(outputdir):
    print("Making %s" % (outputdir, ))
    os.mkdir(outputdir)

nInChans = 3
nOutChans = 10
samplesPerBatch = 3
batchesPerStep = 2
anchors = ["nllLossVal", "probs"]
art = pywillow.AnchorReturnType.ALL
dataFeed = pywillow.DataFlow(batchesPerStep, samplesPerBatch, anchors, art)
earlyInfo = pywillow.EarlyInfo()
earlyInfo.add(
    "image0", pywillow.TensorInfo("FLOAT",
                                  [samplesPerBatch, nInChans, 32, 32]))
earlyInfo.add("label", pywillow.TensorInfo("INT32", [samplesPerBatch]))
inNames = ["image0"]
cifarInIndices = {"image0": 0, "label": 1}
outNames = ["probs"]
losses = [pywillow.NllLoss("probs", "label", "nllLossVal")]
willowOptPasses = ["PreUniRepl", "PostNRepl", "SoftmaxGradDirect"]


class Module0(torch.nn.Module):
    def __init__(self):
        torch.nn.Module.__init__(self)
        self.conv1 = torchwriter.conv3x3(nInChans, nOutChans)
        self.relu = torch.nn.functional.relu
        # for softmax dim -1 is correct for [sample][class],
        # gives class probabilities for each sample.
        self.softmax = torch.nn.Softmax(dim=-1)
        # self.prebrobs = None

    def forward(self, inputs):
        image0 = inputs[0]
        x = self.conv1(image0)
        #  x = self.relu(x)
        window_size = (int(x.size()[2]), int(x.size()[3]))
        x = torch.nn.functional.avg_pool2d(x, kernel_size=window_size)
        preprobs = torch.squeeze(x)
        # probabilities:
        # Note that for Nll, Pytorch requires logsoftmax input.
        # We do this separately in the framework specfic code,
        # torchwriter.py
        probs = self.softmax(preprobs)
        # -> currently no support from pytorch
        # -> for gather or log (pytorch 0.4.1)
        # x = torch.gather(input = x, dim = 1, index= labels)
        # loss = torch.log(x)
        return probs


torchWriter = torchwriter.PytorchNetWriter(
    inNames=inNames,
    outNames=outNames,
    losses=losses,
    optimizer=pywillow.ConstSGD(0.001),
    earlyInfo=earlyInfo,
    dataFeed=dataFeed,
    ### Torch specific:
    module=Module0())

c10driver.run(torchWriter, willowOptPasses, outputdir, cifarInIndices)

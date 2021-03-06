# Copyright (c) 2019 Graphcore Ltd. All rights reserved.
import numpy as np
import popart
import pytest

# `import test_util` requires adding to sys.path
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).resolve().parent.parent))
import test_util as tu


# test that we can get the graph & summary report after an out of memory exception
# This test currently requires hardware, as the ipu model does not throw an exception
# when it run's out of memory
@tu.requires_ipu
def test_out_of_memory_exception():
    deviceInfo = tu.create_test_device(1)
    if deviceInfo.tilesPerIpu != 1216:
        pytest.skip("Out of memory only on an IPU with 1216 tiles")

    d1 = np.random.rand(2000, 2000).astype(np.float32)
    d2 = np.random.rand(2000, 2000).astype(np.float32)
    d3 = np.random.rand(2000, 2000).astype(np.float32)
    d4 = np.random.rand(2000, 2000).astype(np.float32)

    datas = [np.random.rand(2000, 2000).astype(np.float32) for _ in range(4)]

    builder = popart.Builder()

    i1 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i2 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i3 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i4 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i5 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i6 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i7 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))
    i8 = builder.addInputTensor(popart.TensorInfo("FLOAT", [2000, 2000]))

    a1 = builder.aiOnnx.matmul([i1, i2])
    a2 = builder.aiOnnx.matmul([i3, i4])
    a3 = builder.aiOnnx.add([a1, a2])

    a4 = builder.aiOnnx.matmul([i5, i6])
    a5 = builder.aiOnnx.matmul([i7, i8])
    a6 = builder.aiOnnx.add([a4, a5])

    out = builder.aiOnnx.add([a3, a6])

    builder.addOutputTensor(out)

    options = popart.SessionOptions()
    options.engineOptions = {"debug.allowOutOfMemory": "true"}
    patterns = popart.Patterns(popart.PatternsLevel.NoPatterns)
    patterns.enableRuntimeAsserts(False)

    session = popart.InferenceSession(
        fnModel=builder.getModelProto(),
        dataFlow=popart.DataFlow(1, {out: popart.AnchorReturnType("All")}),
        userOptions=options,
        patterns=patterns,
        deviceInfo=deviceInfo)

    with pytest.raises(popart.poplar_exception) as e:
        session.prepareDevice()
        print("Caught OutOfMemoryException exception {}", e)
        print(e.getSummaryReport())
        print(e.getGraphReport())
        assert e.value.args[0].startswith(
            "Out of memory. Data-stream is larger than remaining host buffer 1 memory."
        )

    session.getTensorTileMap()

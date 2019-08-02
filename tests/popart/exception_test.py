import numpy as np
import popart


# test that we can get the graph & summary report after an out of memory exception
# This test currently requires hardware, as the ipu model does not throw an exception
# when it run's out of memory
def test_out_of_memory_exception():
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

    session = popart.InferenceSession(
        fnModel=builder.getModelProto(),
        dataFeed=popart.DataFlow(1, {out: popart.AnchorReturnType("ALL")}),
        losses=[],
        userOptions=options,
        passes=popart.Patterns(popart.PatternsLevel.NONE),
        deviceInfo=popart.DeviceManager().acquireAvailableDevice(1))

    try:
        session.prepareDevice()
        assert (False)
    except popart.PrepareDeviceException as e:
        print("Caught PrepareDeviceException exception {}", e)
        print(e.getSummaryReport())
        print(e.getGraphReport())

    session.getTensorTileMap()
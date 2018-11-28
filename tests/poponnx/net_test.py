import pytest

import poponnx


def test_net_from_string(tmpdir):

    builder = poponnx.Builder()

    i1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    i2 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    o = builder.add([i1, i2])
    builder.addOutputTensor(o)

    proto = builder.getModelProto()

    earlyInfo = poponnx.EarlyInfo()
    earlyInfo.add("1", poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    earlyInfo.add("2", poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))

    dataFlow = poponnx.DataFlow(1, 1, [], poponnx.AnchorReturnType.ALL)

    poponnx.Session(
        fnModel=proto,
        earlyInfo=earlyInfo,
        dataFeed=dataFlow,
        outputdir=str(tmpdir))


def test_net_from_file(tmpdir):

    builder = poponnx.Builder()

    i1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    i2 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    o = builder.add([i1, i2])
    builder.addOutputTensor(o)

    proto = builder.getModelProto()

    with open("test.onnx", "wb") as f:
        f.write(proto)

    earlyInfo = poponnx.EarlyInfo()
    earlyInfo.add("1", poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    earlyInfo.add("2", poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))

    dataFlow = poponnx.DataFlow(1, 1, [], poponnx.AnchorReturnType.ALL)

    poponnx.Session(
        fnModel="test.onnx",
        earlyInfo=earlyInfo,
        dataFeed=dataFlow,
        outputdir=str(tmpdir))


def test_net_failure(tmpdir):

    earlyInfo = poponnx.EarlyInfo()
    earlyInfo.add("1", poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))
    earlyInfo.add("2", poponnx.TensorInfo("FLOAT", [1, 2, 32, 32]))

    dataFlow = poponnx.DataFlow(1, 1, [], poponnx.AnchorReturnType.ALL)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        poponnx.Session(
            fnModel="nothing",
            earlyInfo=earlyInfo,
            dataFeed=dataFlow,
            outputdir=str(tmpdir))

    assert (e_info.type == poponnx.poponnx_exception)
    assert (e_info.value.args[0] == "Failed to parse ModelProto from string")

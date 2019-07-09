import numpy as np
import pytest
import poponnx


def test_disabled_virtual_graphs():
    """
    In this test we check that an error is thrown when doing pipelining
    if enableVirtualGraph session option is not set to true
    """
    builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss = get_simple_linear_model(
    )

    opts = poponnx.SessionOptionsCore()
    opts.enablePipelining = True
    opts.enableVirtualGraphs = False

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(10, anchor_map),
            userOptions=opts,
            losses=[loss],
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice({}))
    assert (e_info.value.args[0].startswith(
        "Pipelining requires the 'enableVirtualGraphs' session option"))


def test_enabled_recomputation():
    """
    In this test we check that an error is thrown when doing pipelining
    if recomputation is enabled
    """
    builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss = get_simple_linear_model(
    )

    opts = poponnx.SessionOptionsCore()
    opts.enablePipelining = True
    opts.enableVirtualGraphs = True
    opts.autoRecomputation = poponnx.RecomputationType.Standard

    builder.virtualGraph(op0_out, 0)
    builder.virtualGraph(op1_out, 1)
    builder.virtualGraph(op2_out, 1)
    builder.virtualGraph(op3_out, 1)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(10, anchor_map),
            userOptions=opts,
            losses=[loss],
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice({
                'numIPUs':
                2,
                "tilesPerIPU":
                20
            }))
    assert (e_info.value.args[0].startswith(
        "When pipelining is enabled, recomputation is currently not allowed"))


def test_bad_sharding0():
    """
    Non-linear sharding throws error.
    For our graph : Op0 -> Op1 -> Op2 -> Op3 -> Loss
    consider the three cases
      1) IPU0 : {Op2, Op3}, IPU1 : {Op0, Op1, Loss}
      2) IPU0 : {Op0, Op2}, IPU1 : {Op1, Op3, Loss}
      3) IPU0 : {Op0, Op1, Loss}, IPU1 : {Op2, Op3}
    """

    opts = poponnx.SessionOptionsCore()
    opts.enablePipelining = True
    opts.enableVirtualGraphs = True

    # 1)
    builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss = get_simple_linear_model(
    )
    builder.virtualGraph(op0_out, 1)
    builder.virtualGraph(op1_out, 1)
    builder.virtualGraph(op2_out, 0)
    builder.virtualGraph(op3_out, 0)
    loss.virtualGraph(1)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(10, anchor_map),
            userOptions=opts,
            losses=[loss],
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice({
                'numIPUs':
                2,
                "tilesPerIPU":
                20
            }))
    assert (
        e_info.value.args[0].find("forward IPU copies go from IPU N to N+1"))

    # 2)
    builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss = get_simple_linear_model(
    )
    builder.virtualGraph(op0_out, 0)
    builder.virtualGraph(op1_out, 1)
    builder.virtualGraph(op2_out, 0)
    builder.virtualGraph(op3_out, 1)
    loss.virtualGraph(1)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(10, anchor_map),
            userOptions=opts,
            losses=[loss],
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice({
                'numIPUs':
                2,
                "tilesPerIPU":
                20
            }))
    assert (
        e_info.value.args[0].find("forward IPU copies go from IPU N to N+1"))

    # 3)
    builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss = get_simple_linear_model(
    )
    builder.virtualGraph(op0_out, 0)
    builder.virtualGraph(op1_out, 0)
    builder.virtualGraph(op2_out, 1)
    builder.virtualGraph(op3_out, 1)
    loss.virtualGraph(0)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(10, anchor_map),
            userOptions=opts,
            losses=[loss],
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice({
                'numIPUs':
                2,
                "tilesPerIPU":
                20
            }))
    assert (e_info.value.args[0].find(
        "such that the loss is on the final IPU in the pipeline"))


def test_stream_tensors_to_multiple_ipus():
    """
    Streaming an input to Ops on multiple IPUs throws an error

    09/07/2019 Since D12445 this test no longer raises an exception. By
    default, stream tensors are now replicated by streaming to a single
    IPU, then copied across to the other IPUs where they are needed.
    Leaving this test in to verify that this remains the case
    """
    builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss = get_simple_linear_model(
        streamInputToOp1AndOp2=True)

    opts = poponnx.SessionOptionsCore()
    opts.enablePipelining = True
    opts.enableVirtualGraphs = True

    builder.virtualGraph(op0_out, 0)
    builder.virtualGraph(op1_out, 1)
    builder.virtualGraph(op2_out, 1)
    builder.virtualGraph(op3_out, 1)
    loss.virtualGraph(1)

    session = poponnx.InferenceSession(
        fnModel=builder.getModelProto(),
        dataFeed=poponnx.DataFlow(10, anchor_map),
        userOptions=opts,
        losses=[loss],
        deviceInfo=poponnx.DeviceManager().createIpuModelDevice({
            'numIPUs':
            2,
            "tilesPerIPU":
            20
        }))


def test_bad_sharding1():
    """
    Branched sharding throws error
    e.g. Op0 -> Op2
                 ^
         Op1 ----'
    where the vGraph split is IPU0 : {Op0}, IPU1 : {Op1}, IPU2 : {Op2}
    """
    builder = poponnx.Builder()
    shape_d = [10]
    shape_l = [1]
    d0 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", shape_d))
    d1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", shape_d))
    l0 = builder.addInputTensor(poponnx.TensorInfo("INT32", shape_l))

    op0_out = builder.aiOnnx.sin([d0], "s0")
    op1_out = builder.aiOnnx.exp([d1], "r0")
    op2_out = builder.aiOnnx.mul([op0_out, op1_out], "m0")
    builder.addOutputTensor(op2_out)

    art = poponnx.AnchorReturnType("ALL")
    loss = poponnx.NllLoss(op2_out, l0, "loss")
    anchor_map = {op2_out: art, "loss": art}

    opts = poponnx.SessionOptionsCore()
    opts.enablePipelining = True
    opts.enableVirtualGraphs = True

    builder.virtualGraph(op0_out, 0)
    builder.virtualGraph(op1_out, 1)
    builder.virtualGraph(op2_out, 2)
    loss.virtualGraph(2)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(10, anchor_map),
            userOptions=opts,
            losses=[loss],
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice({
                'numIPUs':
                3,
                "tilesPerIPU":
                20
            }))
    assert (
        e_info.value.args[0].find("forward IPU copies go from IPU N to N+1"))


def test_inference_min_batches():
    """
    Check that we throw if too few batches to fill and flush the pipeline
    for an inference model
    """
    minBatches = 3  # numIpus

    get_model_anchors(doSharding=True,
                      doPipelining=True,
                      batchesPerStep=minBatches,
                      doTraining=False,
                      doDevicex=False)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        get_model_anchors(doSharding=True,
                          doPipelining=True,
                          batchesPerStep=minBatches - 1,
                          doTraining=False,
                          doDevicex=False)
    assert (e_info.value.args[0].startswith(
        "For pipelining, depth must be at least"))


def test_training_min_batches():
    """
    Check that we throw if too few batches to fill and flush the pipeline
    for a training model
    """
    minBatches = 5  # 2 * (numIpus-1) + 1

    get_model_anchors(doSharding=True,
                      doPipelining=True,
                      batchesPerStep=minBatches,
                      doTraining=True,
                      doDevicex=False)

    with pytest.raises(poponnx.poponnx_exception) as e_info:
        get_model_anchors(doSharding=True,
                          doPipelining=True,
                          batchesPerStep=minBatches - 1,
                          doTraining=True,
                          doDevicex=False)
    assert (e_info.value.args[0].startswith(
        "For pipelining, depth must be at least"))


def test_output_matches_train():
    """
    In this test we check that the anchors of equivalent non-sharded, sharded
    and non-pipelined, and sharded and pipelined models are equal when doing
    training. We expect only the first output and weight update to be the same
    as non-pipelined models
    """
    bps = 8
    singleIpu_anchors = get_model_anchors(doSharding=False,
                                          doPipelining=False,
                                          batchesPerStep=bps,
                                          doTraining=True)
    multiIpu_anchors = get_model_anchors(doSharding=True,
                                         doPipelining=False,
                                         batchesPerStep=bps,
                                         doTraining=True)
    pipelined_anchors = get_model_anchors(doSharding=True,
                                          doPipelining=True,
                                          batchesPerStep=bps,
                                          doTraining=True)
    # TODO, depends on T9630, add a case with grad accumulation. All tensor
    # outputs should be exactly the same when doing pipelined vs non-pipelined
    # when grad accumulation is turned on

    for (tId1, t1), (tId2, t2) in zip(singleIpu_anchors.items(),
                                      multiIpu_anchors.items()):
        assert (np.allclose(t1, t2))

    # Expect only the anchors from the first batch to be equal. After that, the
    # continuous gradient accumulation option causes model parameters to diverge
    # for (tId1,t1), (tId2,t2) in zip(singleIpu_anchors.items(), pipelined_anchors.items()):
    #     for i in range(np.shape(t1)[0]):
    #         print("singleIpu   , batch: ", i, tId1, np.sum(t1[i]))
    #         print("pipelinedIpu, batch: ", i, tId2, np.sum(t2[i]))
    #     assert(np.allclose(t1[0], t2[0]))


def test_output_matches_infer():
    """
    In this test we check that the anchors of equivalent non-sharded, sharded
    and non-pipelined, and sharded and pipelined models are equal when doing
    inference
    """
    bps = 8
    singleIpu_anchors = get_model_anchors(doSharding=False,
                                          doPipelining=False,
                                          batchesPerStep=bps,
                                          doTraining=False)
    multiIpu_anchors = get_model_anchors(doSharding=True,
                                         doPipelining=False,
                                         batchesPerStep=bps,
                                         doTraining=False)
    pipelined_anchors = get_model_anchors(doSharding=True,
                                          doPipelining=True,
                                          batchesPerStep=bps,
                                          doTraining=False)

    for (tId1, t1), (tId2, t2) in zip(singleIpu_anchors.items(),
                                      multiIpu_anchors.items()):
        assert (np.allclose(t1, t2))
    for (tId1, t1), (tId2, t2) in zip(singleIpu_anchors.items(),
                                      pipelined_anchors.items()):
        assert (np.allclose(t1, t2))


# Model
#  <--- ipu0 ----> <--------- ipu1 ---> <------------ ipu2 ------------>
#
#  d0 --|-- Sin --|-- Exp --|
#                           |-- Conv --|-- Reshape --|-- Softmax --> out
#                      w0 --|
def get_model_anchors(doSharding,
                      doPipelining,
                      batchesPerStep,
                      doTraining,
                      doProfiling=False,
                      doDevicex=True):
    builder = poponnx.Builder()
    batchSize = 2
    shape_d0 = [batchSize, 2, 4, 4]
    shape_l0 = [batchSize]
    d0 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", shape_d0))
    data_w0 = np.ones(shape=[2, 2, 3, 3]).astype(np.float32)
    w0 = builder.addInitializedInputTensor(data_w0)
    if doTraining is True:
        l0 = builder.addInputTensor(poponnx.TensorInfo("INT32", shape_l0))

    s0 = builder.aiOnnx.sin([d0], "s0")
    e0 = builder.aiOnnx.exp([s0], "e0")
    c0 = builder.aiOnnx.conv([e0, w0],
                             dilations=[1, 1],
                             pads=[1, 1, 1, 1],
                             strides=[1, 1],
                             debugPrefix="c0")
    r0 = builder.reshape_const(builder.aiOnnx, [c0], [batchSize, 32])
    out = builder.aiOnnx.softmax([r0], axis=1, debugPrefix="sfm")
    builder.addOutputTensor(out)

    art = poponnx.AnchorReturnType("ALL")
    if doTraining is True:
        loss = poponnx.NllLoss(out, l0, "loss")
        d0_grad = poponnx.reservedGradientPrefix() + d0
        anchor_map = {d0_grad: art, out: art, "loss": art}
    else:
        anchor_map = {out: art}

    opts = poponnx.SessionOptionsCore()
    opts.reportOptions = {"showExecutionSteps": "true"}
    opts.enablePipelining = doPipelining

    if doSharding is False:
        deviceOpts = {'numIPUs': 1, "tilesPerIPU": 20}
    else:
        opts.enableVirtualGraphs = True
        deviceOpts = {'numIPUs': 3, "tilesPerIPU": 20}
        builder.virtualGraph(s0, 0)
        builder.virtualGraph(e0, 1)
        builder.virtualGraph(c0, 1)
        builder.virtualGraph(r0, 2)
        builder.virtualGraph(out, 2)
        if doTraining is True:
            loss.virtualGraph(2)

    if doTraining is True:
        session = poponnx.TrainingSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(batchesPerStep, anchor_map),
            losses=[loss],
            optimizer=poponnx.ConstSGD(0.01),
            userOptions=opts,
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice(
                deviceOpts))
    else:
        session = poponnx.InferenceSession(
            fnModel=builder.getModelProto(),
            dataFeed=poponnx.DataFlow(batchesPerStep, anchor_map),
            userOptions=opts,
            deviceInfo=poponnx.DeviceManager().createIpuModelDevice(
                deviceOpts))

    if doDevicex is False:
        return None

    anchors = session.initAnchorArrays()
    session.prepareDevice()

    if batchesPerStep > 1:
        shape_d0.insert(0, batchesPerStep)
        shape_l0.insert(0, batchesPerStep)
    data = 2 * np.ones(shape_d0, dtype=np.float32)
    label = np.ones(shape_l0, dtype=np.int32)

    if doTraining is True:
        inputs = {d0: data, l0: label}
    else:
        inputs = {d0: data}
    stepio = poponnx.PyStepIO(inputs, anchors)

    session.weightsFromHost()
    if doTraining is True:
        session.optimizerFromHost()
    session.run(stepio)

    if doProfiling is True:
        from gcprofile import save_poponnx_report
        save_poponnx_report(session)

    return anchors


def get_simple_linear_model(streamInputToOp1AndOp2=False):
    builder = poponnx.Builder()
    shape_d = [10]
    shape_l = [1]
    d0 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", shape_d))
    d1 = builder.addInputTensor(poponnx.TensorInfo("FLOAT", shape_d))
    l0 = builder.addInputTensor(poponnx.TensorInfo("INT32", shape_l))

    op0_out = builder.aiOnnx.sin([d0], "s0")
    if streamInputToOp1AndOp2 is True:
        op1_out = builder.aiOnnx.mul([op0_out, d0])
    else:
        op1_out = builder.aiOnnx.mul([op0_out, d1])
    op2_out = builder.aiOnnx.exp([op1_out], "e0")
    op3_out = builder.aiOnnx.exp([op2_out], "e1")
    builder.addOutputTensor(op3_out)

    art = poponnx.AnchorReturnType("ALL")
    loss = poponnx.NllLoss(op3_out, l0, "loss")
    anchor_map = {op3_out: art, "loss": art}

    return builder, op0_out, op1_out, op2_out, op3_out, anchor_map, loss

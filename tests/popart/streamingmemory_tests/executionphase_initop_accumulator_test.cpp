// Copyright (c) 2020 Graphcore Ltd. All rights reserved.
#define BOOST_TEST_MODULE ExecutionPhaseInitOpAccumulatorTest

#include <boost/test/unit_test.hpp>
#include <iostream>
#include <popart/builder.hpp>
#include <popart/dataflow.hpp>
#include <popart/filereader.hpp>
#include <popart/ir.hpp>
#include <popart/op/init.hpp>
#include <popart/op/l1.hpp>
#include <popart/optimizer.hpp>
#include <popart/popx/devicex.hpp>
#include <popart/testdevice.hpp>

using namespace popart;
using namespace std;

// This test simulates matmul with accumulation over multiple execution phases.
// It checks that the InitOp tensor layout is efficient for this use-case.
//
// 1/ the even and odd execution phases accumulate their own matmul result.
// 2/ a final reduce is made to sum even and odd results.
//
// The initOp should be laid out efficiently to minimise internal exchange
// for the addOp. In this case that requires a layout according to the addOp's
// RHS layout. This is the result of the matmul, so should be non-linear.
BOOST_AUTO_TEST_CASE(TestInitOpAccumulator) {
  int N         = 4;
  int size      = 4;
  bool training = false;

  auto builder     = Builder::create();
  auto aiOnnx      = builder->aiOnnxOpset9();
  auto aiGraphcore = builder->aiGraphcoreOpset1();

  // Weights are [size, size]
  TensorInfo wInfo{"FLOAT", std::vector<int64_t>{size, size}};
  std::vector<float> wData(wInfo.nelms(), 0);
  ConstVoidData wCVData{wData.data(), wInfo};

  // Input are [size, size]
  TensorInfo inInfo{"FLOAT", std::vector<int64_t>{size, size}};
  auto input = builder->addInputTensor(inInfo);

  // Even Accumulations are [size, size]
  auto init0 = aiGraphcore.init(std::vector<int64_t>{size, size},
                                static_cast<int64_t>(DataType::FLOAT),
                                static_cast<int64_t>(InitType::Zero),
                                "even_accumulator");
  builder->executionPhase(init0, 0);
  builder->virtualGraph(init0, 0);

  // Odd Accumulations are [size, size]
  auto init1 = aiGraphcore.init(std::vector<int64_t>{size, size},
                                static_cast<int64_t>(DataType::FLOAT),
                                static_cast<int64_t>(InitType::Zero),
                                "odd_accumulator");
  builder->executionPhase(init1, 1);
  builder->virtualGraph(init1, 1);

  // 2N phases
  for (int n = 0; n < 2 * N; ++n) {
    const VGraphId vgid = (n % 2);

    // Weights are common.
    auto w = builder->addInitializedInputTensor(wCVData);

    // inputs x w
    auto out = aiOnnx.matmul(
        {input, w}, logging::format("CHECKOP_MM: [{} {}]", n, vgid % 2));
    builder->executionPhase(out, n);
    builder->virtualGraph(out, vgid);

    out = aiOnnx.relu({out},
                      logging::format("CHECKOP_RELU: [{} {}]", n, vgid % 2));
    builder->executionPhase(out, n);
    builder->virtualGraph(out, vgid);

    if (n % 2) {
      // Odd phase accumulation
      init1 = aiOnnx.add({init1, out},
                         logging::format("CHECKOP_ACC: [{} {}]", n, vgid % 2));
      builder->executionPhase(init1, n);
      builder->virtualGraph(init1, vgid);
    } else {
      // Even phase accumulation (swapped operand order)
      init0 = aiOnnx.add({out, init0},
                         logging::format("CHECKOP_ACC: [{} {}]", n, vgid % 2));
      builder->executionPhase(init0, n);
      builder->virtualGraph(init0, vgid);
    }
  }

  // Reduce by adding even and odd accumulators together (within final phase)
  auto init = aiOnnx.add({init0, init1}, "reduce_add");
  builder->executionPhase(init, N * 2 - 1);
  builder->virtualGraph(init, 1);

  SessionOptions session_opts;

  // Large model settings
  session_opts.enableOutlining = true;
  session_opts.aliasZeroCopy   = true;
  session_opts.constantWeights = false;

  // TODO: T17972
  // AddOpx::getInputCreatorType currently has a constraint such that
  // it will only unwind if decomposeGradSum or batchSerialization is enabled.
  session_opts.decomposeGradSum = true;

  session_opts.executionPhaseSettings.phases = N * 2;
  session_opts.virtualGraphMode = VirtualGraphMode::ExecutionPhases;

  auto testDev = createTestDevice(TEST_TARGET, 2);

  Ir ir;
  ir.prepare({
      io::getModelFromString(builder->getModelProto()),
      InputShapeInfo(),
      DataFlow(1, {{init, AnchorReturnType("ALL")}}),
      {}, // no loss
      {}, // no optimizer
      *testDev,
      session_opts,
      {}, // Patterns({}).enableRuntimeAsserts(false)
  });

  // Compile.
  std::unique_ptr<popx::Devicex> device;
  device.reset(new popx::Devicex(ir, testDev));
  device->prepare();

  // Get resultant tensors (efficient v linear).
  auto efficient = device->getEfficientlyCreatedInputTensors();
  auto linear    = device->getLinearlyCreatedInputTensors();

  auto print = [](const string &prefix, const auto &ts) {
    cout << prefix;
    for (auto &&t : ts)
      cout << t << " ";
    cout << endl;
  };

  print("Efficient:", efficient);
  print("Linear:", linear);

  // Check the linearly mapped set.
  // Only "RemoteArg" or "Constant" should appear here.
  for (auto &&t : linear) {
    if (t.find("RemoteArg") == std::string::npos &&
        t.find("Constant") == std::string::npos)
      BOOST_ERROR(
          logging::format("Tensor should not be mapped linearly: {}", t));
  }
}

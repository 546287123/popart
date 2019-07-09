#define BOOST_TEST_MODULE Pipelining

#include <boost/test/unit_test.hpp>
#include <poponnx/builder.hpp>
#include <poponnx/dataflow.hpp>
#include <poponnx/device.hpp>
#include <poponnx/devicemanager.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/names.hpp>
#include <poponnx/op/nll.hpp>
#include <poponnx/op/stash.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/session.hpp>
#include <poponnx/tensordata.hpp>
#include <poponnx/tensornames.hpp>

using namespace poponnx;

// model:
//  <--- ipu0 ----> <--------- ipu1 ---> <------------ ipu2 ------------>
//
//  d0 --|-- Sin --|-- Exp --|
//                           |-- Conv --|-- Reshape --|-- Softmax --> out
//                      w0 --|
BOOST_AUTO_TEST_CASE(test) {

  auto builder = Builder::create();
  auto aiOnnx  = builder->aiOnnxOpset9();

  // data
  TensorInfo info_d{"FLOAT", std::vector<int64_t>{1, 2, 4, 4}};
  auto d0 = builder->addInputTensor(info_d);

  // label
  TensorInfo info_l{"INT32", std::vector<int64_t>{1}};
  auto l0 = builder->addInputTensor(info_l);

  // weights
  TensorInfo info_w{"FLOAT", std::vector<int64_t>{2, 2, 3, 3}};
  float vals_w[2 * 2 * 3 * 3] = {1};
  ConstVoidData weight_data   = {vals_w, info_w};
  auto w0                     = builder->addInitializedInputTensor(weight_data);

  auto s0  = aiOnnx.sin({d0}, "s0");
  auto e0  = aiOnnx.exp({s0}, "e0");
  auto c0  = aiOnnx.conv({e0, w0}, {1, 1}, 1, {}, {1, 1, 1, 1}, {1, 1}, "c0");
  auto r0  = builder->reshape_const(aiOnnx, {c0}, {1, 32});
  auto out = aiOnnx.softmax({r0}, 1, "sfm");
  builder->addOutputTensor(out);

  std::map<std::string, std::string> deviceOpts{{"numIPUs", "3"},
                                                {"tilesPerIPU", "20"}};
  auto deviceInfo =
      DeviceManager::createDeviceManager().createIpuModelDevice(deviceOpts);

  auto optimizer = ConstSGD(0.01);
  Loss *loss(new NllLoss(out, l0, "loss", ReductionType::MEAN));

  auto art                                       = AnchorReturnType("ALL");
  std::map<TensorId, AnchorReturnType> anchorMap = {
      {out, art}, {reservedGradientPrefix() + d0, art}};

  auto opts                = SessionOptions();
  opts.enableVirtualGraphs = true;
  opts.enablePipelining    = true;
  builder->virtualGraph(s0, 0);
  builder->virtualGraph(e0, 1);
  builder->virtualGraph(c0, 1);
  builder->virtualGraph(r0, 2);
  builder->virtualGraph(out, 2);
  loss->virtualGraph(2);

  Ir ir;
  ir.prepare({io::getModelFromString(builder->getModelProto()),
              InputShapeInfo(),
              DataFlow(5, anchorMap), // We will process 5 batches
              {loss},
              &optimizer,
              *deviceInfo,
              opts,
              poponnx::Patterns(PatternsLevel::DEFAULT)});

  // What do we expect the transformation to do?
  // There are 4 activation/stream/variable tensors that are required
  // in the bwd pass:
  // 1. d0, to compute d0_grad
  // 2. e0, to compute s0_grad
  // 3. w0, to compute e0_grad
  // 4. out, to compute out_grad

  // However...
  // Don't stash (3):
  //   w0 is a weight tensor. We don't stash weights due to their
  //   memory requirement. If doing grad accumulation, w0 is
  //   constant for all pipeline cycles in a step. If not, then we
  //   are calculating an approximate e0_grad
  // Don't stash (4):
  //   out is on the final IPU in the pipeline, so is not
  //   stashed

  // So we expect the transformation to stash (1) and (2)
  BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::Stash).size() == 2);
  BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::Restore).size() == 2);

  BOOST_CHECK(ir.isConsumedByOpOfType(d0, Onnx::CustomOperators::Stash));
  BOOST_CHECK(ir.isConsumedByOpOfType(d0, Onnx::CustomOperators::Restore));

  BOOST_CHECK(ir.isConsumedByOpOfType(e0, Onnx::CustomOperators::Stash));
  BOOST_CHECK(ir.isConsumedByOpOfType(e0, Onnx::CustomOperators::Restore));

  // The model is split over 3 IPUs.
  // d0 is on IPU0, so its expected stash size is (2 - 0)*2 + 1 = 5
  // e0 is on IPU1, so its expected stash size is (1 - 0)*2 + 1 = 3
  for (Op *op : ir.opsOfType(Onnx::CustomOperators::Stash)) {
    auto stashOp = dynamic_cast<StashOp *>(op);
    if (stashOp->inId(0) == d0) {
      BOOST_CHECK(stashOp->getStashSize() == 5);
    } else if (stashOp->inId(0) == e0) {
      BOOST_CHECK(stashOp->getStashSize() == 3);
    } else {
      throw error("Stash op should consume one of those tensors");
    }
  }
}

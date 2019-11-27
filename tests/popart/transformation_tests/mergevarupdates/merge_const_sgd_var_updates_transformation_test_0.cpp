#define BOOST_TEST_MODULE MergeConstSgdVarUpdatesTransformation0

#include <boost/test/unit_test.hpp>
#include <popart/builder.hpp>
#include <popart/dataflow.hpp>
#include <popart/devicemanager.hpp>
#include <popart/filereader.hpp>
#include <popart/inputshapeinfo.hpp>
#include <popart/ndarraywrapper.hpp>
#include <popart/op/l1.hpp>
#include <popart/optimizer.hpp>
#include <popart/session.hpp>
#include <popart/tensorinfo.hpp>
#include <popart/tensornames.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <tuple>
#include <vector>

using namespace popart;

namespace {
TensorId conv(Builder *b, TensorId act, ConstVoidData wdata) {
  auto aiOnnx  = b->aiOnnxOpset9();
  auto weights = b->addInitializedInputTensor(wdata);
  act = aiOnnx.conv({act, weights}, {1, 1}, 1, {}, {1, 1, 1, 1}, {1, 1});
  return act;
}
} // namespace

BOOST_AUTO_TEST_CASE(Transformation_MergeConstSGD0) {

  auto test = [](MergeVarUpdateType mvu) {
    // we will generate random input data
    int seed = 1013;
    std::default_random_engine eng(seed);
    std::uniform_real_distribution<float> fdis(-4, 4);

    // construct onnx model
    auto builder = Builder::create();
    auto aiOnnx  = builder->aiOnnxOpset9();

    int nInChans  = 3;
    int batchsize = 1;
    TensorInfo in0info{"FLOAT",
                       std::vector<int64_t>{batchsize, nInChans, 32, 32}};
    auto in0 = builder->addInputTensor(in0info);
    std::vector<float> in0data(in0info.nelms());
    for (auto &val : in0data) {
      val = fdis(eng);
    }

    // The model will be reduce(conv(conv(conv(conv(....(conv(input))...)))),
    // nConv convolutions chained together, with the number of
    // channels increasing by 1 at each subsequent conv.
    constexpr int nConv = 11;
    std::array<std::vector<float>, nConv> weights;
    std::array<ConstVoidData, nConv> cvds;
    std::array<TensorId, nConv + 1> actIds;
    actIds[0] = in0;

    int64_t totalWeightElms = 0;

    for (int i = 0; i < nConv; ++i) {
      int nOutChans = nInChans + 1;
      TensorInfo weight_info{"FLOAT",
                             std::vector<int64_t>{nOutChans, nInChans, 1, 1}};

      totalWeightElms += nOutChans * nInChans;
      weights[i] = std::vector<float>(nOutChans * nInChans, 0);
      for (auto &x : weights[i]) {
        x = fdis(eng);
      }
      cvds[i]       = {weights[i].data(), weight_info};
      actIds[i + 1] = conv(builder.get(), actIds[i], cvds[i]);
      nInChans      = nOutChans;
    }

    auto reduced      = aiOnnx.reducesum({actIds[nConv]}, {1, 2, 3});
    std::string proto = builder->getModelProto();
    auto modelProto   = io::getModelFromString(proto);

    // create the IR
    auto art      = AnchorReturnType("ALL");
    auto dataFlow = DataFlow(1, {{reduced, art}});

    auto cpuDevice =
        popart::DeviceManager::createDeviceManager().createCpuDevice();

    auto opts                       = SessionOptions();
    opts.enableOutlining            = false;
    opts.mergeVarUpdate             = mvu;
    opts.mergeVarUpdateMemThreshold = 100;

    float lossLambda = 0.26;
    float learnRate  = 0.1;
    auto optimizer   = ConstSGD(learnRate);
    std::vector<Loss *> losses{
        new L1Loss(reduced, "l1LossVal", lossLambda, ReductionType::SUM)};

    Ir ir;
    ir.prepare({modelProto,
                InputShapeInfo(),
                dataFlow,
                losses,
                &optimizer,
                *cpuDevice,
                opts,
                Patterns({})});

    // Check the ir
    //
    // For case MergeVarUpdateType::All, all the ConstSgdVarUpdates have the
    // same learning rate, weight decay, so should all be merged into a single
    // group
    if (mvu == MergeVarUpdateType::All) {
      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::SGD0VarUpdate).size() ==
                  1);
      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::FlattenInplace).size() ==
                  nConv * 2);

      // 2 ConcatInplace, one for all the Vars and one for all the Grads
      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::ConcatInplace).size() ==
                  2);

      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::CopyVarUpdate).size() ==
                  0);

    } else if (mvu == MergeVarUpdateType::None) {
      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::SGD0VarUpdate).size() ==
                  nConv);
      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::FlattenInplace).size() ==
                  0);
      BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::ConcatInplace).size() ==
                  0);
    }

    else if (mvu == MergeVarUpdateType::AutoTight) {

      auto nConstSgds =
          ir.opsOfType(Onnx::CustomOperators::SGD0VarUpdate).size();
      std::cout << "total weight elms: " << totalWeightElms
                << " threshold: " << opts.mergeVarUpdateMemThreshold
                << " nConstSgds : " << nConstSgds;
      auto weightsMem   = 4 * totalWeightElms;
      bool notDivisible = (weightsMem % opts.mergeVarUpdateMemThreshold) != 0;
      BOOST_CHECK((notDivisible +
                   weightsMem / opts.mergeVarUpdateMemThreshold) == nConstSgds);
    }
  };

  std::cout << "Test AutoTight" << std::endl;
  test(MergeVarUpdateType::AutoTight);

  std::cout << "Test All" << std::endl;
  test(MergeVarUpdateType::All);

  std::cout << "Test None" << std::endl;
  test(MergeVarUpdateType::None);
}

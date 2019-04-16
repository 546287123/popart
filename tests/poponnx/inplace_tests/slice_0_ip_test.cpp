#define BOOST_TEST_MODULE Slice0InplaceTest

#include <boost/test/unit_test.hpp>
#include <vector>
#include <poponnx/builder.hpp>
#include <poponnx/dataflow.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorinfo.hpp>
#include <poponnx/tensornames.hpp>
#include <poponnx/tensors.hpp>

using namespace poponnx;

BOOST_AUTO_TEST_CASE(Inplace_slice0) {

  //             |- [Slice [(0->3), (0->3)]] -|
  //  in0 (3,6) -|                            |-- [MatMul] ---
  //             |- [Slice [(0->3), (3->6)]] -|
  //
  // We expect both Slice ops to be inplaced when enableInPlace is true

  auto test = [](bool enable_inplace) {
    // Build an onnx model
    auto builder = Builder::create();
    auto aiOnnx  = builder->aiOnnxOpset9();

    TensorInfo shape0{"FLOAT", std::vector<int64_t>{3, 6}};

    auto in0 = builder->addInputTensor(shape0);
    // the API required (name) (ends) (starts)
    auto sl0 = aiOnnx.slice({in0}, {6, 3}, {0, 0});
    auto sl1 = aiOnnx.slice({in0}, {6, 6}, {0, 3});
    auto out = aiOnnx.matmul({sl0, sl1});
    builder->addOutputTensor(out);

    auto proto      = builder->getModelProto();
    auto modelProto = io::getModelFromString(proto);

    // Create the IR
    auto dataFlow = DataFlow(1, {{out, AnchorReturnType("ALL")}});

    auto cpuDevice = DeviceManager::createDeviceManager().createCpuDevice();

    Ir ir;
    ir.prepare({modelProto,
                InputShapeInfo(),
                dataFlow,
                {},
                nullptr,
                *cpuDevice,
                {},
                Patterns(PatternsLevel::NONE).enableInPlace(enable_inplace)});

    // Check the ir
    // All the Relus have been optimised out if enable_inplace
    BOOST_CHECK(ir.opsOfType(Onnx::AiOnnx::OpSet9::Slice).size() ==
                (enable_inplace ? 0 : 2));
    // and have been replaced with ReluInplace if enable_inplace
    BOOST_CHECK(ir.opsOfType(Onnx::CustomOperators::SliceInplace).size() ==
                (enable_inplace ? 2 : 0));
  };

  // test with inplacing enabled,
  test(true);
  // test with inplacing disabled.
  test(false);
}

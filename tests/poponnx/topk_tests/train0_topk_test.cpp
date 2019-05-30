#define BOOST_TEST_MODULE Train0TopkTest

#include <boost/test/unit_test.hpp>
#include <poponnx/builder.hpp>
#include <poponnx/dataflow.hpp>
#include <poponnx/devicemanager.hpp>
#include <poponnx/filereader.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/ndarraywrapper.hpp>
#include <poponnx/op/l1.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/session.hpp>
#include <poponnx/tensorinfo.hpp>
#include <poponnx/tensornames.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <tuple>
#include <vector>

BOOST_AUTO_TEST_CASE(Train0TopK) {

  // input tensor X
  // The input tensor will be rank 3, D0 x D1 x D2 x D3
  int D0 = 3;
  int D1 = 7;
  int D2 = 2;
  int D3 = 5;

  // We will perform top-k on axis "axis = 1" and so the output will
  // be of size D0 x top_k x D2 x D3, where top_k <= D1.
  int axis = 1;
  std::vector<int> top_ks{1, 3, D1};

  auto test = [D0, D1, D2, D3, axis](int top_k) {
    // basic differentiation shows that we expect
    // d_topk_out = 2*scale*lossLambda*topk_out

    using namespace poponnx;

    // we will generate random input data
    int seed = 1013;
    std::default_random_engine eng(seed);
    std::uniform_real_distribution<float> fdis(-4, 4);

    // prepare to build an onnx model
    auto builder = Builder::create();
    auto aiOnnx  = builder->aiOnnxOpset9();

    TensorInfo xInfo{"FLOAT", std::vector<int64_t>{D0, D1, D2, D3}};
    TensorId xId = builder->addInputTensor(xInfo);
    std::vector<float> vXData(xInfo.nelms());
    for (auto &val : vXData) {
      val = fdis(eng);
    }

    // the networt scale((topk)**2) with loss L1.
    float scaleFactor = 3.0f;
    float lossLambda  = 0.26;

    Shape outShape = xInfo.shape();
    outShape[axis] = top_k;
    TensorInfo outValuesInfo{xInfo.dataType(), outShape};

    // Prepare the baseline data, perform D0*D2
    // sorts of vectors of size D1 to get the values and the indices
    std::vector<int> expectedOutputIndices(outValuesInfo.nelms(), -1);
    std::vector<float> expectedOutputValues(outValuesInfo.nelms(), -1.0f);
    std::vector<float> expectedInputGradients(xInfo.nelms(), 0.0f);
    int stride0 = D1 * D2 * D3;
    int stride1 = D2 * D3;
    int stride2 = D3;

    int outStride0 = top_k * D2 * D3;
    int outStride1 = D2 * D3;
    int outStride2 = D3;

    for (int d0 = 0; d0 < D0; ++d0) {
      for (int d2 = 0; d2 < D2; ++d2) {
        for (int d3 = 0; d3 < D3; ++d3) {

          std::vector<std::tuple<float, int>> toSort(D1);
          for (int d1 = 0; d1 < D1; ++d1) {
            toSort[d1] = std::tuple<float, int>(
                vXData[d0 * stride0 + d1 * stride1 + d2 * stride2 + d3], d1);
          }

          // sort, largest to smallest
          std::sort(toSort.rbegin(), toSort.rend());

          // populate expected values
          for (int d1 = 0; d1 < top_k; ++d1) {
            int sortIndex = std::get<1>(toSort[d1]);
            auto sortVal  = std::get<0>(toSort[d1]);

            int index =
                d0 * outStride0 + d1 * outStride1 + d2 * outStride2 + d3;
            expectedOutputIndices[index] = sortIndex;
            expectedOutputValues[index]  = sortVal;
            expectedInputGradients[d0 * stride0 + sortIndex * stride1 +
                                   d2 * stride2 + d3] =
                2 * scaleFactor * lossLambda * std::get<0>(toSort[d1]);
          }
        }
      }
    }

    auto topkOut = aiOnnx.topk({xId}, top_k, axis);

    auto values  = topkOut[0];
    auto indices = topkOut[1];

    auto squaredOut = aiOnnx.mul({values, values});
    auto halvedOut  = aiOnnx.scale({squaredOut}, scaleFactor);

    builder->addOutputTensor(halvedOut);

    auto proto      = builder->getModelProto();
    auto modelProto = io::getModelFromString(proto);

    // create the IR
    auto art      = AnchorReturnType("ALL");
    auto dataFlow = DataFlow(1, {{reservedGradientPrefix() + xId, art}});

    auto cpuDevice =
        poponnx::DeviceManager::createDeviceManager().createCpuDevice();

    auto opts = SessionOptions();

    float learnRate = 0.1;
    auto optimizer  = ConstSGD(learnRate);
    std::vector<Loss *> losses{
        new L1Loss(halvedOut, "l1LossVal", lossLambda, ReductionType::SUM)};

    auto session = poponnx::TrainingSession::createFromOnnxModel(
        proto,
        dataFlow,
        losses,
        optimizer,
        cpuDevice,
        poponnx::InputShapeInfo(),
        opts,
        poponnx::Patterns(PatternsLevel::DEFAULT));

    // prepare the anchors. We test just the
    // gradient of output values.
    std::vector<float> rawXGrad(xInfo.nelms());
    poponnx::NDArrayWrapper<float> xGrad(rawXGrad.data(), xInfo.shape());

    std::map<poponnx::TensorId, poponnx::IArray &> anchors = {
        {reservedGradientPrefix() + xId, xGrad}};

    session->prepareDevice();

    poponnx::NDArrayWrapper<float> xData(vXData.data(), xInfo);

    std::map<poponnx::TensorId, poponnx::IArray &> inputs = {{xId, xData}};

    poponnx::StepIO stepio(inputs, anchors);

    session->run(stepio);

    // confirm the gradient values agree (exactly...)
    BOOST_CHECK_EQUAL_COLLECTIONS(rawXGrad.begin(),
                                  rawXGrad.end(),
                                  expectedInputGradients.begin(),
                                  expectedInputGradients.end());
  };

  for (int top_k : top_ks) {
    test(top_k);
  }
}

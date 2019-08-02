#define BOOST_TEST_MODULE PipelineTrainingTest0

#include <algorithm>
#include <boost/test/unit_test.hpp>
#include <map>
#include <random>
#include <tuple>
#include <vector>
#include <popart/builder.hpp>
#include <popart/dataflow.hpp>
#include <popart/devicemanager.hpp>
#include <popart/filereader.hpp>
#include <popart/inputshapeinfo.hpp>
#include <popart/ndarraywrapper.hpp>
#include <popart/op/ipucopy.hpp>
#include <popart/op/l1.hpp>
#include <popart/optimizer.hpp>
#include <popart/session.hpp>
#include <popart/tensorinfo.hpp>
#include <popart/tensornames.hpp>

// In this model, where continuous and exact pipelines are numerically
// equivalent, there are Ops in the forwards and backwards passes which are
// discontiguous. We test that the Ir transformation of inserting IPUCopys are
// correct, as well as that the numerical output agrees between the exact and
// continuous cases.
BOOST_AUTO_TEST_CASE(DiscontiguousIpuCopyTest0) {

  using namespace popart;

  bool printStdOut = true;

  enum class TestType { Numerical, Ir };

  auto test = [printStdOut](TestType tt) {
    // input stream samples are generated randomly
    int seed = 1011;
    std::default_random_engine eng(seed);
    std::uniform_real_distribution<float> fdis(0, 1);

    int64_t batchSize      = 4;
    int64_t batchesPerStep = 400;
    int64_t sampleHeight   = 3;
    int64_t samplesPerStep = batchesPerStep * batchSize;
    std::vector<int64_t> sampleShape{sampleHeight, sampleHeight};
    std::vector<int64_t> weightShape = sampleShape;
    std::vector<int64_t> batchShape{batchSize, sampleHeight, sampleHeight};
    std::vector<int64_t> stepDataShape{
        batchesPerStep, batchSize, sampleHeight, sampleHeight};
    TensorInfo sampleInfo{"FLOAT", sampleShape};
    TensorInfo weightInfo = sampleInfo;
    TensorInfo batchInfo{"FLOAT", batchShape};
    TensorInfo stepDataInfo{"FLOAT", stepDataShape};
    int64_t sampleElms{sampleHeight * sampleHeight};
    int64_t weightsElms  = sampleElms;
    int64_t batchElms    = sampleElms * batchSize;
    int64_t stepDataElms = batchElms * batchesPerStep;

    // The model:
    //
    //  input1              input2
    //    |                   |
    //   (Add) -- Weight     (Add) -- Weight
    //    |                   |
    //   (Add) -- Weight     (Add) -- Weight
    //    |                   |
    //   (Add) -- Weight     (Add) -- Weight
    //    |                   |
    //   (Add) -- Weight     (Add) -- Weight
    //    |                   |
    //   (Add) -- Weight     (Add) -- Weight
    //    \                   |
    //     \                  |
    //      \----------(Add)--|
    //                   |
    //                finalOut
    //                   |
    //                 l1-loss
    //
    // Having two branches like this ensures that there is a discontiguous
    // IPUCopy (from one the 2 branches to the IPU where the loss is computed)

    // number of Adds on each of the two branches.
    int nLayers = 10;

    auto builder     = Builder::create();
    auto aiOnnx      = builder->aiOnnxOpset9();
    auto aiGraphcore = builder->aiGraphcoreOpset1();
    auto input0      = builder->addInputTensor(batchInfo, "0tupni");
    auto input1      = builder->addInputTensor(batchInfo, "1tupni");

    // Storage for all layers
    std::vector<std::vector<float>> allWeights;
    std::vector<ConstVoidData> allWeightCvds;
    std::vector<TensorId> allWeightIds;
    std::vector<TensorId> allActIds;
    std::vector<std::vector<float>> w_readbacks;
    WeightsIO weightsRead;

    int nLayersAdded    = 0;
    auto insertAddLayer = [&allWeights,
                           &allWeightCvds,
                           &allWeightIds,
                           &allActIds,
                           sampleElms,
                           &weightsRead,
                           weightInfo,
                           sampleInfo,
                           &builder,
                           &aiOnnx,
                           &nLayersAdded,
                           &w_readbacks](TensorId actInId) {
      w_readbacks.push_back(std::vector<float>(sampleElms, -99.0f));
      allWeights.push_back(std::vector<float>(sampleElms, 0.0f));
      allWeightCvds.push_back({allWeights.back().data(), sampleInfo});
      allWeightIds.push_back(
          builder->addInitializedInputTensor(allWeightCvds.back()));
      TensorId actOutId = "act" + std::to_string(nLayersAdded);
      allActIds.push_back(aiOnnx.add({allWeightIds.back(), actInId}, actOutId));
      weightsRead.insert(allWeightIds.back(),
                         {w_readbacks.back().data(), weightInfo});
      ++nLayersAdded;
    };

    // left branch (branch 0)
    insertAddLayer(input0);
    for (int i = 1; i < nLayers; ++i) {
      insertAddLayer(allActIds.back());
    }
    TensorId actFinal0 = allActIds.back();

    // right branch (branch 1)
    insertAddLayer(input1);
    for (int i = 1; i < nLayers; ++i) {
      insertAddLayer(allActIds.back());
    }
    TensorId actFinal1 = allActIds.back();

    // sum of the 2 branch outputs
    auto actFinal = aiOnnx.add({actFinal0, actFinal1}, "finalAct");

    builder->addOutputTensor(actFinal);
    auto proto = builder->getModelProto();
    // No anchors
    auto dataFlow = DataFlow(batchesPerStep, {});

    std::map<std::string, std::string> deviceOpts{{"numIPUs", "7"}};

    float learnRate = 0.01;
    auto optimizer  = ConstSGD(learnRate);

    float lambda = 0.1;
    auto loss    = std::unique_ptr<Loss>(
        new L1Loss(actFinal, "l1LossVal", lambda, ReductionType::SUM));

    auto device =
        DeviceManager::createDeviceManager().createIpuModelDevice(deviceOpts);

    SessionOptions userOptions;
    userOptions.enableVirtualGraphs = true;
    userOptions.autoVirtualGraph    = true;
    userOptions.enablePipelining    = true;

    if (tt == TestType::Ir) {

      std::vector<std::tuple<int64_t, int64_t>> pipeSrcDsts;
      // we compute on-the-fly what the expected copies are
      std::vector<std::tuple<int64_t, int64_t>> expectedSrcDsts;

      auto getIpuCopies = [](const Ir &ir) {
        std::vector<IpuCopyOp *> copies;
        for (const auto &x : ir.getMainGraphOps()) {
          auto ipuCopy = dynamic_cast<IpuCopyOp *>(x.second.get());
          if (ipuCopy) {
            copies.push_back(ipuCopy);
          }
        }
        return copies;
      };

      auto modelProto = io::getModelFromString(proto);

      Ir irWithPipe;
      irWithPipe.prepare({modelProto,
                          InputShapeInfo(),
                          dataFlow,
                          {loss.get()},
                          &optimizer,
                          *device,
                          userOptions,
                          Patterns(PatternsLevel::DEFAULT)});

      auto copiesWithPipe = getIpuCopies(irWithPipe);
      for (auto cop : copiesWithPipe) {
        pipeSrcDsts.push_back(
            std::make_tuple(cop->getSourceIpu(), cop->getDestIpu()));
      }

      userOptions.enablePipelining = false;
      Ir irWithoutPipe;
      irWithoutPipe.prepare({modelProto,
                             InputShapeInfo(),
                             dataFlow,
                             {loss.get()},
                             &optimizer,
                             *device,
                             userOptions,
                             Patterns(PatternsLevel::DEFAULT)});

      // we are testing both discontiguous copies in the forward and backward
      // passes. So  check that we actually have both types in the graph.
      bool fwdDisco = false;
      bool bwdDisco = false;

      auto copiesWithoutPipe = getIpuCopies(irWithoutPipe);
      for (auto cop : copiesWithoutPipe) {
        auto ipuDiff = static_cast<int64_t>(cop->getDestIpu()) -
                       static_cast<int64_t>(cop->getSourceIpu());
        if (ipuDiff < -1) {
          bwdDisco = true;
        }
        if (ipuDiff > +1) {
          fwdDisco = true;
        }

        auto delta = cop->getDestIpu() < cop->getSourceIpu() ? -1 : +1;
        for (auto src = cop->getSourceIpu(); src != cop->getDestIpu();
             src += delta) {
          expectedSrcDsts.push_back(std::make_tuple(src, src + delta));
        }
      }

      BOOST_CHECK(fwdDisco == true);
      BOOST_CHECK(bwdDisco == true);

      if (printStdOut) {
        std::cout << "With pipelining: " << std::endl;
        for (auto ipuCopy : copiesWithPipe) {
          std::cout << ipuCopy->getFromToStr() << std::endl;
        }
        std::cout << "----------------" << std::endl;
        std::cout << "Without pipelining: " << std::endl;
        for (auto ipuCopy : copiesWithoutPipe) {
          std::cout << ipuCopy->getFromToStr() << std::endl;
        }
      }

      std::sort(pipeSrcDsts.begin(), pipeSrcDsts.end());
      std::sort(expectedSrcDsts.begin(), expectedSrcDsts.end());

      BOOST_CHECK(pipeSrcDsts == expectedSrcDsts);
    }

    // numerical test: compare computed weights after several iterations,
    // compare to the expected weights (which are easy to compute as the model
    // is linear)
    else if (tt == TestType::Numerical) {

      auto session = popart::TrainingSession::createFromOnnxModel(
          proto,
          dataFlow,
          {loss.get()},
          optimizer,
          device,
          InputShapeInfo(),
          userOptions,
          popart::Patterns(PatternsLevel::DEFAULT));

      session->prepareDevice();

      // The samples (same for 0 and 1)
      std::vector<float> v_input_x(stepDataElms);

      // cumulative samples (accumulated over multiple steps).
      std::vector<float> v_sample_sum_x(weightInfo.nelms(), 0.0f);
      std::map<popart::TensorId, popart::IArray &> anchors = {};

      // write initial weights to host
      session->weightsFromHost();

      float sampleNumVal = 100.0f;
      for (int i = 0; i < 3; ++i) {
        std::cout << "Iteration (call to run(...)) # " << i << std::endl;

        // generate new samples
        for (int i = 0; i < samplesPerStep; ++i) {
          for (int j = 0; j < sampleElms; ++j) {
            auto stepIndex = i * sampleElms + j;
            v_input_x[stepIndex] =
                fdis(eng) > 0.5 ? -sampleNumVal : +sampleNumVal;
            v_sample_sum_x[j] += v_input_x[stepIndex];
          }
        }
        popart::NDArrayWrapper<float> input_x_wrapper(v_input_x.data(),
                                                      stepDataInfo);
        std::map<popart::TensorId, popart::IArray &> inputs = {
            {input1, input_x_wrapper}, {input0, input_x_wrapper}};
        popart::StepIO stepio(inputs, anchors);

        // process the samples
        session->run(stepio);
      }

      // read final weights back
      session->weightsToHost();
      session->readWeights(weightsRead);

      // get sum of absolute differences between computed and expected
      float sumAbsDiff = 0.0;
      for (auto &wv : w_readbacks) {
        for (int i = 0; i < wv.size(); ++i) {

          if (printStdOut) {
            std::cout << "Returned : " << wv[i]
                      << "   - learnRate * lambda * sum / sampleNumVal : "
                      << -v_sample_sum_x[i] * learnRate * lambda / sampleNumVal
                      << std::endl;
          }
          sumAbsDiff += std::abs(wv[i] + v_sample_sum_x[i] * learnRate *
                                             lambda / sampleNumVal);
        }
      }
      BOOST_CHECK(sumAbsDiff < 1e-5);
    }
  };

  test(TestType::Ir);
  test(TestType::Numerical);
}
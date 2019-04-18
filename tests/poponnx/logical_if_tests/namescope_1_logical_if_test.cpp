#define BOOST_TEST_MODULE Namescope1LogicalIf

#include <boost/test/unit_test.hpp>
#include <poponnx/builder.hpp>
#include <poponnx/inputshapeinfo.hpp>
#include <poponnx/tensorinfo.hpp>
#include <poponnx/tensornames.hpp>

bool invalidHigherInput(const poponnx::error &ex) {
  std::string what = ex.what();
  BOOST_CHECK(what.find("Failed to add") != std::string::npos);
  BOOST_CHECK(what.find("from higher scope") != std::string::npos);
  return true;
}

BOOST_AUTO_TEST_CASE(LogicalIf_namescope0) {

  using namespace poponnx;
  TensorInfo info{"FLOAT", std::vector<int64_t>{2, 2}};

  auto root       = Builder::create();
  auto in0        = root->addInputTensor(info);
  Builder &child0 = root->createSubgraphBuilder();
  Builder &child1 = child0.createSubgraphBuilder();
  Builder &child2 = child1.createSubgraphBuilder();
  Builder &child3 = child2.createSubgraphBuilder();

  // non-existant name
  BOOST_CHECK_EXCEPTION(
      child3.addInputTensorFromHigherScope("thisNameDoesNotExistInHigherScope"),
      poponnx::error,
      invalidHigherInput);

  // name exists in higher scope
  child3.addInputTensorFromHigherScope(in0);

  auto in3 = child3.addInputTensor(info, "userChosenDebugString");

  // can't add a name from a lower scope
  BOOST_CHECK_EXCEPTION(child1.addInputTensorFromHigherScope(in3),
                        poponnx::error,
                        invalidHigherInput);

  Builder &child4 = child3.createSubgraphBuilder();
  Builder &child5 = child4.createSubgraphBuilder();

  // names exist in higher scope
  child5.addInputTensorFromHigherScope(in0);
  child5.addInputTensorFromHigherScope(in3);
}

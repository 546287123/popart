#define BOOST_TEST_MODULE MatMulTest

#include <boost/test/unit_test.hpp>
#include <poponnx/error.hpp>
#include <poponnx/matmul.hpp>
#include <poponnx/optimizer.hpp>
#include <poponnx/tensor.hpp>

#include <poponnx/popx/matmulx.hpp>

#include <iostream>

using namespace willow;

// Test the simple [2x2] * [2x2] matrix
BOOST_AUTO_TEST_CASE(MatMul_Case1) {

  // Setup
  onnx::NodeProto node;
  node.set_op_type("MatMul");

  willow::Ir ir;

  willow::MatMulOp mm(node, &ir);

  willow::Tensor lhs("lhs", willow::TensorType::ActGrad, &ir);
  lhs.info.set(willow::TP::FLOAT, {2, 2});
  mm.input.insert(0, &lhs);

  willow::Tensor rhs("rhs", willow::TensorType::ActGrad, &ir);
  rhs.info.set(willow::TP::FLOAT, {2, 2});
  mm.input.insert(1, &rhs);

  willow::Tensor out("out", willow::TensorType::ActGrad, &ir);
  mm.output.insert(0, &out);

  // Test the setup is correct
  mm.setup();
  BOOST_CHECK(mm.output.tensor(0)->info.dim(0) == 2);
  BOOST_CHECK(mm.output.tensor(0)->info.dim(1) == 2);
  BOOST_CHECK(mm.output.tensor(0)->info.dataType() == willow::TP::FLOAT);
  BOOST_CHECK(mm.rhsIn() == &rhs);
  BOOST_CHECK(mm.lhsIn() == &lhs);

  // Test the clone operation
  std::unique_ptr<willow::Op> mmClone = mm.clone();
  willow::MatMulOp *mmPtr = dynamic_cast<willow::MatMulOp *>(mmClone.get());
  BOOST_CHECK(mmPtr != nullptr);
  // Clone aka copy does not copy input & output???
  // BOOST_CHECK(mmPtr->output.tensor(0)->info.dim(0) == 2);
  // BOOST_CHECK(mmPtr->output.tensor(0)->info.dim(1) == 2);
  // BOOST_CHECK(mmPtr->output.tensor(0)->info.dataType() == willow::TP::FLOAT);
  // BOOST_CHECK(mmPtr->rhsIn() == &rhs);
  // BOOST_CHECK(mmPtr->lhsIn() == &lhs);

  auto gradOps = mm.getGradOps();
  BOOST_CHECK(gradOps.size() == 2);

  for (auto &&gradOp : gradOps) {
    willow::Op *op = gradOp.get();
    if (typeid(*op) == typeid(willow::MatMulLhsGradOp)) {
      willow::MatMulLhsGradOp *lhsGradOp =
          dynamic_cast<willow::MatMulLhsGradOp *>(op);

      willow::Tensor lhsOut("out", willow::TensorType::ActGrad, &ir);
      lhsGradOp->output.insert(0, &lhsOut);

      BOOST_CHECK(lhsGradOp->getGradInputIndex() == 0);
      BOOST_CHECK(lhsGradOp->getRhsInputIndex() == 1);

      lhsGradOp->setup();
      BOOST_CHECK(lhsGradOp->output.tensor(0)->info.dim(0) == 2);
      BOOST_CHECK(lhsGradOp->output.tensor(0)->info.dim(1) == 2);
      BOOST_CHECK(lhsGradOp->output.tensor(0)->info.dataType() ==
                  willow::TP::FLOAT);

      auto gradInfo = lhsGradOp->gradInputInfo();
      std::vector<GradInOutMapper> expectedGradInfo = {
          {0, 0, willow::GradOpInType::GRADOUT},
          {1, 1, willow::GradOpInType::IN}};
      BOOST_CHECK(gradInfo == expectedGradInfo);

      auto mapping                       = lhsGradOp->gradOutToNonGradIn();
      std::map<int, int> expectedMapping = {{0, 0}};
      BOOST_CHECK(mapping == expectedMapping);
    } else if (typeid(*op) == typeid(willow::MatMulRhsGradOp)) {
      willow::MatMulRhsGradOp *rhsGradOp =
          dynamic_cast<willow::MatMulRhsGradOp *>(op);

      willow::Tensor rhsOut("out", willow::TensorType::ActGrad, &ir);
      rhsGradOp->output.insert(0, &rhsOut);

      BOOST_CHECK(rhsGradOp->getGradInputIndex() == 0);
      BOOST_CHECK(rhsGradOp->getLhsInputIndex() == 1);

      rhsGradOp->setup();
      BOOST_CHECK(rhsGradOp->output.tensor(0)->info.dim(0) == 2);
      BOOST_CHECK(rhsGradOp->output.tensor(0)->info.dim(1) == 2);
      BOOST_CHECK(rhsGradOp->output.tensor(0)->info.dataType() ==
                  willow::TP::FLOAT);

      auto gradInfo = rhsGradOp->gradInputInfo();
      std::vector<GradInOutMapper> expectedGradInfo = {
          {0, 0, willow::GradOpInType::GRADOUT},
          {1, 0, willow::GradOpInType::IN}};

      BOOST_CHECK(gradInfo == expectedGradInfo);

      auto mapping                       = rhsGradOp->gradOutToNonGradIn();
      std::map<int, int> expectedMapping = {{0, 1}};
      BOOST_CHECK(mapping == expectedMapping);

    } else {
      // Unexpected grad op
      BOOST_CHECK(false);
    }
  }
}

// Test the simple [3x2] * [2x6] matrix
BOOST_AUTO_TEST_CASE(MatMul_Case2) {

  // Setup
  onnx::NodeProto node;
  node.set_op_type("MatMul");

  willow::Ir ir;

  willow::MatMulOp mm(node, &ir);

  willow::Tensor lhs("lhs", willow::TensorType::ActGrad, &ir);
  lhs.info.set(willow::TP::FLOAT, {3, 2});
  mm.input.insert(0, &lhs);

  willow::Tensor rhs("rhs", willow::TensorType::ActGrad, &ir);
  rhs.info.set(willow::TP::FLOAT, {2, 6});
  mm.input.insert(1, &rhs);

  willow::Tensor out("out", willow::TensorType::ActGrad, &ir);
  mm.output.insert(0, &out);

  // Test the setup is correct
  mm.setup();
  BOOST_CHECK(mm.output.tensor(0)->info.dim(0) == 3);
  BOOST_CHECK(mm.output.tensor(0)->info.dim(1) == 6);
  BOOST_CHECK(mm.output.tensor(0)->info.dataType() == willow::TP::FLOAT);
  BOOST_CHECK(mm.rhsIn() == &rhs);
  BOOST_CHECK(mm.lhsIn() == &lhs);

  // Test the clone operation
  std::unique_ptr<willow::Op> mmClone = mm.clone();
  willow::MatMulOp *mmPtr = dynamic_cast<willow::MatMulOp *>(mmClone.get());
  BOOST_CHECK(mmPtr != nullptr);
  // Clone aka copy does not copy input & output???
  // BOOST_CHECK(mmPtr->output.tensor(0)->info.dim(0) == 2);
  // BOOST_CHECK(mmPtr->output.tensor(0)->info.dim(1) == 2);
  // BOOST_CHECK(mmPtr->output.tensor(0)->info.dataType() == willow::TP::FLOAT);
  // BOOST_CHECK(mmPtr->rhsIn() == &rhs);
  // BOOST_CHECK(mmPtr->lhsIn() == &lhs);

  auto gradOps = mm.getGradOps();
  BOOST_CHECK(gradOps.size() == 2);

  for (auto &&gradOp : gradOps) {
    willow::Op *op = gradOp.get();
    if (typeid(*op) == typeid(willow::MatMulLhsGradOp)) {
      willow::MatMulLhsGradOp *lhsGradOp =
          dynamic_cast<willow::MatMulLhsGradOp *>(op);

      willow::Tensor lhsOut("out", willow::TensorType::ActGrad, &ir);
      lhsGradOp->output.insert(0, &lhsOut);

      BOOST_CHECK(lhsGradOp->getGradInputIndex() == 0);
      BOOST_CHECK(lhsGradOp->getRhsInputIndex() == 1);

      lhsGradOp->setup();
      BOOST_CHECK(lhsGradOp->output.tensor(0)->info.dim(0) == 3);
      BOOST_CHECK(lhsGradOp->output.tensor(0)->info.dim(1) == 2);
      BOOST_CHECK(lhsGradOp->output.tensor(0)->info.dataType() ==
                  willow::TP::FLOAT);

      auto gradInfo = lhsGradOp->gradInputInfo();
      std::vector<GradInOutMapper> expectedGradInfo = {
          {0, 0, willow::GradOpInType::GRADOUT},
          {1, 1, willow::GradOpInType::IN}};
      BOOST_CHECK(gradInfo == expectedGradInfo);

      auto mapping                       = lhsGradOp->gradOutToNonGradIn();
      std::map<int, int> expectedMapping = {{0, 0}};
      BOOST_CHECK(mapping == expectedMapping);
    } else if (typeid(*op) == typeid(willow::MatMulRhsGradOp)) {
      willow::MatMulRhsGradOp *rhsGradOp =
          dynamic_cast<willow::MatMulRhsGradOp *>(op);

      willow::Tensor rhsOut("out", willow::TensorType::ActGrad, &ir);
      rhsGradOp->output.insert(0, &rhsOut);

      BOOST_CHECK(rhsGradOp->getGradInputIndex() == 0);
      BOOST_CHECK(rhsGradOp->getLhsInputIndex() == 1);

      rhsGradOp->setup();
      BOOST_CHECK(rhsGradOp->output.tensor(0)->info.dim(0) == 2);
      BOOST_CHECK(rhsGradOp->output.tensor(0)->info.dim(1) == 6);
      BOOST_CHECK(rhsGradOp->output.tensor(0)->info.dataType() ==
                  willow::TP::FLOAT);

      auto gradInfo = rhsGradOp->gradInputInfo();
      std::vector<GradInOutMapper> expectedGradInfo = {
          {0, 0, willow::GradOpInType::GRADOUT},
          {1, 0, willow::GradOpInType::IN}};

      BOOST_CHECK(gradInfo == expectedGradInfo);

      auto mapping                       = rhsGradOp->gradOutToNonGradIn();
      std::map<int, int> expectedMapping = {{0, 1}};
      BOOST_CHECK(mapping == expectedMapping);

    } else {
      // Unexpected grad op
      BOOST_CHECK(false);
    }
  }
}

// Test invalid rank on lhs
BOOST_AUTO_TEST_CASE(MatMul_ErrorCase1) {

  // Setup
  onnx::NodeProto node;
  node.set_op_type("MatMul");

  willow::Ir ir;

  willow::MatMulOp mm(node, &ir);

  willow::Tensor lhs("lhs", willow::TensorType::ActGrad, &ir);
  lhs.info.set(willow::TP::FLOAT, {2, 2, 3});
  mm.input.insert(0, &lhs);

  willow::Tensor rhs("rhs", willow::TensorType::ActGrad, &ir);
  rhs.info.set(willow::TP::FLOAT, {2, 2});
  mm.input.insert(1, &rhs);

  willow::Tensor out("out", willow::TensorType::ActGrad, &ir);
  mm.output.insert(0, &out);

  // Test the setup is correct
  BOOST_CHECK_THROW(mm.setup(), error);
}
// Test invalid rank on rhs
BOOST_AUTO_TEST_CASE(MatMul_ErrorCase2) {

  // Setup
  onnx::NodeProto node;
  node.set_op_type("MatMul");

  willow::Ir ir;

  willow::MatMulOp mm(node, &ir);

  willow::Tensor lhs("lhs", willow::TensorType::ActGrad, &ir);
  lhs.info.set(willow::TP::FLOAT, {2, 2});
  mm.input.insert(0, &lhs);

  willow::Tensor rhs("rhs", willow::TensorType::ActGrad, &ir);
  rhs.info.set(willow::TP::FLOAT, {2});
  mm.input.insert(1, &rhs);

  // Test the setup is correct
  BOOST_CHECK_THROW(mm.setup(), error);
}
// Test invalid matrix multiplication
BOOST_AUTO_TEST_CASE(MatMul_ErrorCase3) {

  // Setup
  onnx::NodeProto node;
  node.set_op_type("MatMul");

  willow::Ir ir;

  willow::MatMulOp mm(node, &ir);

  willow::Tensor lhs("lhs", willow::TensorType::ActGrad, &ir);
  lhs.info.set(willow::TP::FLOAT, {2, 3});
  mm.input.insert(0, &lhs);

  willow::Tensor rhs("rhs", willow::TensorType::ActGrad, &ir);
  rhs.info.set(willow::TP::FLOAT, {10, 2});
  mm.input.insert(1, &rhs);

  willow::Tensor out("out", willow::TensorType::ActGrad, &ir);
  mm.output.insert(0, &out);

  // Test the setup is correct
  BOOST_CHECK_THROW(mm.setup(), error);
}

/*
BOOST_AUTO_TEST_CASE(MatMulX_Case1){

  // Setup
  onnx::NodeProto node;
  node.set_op_type("MatMul");

  willow::Ir ir;

  willow::MatMulOp mm(node, &ir);

  willow::Tensor lhs("lhs", willow::TensorType::ActGrad, &ir);
  lhs.info.set(willow::TP::FLOAT, {3, 2});
  mm.input.insert(0, &lhs);

  willow::Tensor rhs("rhs", willow::TensorType::ActGrad, &ir);
  rhs.info.set(willow::TP::FLOAT, {2, 6});
  mm.input.insert(1, &rhs);

  willow::Tensor out("out", willow::TensorType::ActGrad, &ir);
  mm.output.insert(0, &out);


  popx::MatMulOpx op(&mm, 0);

  op.grow();
}
*/
#include <neuralnet/error.hpp>
#include <neuralnet/tensor.hpp>
#include <neuralnet/varupdate.hpp>

namespace neuralnet {
VarUpdateOp::VarUpdateOp(TensorId varId_, Graph *pgraph)
    : Op({"VarUpdate", pgraph, {}, getNeuralNetDomain()}), varId(varId_),
      varGradId(getGradId(varId)) {}

void VarUpdateOp::setup() {
  // throw error("is there anything to do in var update op setup?");
}

int VarUpdateOp::getVarIndex() { return 0; }

int VarUpdateOp::getVarGradIndex() { return 1; }

int VarUpdateOp::getLearnRateIndex() { return 2; }

void VarUpdateOp::imposeTopoCons() {
  input.tensor(getVarIndex())->consumers.setTopoLast(this);
}

} // namespace neuralnet
// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <memory>
#include <popart/error.hpp>
#include <popart/op/logsoftmax.hpp>
#include <popart/opmanager.hpp>
#include <popart/tensor.hpp>

namespace popart {

LogSoftmaxOp::LogSoftmaxOp(const OperatorIdentifier &_opid,
                           int64_t axis_,
                           const Op::Settings &settings_)
    : ElementWiseUnaryOp(_opid, settings_), axis(axis_) {}

std::unique_ptr<Op> LogSoftmaxOp::clone() const {
  return std::make_unique<LogSoftmaxOp>(*this);
}

std::vector<std::unique_ptr<Op>> LogSoftmaxOp::getGradOps() {
  throw error("LogSoftmaxOp should be removed by pattern 'LogSoftmaxOp' before "
              "call to getGradOps");
}

int64_t LogSoftmaxOp::getAxis() const {
  auto r = static_cast<int64_t>(inShape(getInIndex()).size());
  if (axis < -r || axis > r - 1) {
    throw error("LogSoftmax axis, {}, is outside of acceptable range [{}, {}]",
                axis,
                -r,
                r - 1);
  }

  if (axis < 0) {
    return r + axis;
  } else {
    return axis;
  }
}

namespace {

static OpDefinition::DataTypes T = {DataType::FLOAT16, DataType::FLOAT};

static OpDefinition logSoftmaxOpDef({OpDefinition::Inputs({{"input", T}}),
                                     OpDefinition::Outputs({{"output", T}}),
                                     OpDefinition::Attributes({
                                         {"axis", {"*"}},
                                     })});

static OpCreator<LogSoftmaxOp> logSoftmaxOpCreator(
    OpDefinitions({{Onnx::Operators::LogSoftmax_1, logSoftmaxOpDef},
                   {Onnx::Operators::LogSoftmax_11, logSoftmaxOpDef}}),
    [](const OpCreatorInfo &info) {
      int64_t axis = info.attributes.getAttribute<Attributes::Int>("axis", 1);

      return std::make_unique<LogSoftmaxOp>(info.opid, axis, info.settings);
    },
    true);

} // namespace

} // namespace popart

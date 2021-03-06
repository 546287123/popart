// Copyright (c) 2018 Graphcore Ltd. All rights reserved.
#include <algorithm>
#include <memory>
#include <vector>
#include <popart/error.hpp>
#include <popart/ir.hpp>
#include <popart/op/conv.hpp>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/tensor.hpp>

namespace popart {

// These are utility functions that are need by the Ir Conv.
namespace popx {
ConvParameters getConvGradParameters(const ConvParameters &fwdParams);
ConvParameters canonicalizeConvParams(const ConvParameters &param);
} // namespace popx

ConvOp::ConvOp(const OperatorIdentifier &_opid,
               const Settings &settings_,
               std::vector<int64_t> strides,
               std::vector<int64_t> pads,
               std::vector<int64_t> dilations,
               int64_t group_,
               const AutoPad &padType,
               const MultiConvOptions &convOpts)
    : MultiConvBaseOp(_opid,
                      settings_,
                      strides,
                      pads,
                      dilations,
                      padType,
                      convOpts),
      group(group_) {}

std::vector<std::unique_ptr<Op>> ConvOp::getGradOps() {
  std::vector<std::unique_ptr<Op>> upops;
  upops.emplace_back(std::make_unique<ConvDataGradOp>(*this));
  upops.emplace_back(std::make_unique<ConvWeightsGradOp>(*this));
  return upops;
}

std::unique_ptr<Op> ConvOp::clone() const {
  return std::make_unique<ConvOp>(*this);
}

void ConvOp::setup() {
  // The non-optional 'group' argument can always be determined based
  // on input shapes. Check that they match
  if (group == 0) {
    throw error("group attribute in {} must be greater than zero", debugName());
  }

  if (group != getGroups()) {
    throw error(
        "Invalid value for group ({}) in {}. number of input channels ({}) / "
        "group ({}) should be equal to the weight inputs second dimension ({})",
        group,
        debugName(),
        getNInChans(),
        group,
        inInfo(getWeightsInIndex()).dim(1));
  }

  MultiConvBaseOp::setup();
}

ConvWeightsGradOp::ConvWeightsGradOp(const ConvOp &op_)
    : MultiConvWeightsGradBaseOp(op_, Onnx::GradOperators::ConvWeightsGrad) {}

std::unique_ptr<Op> ConvWeightsGradOp::clone() const {
  return std::make_unique<ConvWeightsGradOp>(*this);
}

ConvDataGradOp::ConvDataGradOp(const ConvOp &op_)
    : MultiConvDataGradBaseOp(op_, Onnx::GradOperators::ConvDataGrad) {}

std::unique_ptr<Op> ConvDataGradOp::clone() const {
  return std::make_unique<ConvDataGradOp>(*this);
}

ConvFlipWeightsOp::ConvFlipWeightsOp(const OperatorIdentifier &opid_,
                                     const Op::Settings &settings_)
    : Op(opid_, settings_), convOpts({}, {}) {}

std::unique_ptr<Op> ConvFlipWeightsOp::clone() const {
  return std::make_unique<ConvFlipWeightsOp>(*this);
}

ConvFlipWeightsOp::~ConvFlipWeightsOp() {}

void ConvFlipWeightsOp::setup() {

  auto &weightsIn = inInfo(getInIndex());

  // Switch the first two dimensions
  Shape weightsOutShape;
  weightsOutShape.push_back(weightsIn.dim(1));
  weightsOutShape.push_back(weightsIn.dim(0));
  for (int i = 2; i < weightsIn.shape().size(); ++i) {
    weightsOutShape.push_back(weightsIn.dim(i));
  }

  outInfo(getOutIndex()) = {weightsIn.dataType(), weightsOutShape};
}

void ConvFlipWeightsOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);

  // Append conv options
  for (auto key_val : getConvOptions()) {
    os.appendAttribute(key_val.first, key_val.second);
  }
}

namespace {

static OpDefinition convOpDef(
    {OpDefinition::Inputs({
         {"X", {{DataType::FLOAT, DataType::FLOAT16}}},
         {"W", {{DataType::FLOAT, DataType::FLOAT16}}},
         {"B", {{DataType::FLOAT, DataType::FLOAT16}}},
     }),
     OpDefinition::Outputs({{"Y", {{DataType::FLOAT, DataType::FLOAT16}}}}),
     OpDefinition::Attributes({
         {"auto_pad", {"NOTSET"}},
         // deprecated from conv
         {"dilations", {"*"}},
         {"group", {"*"}},
         {"kernel_shape", {"*"}}, // Do we support this?
         {"pads", {"*"}},
         {"strides", {"*"}},
     })});

static OpCreator<ConvOp> convOpCreator(
    OpDefinitions({
        {Onnx::Operators::Conv_1, convOpDef},
        {Onnx::Operators::Conv_11, convOpDef},
    }),
    [](const OpCreatorInfo &info) -> std::unique_ptr<Op> {
      auto strides =
          info.attributes.getAttribute<Attributes::Ints>("strides", {});
      auto pads = info.attributes.getAttribute<Attributes::Ints>("pads", {});
      auto dilations =
          info.attributes.getAttribute<Attributes::Ints>("dilations", {});
      auto group   = info.attributes.getAttribute<Attributes::Int>("group", 1);
      auto padType = info.attributes.getAttribute<Attributes::String>(
          "auto_pad", "NOTSET");

      auto sessOpts =
          info.settings.getIr().getSessionOptions().convolutionOptions;
      auto convOpts = MultiConvOptions(sessOpts, info.attributes);

      return std::unique_ptr<Op>(
          new ConvOp(info.opid,
                     info.settings,
                     strides,
                     pads,
                     dilations,
                     group,
                     HasReceptiveFieldOp::getAutoPad(padType),
                     convOpts));
    },
    true);

static OpDefinition::DataTypes T = {DataType::FLOAT16, DataType::FLOAT};

static OpDefinition
    convFlipWeightsOpDef({OpDefinition::Inputs({{"input", T}}),
                          OpDefinition::Outputs({{"output", T}}),
                          OpDefinition::Attributes({})});

static OpCreator<ConvFlipWeightsOp> convFlipWeightsOpCreator(OpDefinitions({
    {Onnx::CustomOperators::ConvFlipWeights, convFlipWeightsOpDef},
}));
} // namespace

} // namespace popart

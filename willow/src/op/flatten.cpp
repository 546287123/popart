// Copyright (c) 2019 Graphcore Ltd. All rights reserved.
#include <algorithm>
#include <memory>
#include <numeric>
#include <popart/op/flatten.hpp>
#include <popart/opmanager.hpp>
#include <popart/opserialiser.hpp>
#include <popart/tensor.hpp>

namespace popart {

std::unique_ptr<Op>
FlattenOp::getInplaceVariant(const OperatorIdentifier &operator_id) const {
  if (operator_id == Onnx::CustomOperators::FlattenInplace) {
    return std::make_unique<FlattenInplaceOp>(*this);
  }
  // catch remaining cases and throw an error
  return Op::getInplaceVariant(operator_id);
}

view::RegMap FlattenBaseOp::fwdRegMap(InIndex inIndex,
                                      OutIndex outIndex) const {
  if (inIndex != 0 || outIndex != 0) {
    throw internal_error("[FlattenBaseOp::fwdRegMap] "
                         "Received input index {} but only 0 allowed, "
                         "This for Op {}, ",
                         inIndex,
                         str());
  }
  auto inRegion    = view::Region::getFull(inInfo(getInIndex()).shape());
  auto outRegion   = view::Region::getFull(outInfo(getOutIndex()).shape());
  auto emptyRegion = view::Region::getEmpty(outRank(getOutIndex()));
  return [emptyRegion, inRegion, outRegion](const view::Region &r) {
    if (r.isEmpty()) {
      return view::Regions(1, emptyRegion);
    }
    return r.reshape(inRegion, outRegion);
  };
}

view::RegMap FlattenBaseOp::bwdRegMap(InIndex inIndex,
                                      OutIndex outIndex) const {
  if (inIndex != 0 || outIndex != 0) {
    throw internal_error("[FlattenBaseOp::bwdRegMap] "
                         "Received input index {} but only 0 allowed, "
                         "This for Op {}, ",
                         inIndex,
                         str());
  }
  auto inRegion    = view::Region::getFull(inInfo(getInIndex()).shape());
  auto outRegion   = view::Region::getFull(outInfo(getOutIndex()).shape());
  auto emptyRegion = view::Region::getEmpty(inRank(getInIndex()));
  return [emptyRegion, inRegion, outRegion](const view::Region &r) {
    if (r.isEmpty()) {
      return view::Regions(1, emptyRegion);
    }
    return r.reshape(outRegion, inRegion);
  };
}

FlattenBaseOp::FlattenBaseOp(const OperatorIdentifier &_opid,
                             int64_t axis_,
                             const Op::Settings &settings_)
    : Op(_opid, settings_), axis(axis_) {}

FlattenInplaceOp::FlattenInplaceOp(const FlattenOp &op)
    : FlattenBaseOp(Onnx::CustomOperators::FlattenInplace,
                    op.getAxis(),
                    op.settings) {}

std::unique_ptr<Op> FlattenInplaceOp::clone() const {
  return std::make_unique<FlattenInplaceOp>(*this);
}

std::unique_ptr<Op> FlattenOp::clone() const {
  return std::make_unique<FlattenOp>(*this);
}

FlattenInplaceOp::FlattenInplaceOp(const OperatorIdentifier &_opid,
                                   int64_t axis_,
                                   const Op::Settings &settings_)
    : FlattenBaseOp(_opid, axis_, settings_) {}

FlattenOp::FlattenOp(const OperatorIdentifier &_opid,
                     int64_t axis_,
                     const Op::Settings &settings_)
    : FlattenBaseOp(_opid, axis_, settings_) {}

void FlattenBaseOp::setup() {
  const auto in_shape = inInfo(getInIndex()).shape();

  // Flatten can support negative indexing as per onnx spec.
  const int64_t axisAdjusted = axis >= 0 ? axis : in_shape.size() + axis;

  const auto begin = in_shape.begin();
  const auto mid   = in_shape.begin() + axisAdjusted;
  const auto end   = in_shape.end();

  // The product of the first axis dimensions to flatten
  const auto m = std::accumulate(begin, mid, 1, std::multiplies<int64_t>());

  // The product of the remaining dimensions
  const auto n = std::accumulate(mid, end, 1, std::multiplies<int64_t>());

  // The "flattened" shape
  const Shape out_shape = {m, n};

  outInfo(getOutIndex()) = {inInfo(getInIndex()).data_type(), out_shape};
}

std::vector<std::unique_ptr<Op>> FlattenBaseOp::getGradOps() {
  throw error("No gradient operation for flatten is available. Flatten should "
              "have been automatically replaced by a reshape operation by the "
              "built-in OpToReshape pattern");
}

int64_t FlattenBaseOp::getAxis() const { return axis; }

void FlattenBaseOp::setAxis(int64_t value) { axis = value; }

void FlattenBaseOp::appendOutlineAttributes(OpSerialiserBase &os) const {
  Op::appendOutlineAttributes(os);
  os.appendAttribute("axis", axis);
}

view::Regions FlattenInplaceOp::aliases(InIndex in, OutIndex) const {
  return uses(in);
}

namespace {

static OpDefinition::DataTypes T = {DataType::UINT8,
                                    DataType::UINT16,
                                    DataType::UINT32,
                                    DataType::UINT64,
                                    DataType::INT8,
                                    DataType::INT16,
                                    DataType::INT32,
                                    DataType::INT64,
                                    DataType::FLOAT16,
                                    DataType::FLOAT,
                                    DataType::BOOL};

static OpDefinition
    flatternOpDef({OpDefinition::Inputs({{"input", T}}),
                   OpDefinition::Outputs({{"output", T}}),
                   OpDefinition::Attributes({{"axis", {"*"}}})});

static std::unique_ptr<Op> flattenOpFactory(const OpCreatorInfo &info) {
  int64_t axis = info.attributes.getAttribute<Attributes::Int>("axis", 1);
  return std::make_unique<FlattenOp>(info.opid, axis, info.settings);
}

static OpCreator<FlattenOp>
    flattenOpCreator({{Onnx::Operators::Flatten_1, flatternOpDef},
                      {Onnx::Operators::Flatten_9, flatternOpDef},
                      {Onnx::Operators::Flatten_11, flatternOpDef}},
                     flattenOpFactory,
                     true);

} // namespace

} // namespace popart

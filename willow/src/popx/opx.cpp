#include <poponnx/error.hpp>
#include <poponnx/ir.hpp>
#include <poponnx/op/conv.hpp>
#include <poponnx/popx/devicex.hpp>
#include <poponnx/popx/opx.hpp>
#include <poponnx/popx/opxmanager.hpp>
#include <poponnx/tensor.hpp>
#include <poponnx/tensorindex.hpp>

namespace poponnx {
namespace popx {

Opx::Opx(Op *op_p_, Devicex *dv_p_) : op_p(op_p_), dv_p(dv_p_) {}

Opx::~Opx() = default;

poplar::Tensor Opx::createInput(InIndex index, const std::string &name) const {
  throw error("Opx for {} cannot create Input index:{} name:{}",
              op_p->opid,
              index,
              name);
}

bool Opx::createsEquiv(int, const Opx *, int) const {
  throw error("No check for equivalent tensor create for type {}", op_p->opid);
}

std::vector<TensorId> Opx::mustExistBeforeCreate(int index0) const {
  throw error("Opx for {} cannot say which poplar Tensors must exist to create "
              "at index {}",
              op_p->opid,
              index0);
}

void Opx::grow(poplar::program::Sequence &) const {
  throw error("adding poplar::Tensors not implemented for {}", op_p->opid);
}

InputCreatorType Opx::getInputCreatorType(int) const {
  return InputCreatorType::DEADEND;
}

poplar::Tensor
Opx::unwindTensorLayout(poplar::Tensor, InIndex, OutIndex) const {
  throw error("Opx for {} cannot unwind the tensor layout change between input "
              "and output",
              op_p->opid);
}

int64_t Opx::getVirtualGraphId() const {
  if (op_p->getVirtualGraphId())
    return *(op_p->getVirtualGraphId());
  else {
    if (op_p->getIr().getSessionOptions().enableVirtualGraphs) {
      throw error("{} does not have a virtual graph attribute",
                  op_p->debugName());
    } else {
      return 0;
    }
  }
}

poplar::Graph &Opx::masterGraph() const { return dv_p->masterGraph(); }

poplar::Graph &Opx::graph() const {
  if (op_p->getIr().getSessionOptions().enableVirtualGraphs) {
    return dv_p->graph(getVirtualGraphId());
  } else {
    return dv_p->masterGraph();
  }
}

const poplar::Tensor &Opx::get(TensorId id) const {
  return dv_p->tensors.get(id);
}

void Opx::insert(TensorId id, const poplar::Tensor &tensor) const {
  dv_p->tensors.insert(id, tensor);
}

TensorId Opx::inId(InIndex index) const { return op_p->input->id(index); }
TensorId Opx::outId(OutIndex index) const { return op_p->output->id(index); }

const poplar::Tensor &Opx::getInTensor(InIndex index) const {
  if (!cachedInputs.empty()) {
    return cachedInputs[index];
  } else {
    return get(op_p->input->id(index));
  }
}

void Opx::setOutTensor(OutIndex index, const poplar::Tensor &tensor) const {
  // Assume that if we have cached inputs then we will use cached outputs
  if (cachedOutputs) {
    cachedOutputs->insert(cachedOutputs->begin() + index, tensor);
  } else {
    insert(op_p->output->id(index), tensor);
  }
}

Tensor *Opx::inTensor(InIndex index) const {
  return op_p->input->tensor(index);
}
Tensor *Opx::outTensor(OutIndex index) const {
  return op_p->output->tensor(index);
}

const TensorInfo &Opx::inInfo(InIndex index) const {
  return inTensor(index)->info;
}

const Shape &Opx::inShape(InIndex index) const { return inInfo(index).shape(); }

const TensorInfo &Opx::outInfo(OutIndex index) const {
  return outTensor(index)->info;
}

const Shape &Opx::outShape(OutIndex index) const {
  return outInfo(index).shape();
}

// If the operator has been named return the name, (i.e. "my_add/23")
// else return the id (i.e "23")
std::string Opx::idStr() const {
  if (!op_p->name().empty()) {
    return op_p->name() + sNameDelimiter + std::to_string(op_p->id);
  } else {
    return std::to_string(op_p->id);
  }
}

std::string Opx::debugPrefix(const std::string &prefix) const {
  return idStr() + sNameDelimiter + prefix;
}

poplar::Tensor Opx::cloneNcopy(poplar::program::Sequence &prog,
                               TensorId id) const {
  auto outTensor = graph().clone(get(id));
  poplar::program::Copy copyProg(get(id), outTensor);
  prog.add(copyProg);
  return outTensor;
}

poplar::Tensor Opx::cloneNcopy(poplar::program::Sequence &prog,
                               const poplar::Tensor &tensor) const {
  auto outTensor = graph().clone(tensor);
  prog.add(poplar::program::Copy(tensor, outTensor));
  return outTensor;
}

poplar::Tensor Opx::broadcast(const std::vector<int64_t> &desired_shape,
                              TensorId id) const {
  return broadcast(desired_shape, get(id));
}

poplar::Tensor Opx::broadcast(const std::vector<int64_t> &desired_shape,
                              poplar::Tensor t) const {
  const auto &t_shape = t.shape();

  // `new_shape` is `t_shape` prepended with ones to have matching rank as
  // `desired_shape`
  std::vector<std::size_t> new_shape(desired_shape.size(), 1);
  std::copy(t_shape.rbegin(), t_shape.rend(), new_shape.rbegin());

  // `t` now has matching rank as `desired_shape`
  t = t.reshape(new_shape);

  // Iteratively broadcast each mismatched dimension of `t`. This will
  // result in the shape of `t` matching `desired_shape`.
  for (int dim = 0; dim < desired_shape.size(); ++dim) {
    if (new_shape[dim] != desired_shape[dim] && new_shape[dim] != 1) {
      // Incompatible dimension found. Throw an exception, borrowing the same
      // terminology as numpy.
      throw error("np broadcasting failed, frames are not aligned");
    }

    if (new_shape[dim] != desired_shape[dim]) {
      t = t.broadcast(static_cast<unsigned>(desired_shape[dim]), dim);
    }
  }

  return t;
}

poplar::Tensor Opx::getConst(const poplar::Type &type,
                             const std::vector<size_t> &shape,
                             double val,
                             const std::string &name) const {
  return dv_p->getConst(graph(), type, shape, val, name);
}

// TODO : Find a better place to put these, ops that will be optimized out
// before creating opx's
namespace {
OpxCreator<Opx>
    gemmOpxCreator_6(Onnx::Operators::Gemm_6,
                     "GemmOp should be removed by pattern 'GemmOp'");
OpxCreator<Opx>
    gemmOpxCreator_7(Onnx::Operators::Gemm_7,
                     "GemmOp should be removed by pattern 'GemmOp'");
OpxCreator<Opx>
    gemmOpxCreator_9(Onnx::Operators::Gemm_9,
                     "GemmOp should be removed by pattern 'GemmOp'");

OpxCreator<Opx> tanGradOpxCreator(Onnx::Operators::Tan_7,
                                  "TanOp should be removed by pattern 'TanOp'");
} // namespace

} // namespace popx
} // namespace poponnx

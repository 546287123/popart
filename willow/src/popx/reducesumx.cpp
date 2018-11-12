#include <algorithm>
#include <iterator>
#include <vector>

#include <poponnx/error.hpp>
#include <poponnx/popx/reducesumx.hpp>
#include <poponnx/reducesum.hpp>
#include <poponnx/tensor.hpp>

#pragma clang diagnostic push // start ignoring warnings
#pragma clang diagnostic ignored "-Weverything"
#include <popops/Reduce.hpp>
#pragma clang diagnostic pop // stop ignoring warnings

namespace willow {
namespace popx {

ReduceSumOpx::ReduceSumOpx(Op *op, Devicex *devicex) : Opx(op, devicex) {
  if (op->opType != OpType::REDUCESUM) {
    throw error("cannot create ReduceSumOpx from " + op->op_type());
  }
}

template <typename IIter>
static bool contains(IIter begin,
                     IIter end,
                     typename std::iterator_traits<IIter>::value_type elem) {
  return std::find(begin, end, elem) != end;
}

template <typename T1, typename T2>
static std::vector<T1> vector_cast(const std::vector<T2> &xs) {
  std::vector<T1> ys;

  ys.reserve(xs.size());
  for (const auto &x : xs) {
    ys.emplace_back(static_cast<T1>(x));
  }

  return ys;
}

void ReduceSumOpx::grow() const {
  const auto op    = dynamic_cast<ReduceSumOp *>(op_p);
  const auto input = get(inId(0));

  auto output_tensor = popops::reduce(graph(),
                                      input,
                                      vector_cast<std::size_t>(op->getAxes()),
                                      {popops::Operation::ADD},
                                      step());

  insert(outId(0), output_tensor.reshape(outInfo(0).shape_szt()));
}

ReduceSumGradOpx::ReduceSumGradOpx(Op *op, Devicex *devicex)
    : Opx(op, devicex) {
  if (op->opType != OpType::REDUCESUMGRAD) {
    throw error("cannot create ReduceSumOpx from " + op->op_type());
  }
}

void ReduceSumGradOpx::grow() const {
  const auto op        = dynamic_cast<ReduceSumGradOp *>(op_p);
  auto &input          = get(inId(0));
  auto input_shape     = inShape(0);
  auto output_shape    = outShape(0);
  const auto new_shape = vector_cast<std::size_t>(op->backwardShape());

  auto output = input.reshape(new_shape);

  // Broadcasting across each dimension
  for (int dim = 0; dim < new_shape.size(); ++dim) {
    if (new_shape[dim] != output_shape[dim]) {
      output = output.broadcast(output_shape[dim], dim);
    }
  }

  // output now matches the shape of output_shape
  insert(outId(0), output);
}

} // namespace popx
} // namespace willow
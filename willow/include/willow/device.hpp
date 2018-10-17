#ifndef GUARD_NEURALNET_DEVICE_HPP
#define GUARD_NEURALNET_DEVICE_HPP

#include <willow/names.hpp>
#include <willow/tensorinfo.hpp>

namespace willow {

class Device {

public:
  Device(const Graph *);
  virtual ~Device();
  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;
  virtual void prepare()            = 0;

protected:
  const Graph *graph;
};

} // namespace willow

#endif

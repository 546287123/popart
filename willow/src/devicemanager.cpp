#include <sstream>
#include <popart/devicemanager.hpp>
#include <popart/error.hpp>

namespace popart {

DeviceManager &DeviceManager::createDeviceManager() {
  static DeviceManager deviceManager;
  return deviceManager;
}

void DeviceManager::registerDeviceProvider(DeviceProvider *provider) {
  providers.push_back(provider);
}

std::vector<std::shared_ptr<DeviceInfo>>
DeviceManager::enumerateDevices(SyncPattern pattern,
                                uint32_t replication_factor) {
  std::vector<std::shared_ptr<DeviceInfo>> devices;
  for (auto p : providers) {
    p->enumerate(devices, pattern, replication_factor);
  }

  for (auto d : devices) {
    logging::debug("Device: {}", d.get()->toString());
  }

  return devices;
}

std::shared_ptr<DeviceInfo> DeviceManager::createCpuDevice() {
  for (auto p : providers) {
    std::shared_ptr<DeviceInfo> device =
        p->createHostDevice(DeviceType::Cpu, {});
    if (device != nullptr)
      return device;
  }
  return nullptr;
}

std::shared_ptr<DeviceInfo> DeviceManager::createIpuModelDevice(
    std::map<std::string, std::string> &options) {
  for (auto p : providers) {
    std::shared_ptr<DeviceInfo> device =
        p->createHostDevice(DeviceType::IpuModel, options);
    if (device != nullptr)
      return device;
  }
  return nullptr;
}

std::shared_ptr<DeviceInfo>
DeviceManager::createSimDevice(std::map<std::string, std::string> &options) {
  for (auto p : providers) {
    std::shared_ptr<DeviceInfo> device =
        p->createHostDevice(DeviceType::Sim, options);
    if (device != nullptr)
      return device;
  }
  return nullptr;
}

std::shared_ptr<DeviceInfo>
DeviceManager::acquireAvailableDevice(int numIpus,
                                      int tilesPerIpu,
                                      SyncPattern pattern,
                                      uint32_t replication_factor) {

  auto devices = enumerateDevices(pattern, replication_factor);

  for (auto &device : devices) {
    // Check if numIPUs is positive and a power of two
    if (numIpus > 0 && ((numIpus & (numIpus - 1)) == 0)) {
      if (numIpus == device->getNumIpus() &&
          (!tilesPerIpu || tilesPerIpu == device->getTilesPerIpu())) {

        // Attach to the device. Will succeed if available
        if (device->attach()) {
          return device;
        }
      }
    } else {
      throw error("You have attempted to acquire {} IPUs. The number of IPUs "
                  "requested must be a power of two",
                  numIpus);
    }
  }

  return nullptr;
}

std::shared_ptr<DeviceInfo>
DeviceManager::acquireDeviceById(int id,
                                 SyncPattern pattern = SyncPattern::FULL,
                                 uint32_t replication_factor = 1) {

  auto devices = enumerateDevices(pattern, replication_factor);

  for (auto &device : devices) {
    if (device->getId() == id) {
      if (device->attach())
        return device;
    }
  }

  return nullptr;
}

std::ostream &operator<<(std::ostream &os, const DeviceType &dt) {
  switch (dt) {
  case DeviceType::Cpu:
    os << "cpu";
    break;
  case DeviceType::Ipu:
    os << "ipu";
    break;
  case DeviceType::IpuModel:
    os << "ipu-model";
    break;
  case DeviceType::Sim:
    os << "sim";
    break;
  }

  return os;
}

std::string DeviceInfo::toString() const {
  std::stringstream ss;

  ss << "Device Type:" << getType() << " Id:" << getId()
     << " Version:" << getVersion() << " NumIPUs:" << getNumIpus()
     << " NumTilesPerIPU:" << getTilesPerIpu();

  ss << " DeviceIds: {";
  std::string sep;
  for (unsigned i : getDriverIds()) {
    ss << sep << i;
    sep = ",";
  }
  ss << "}";

  // TODO : Add all the information from Target

  return ss.str();
}

std::ostream &operator<<(std::ostream &os, const DeviceInfo &di) {
  return os << di.toString();
}

} // namespace popart

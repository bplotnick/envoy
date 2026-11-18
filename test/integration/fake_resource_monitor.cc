#include "test/integration/fake_resource_monitor.h"

namespace Envoy {

FakeResourceMonitor::~FakeResourceMonitor() { factory_.onMonitorDestroyed(this); }

void FakeResourceMonitor::updateResourceUsage(Server::ResourceUpdateCallbacks& callbacks) {
  Server::ResourceUsage usage;
  usage.resource_pressure_ = pressure_;
  callbacks.onSuccess(usage);
}

void FakeResourceMonitorFactory::onMonitorDestroyed(FakeResourceMonitor* monitor) {
  if (monitor_ == monitor) {
    monitor_ = nullptr;
  }
}

absl::StatusOr<Server::ResourceMonitorPtr> FakeResourceMonitorFactory::createResourceMonitor(
    const Protobuf::Message&, Server::Configuration::ResourceMonitorFactoryContext& context) {
  auto monitor = std::make_unique<FakeResourceMonitor>(context.mainThreadDispatcher(), *this);
  monitor_ = monitor.get();
  monitors_.push_back(monitor.get());
  return monitor;
}

FakeSynchronousFeedbackResourceMonitor::~FakeSynchronousFeedbackResourceMonitor() {
  factory_.onMonitorDestroyed(this);
}

void FakeSynchronousFeedbackResourceMonitorFactory::onMonitorDestroyed(
    FakeSynchronousFeedbackResourceMonitor* monitor) {
  if (monitor_ == monitor) {
    monitor_ = nullptr;
  }
}

absl::StatusOr<Server::ResourceMonitorPtr>
FakeSynchronousFeedbackResourceMonitorFactory::createResourceMonitor(
    const Protobuf::Message&, Server::Configuration::ResourceMonitorFactoryContext&) {
  auto monitor = std::make_unique<FakeSynchronousFeedbackResourceMonitor>(*this);
  monitor_ = monitor.get();
  return monitor;
}

} // namespace Envoy

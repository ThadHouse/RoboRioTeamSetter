#include <functional>
#include <string>
#include <memory>

class DnsFinder {
 public:
  DnsFinder();
  ~DnsFinder();

  bool StartSearch();
  void StopSearch();

  void SetOnFound(std::function<void(const std::string&, unsigned int, std::string_view)> onFound) {
    this->onFound = onFound;
  }

  void OnFound(const std::string& mac, unsigned int ipAddress, std::string_view name) {
    onFound(mac, ipAddress, name);
  }

  struct Impl;
  std::unique_ptr<Impl> pImpl;

 private:
  std::function<void(const std::string&, unsigned int, std::string_view)> onFound;
};

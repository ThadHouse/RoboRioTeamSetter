#include <functional>
#include <string>

class WindowsDns {
public:
    bool StartSearch();
    void StopSearch();

    void SetOnFound(std::function<void(unsigned int, std::string_view)> onFound);

    void OnFound(unsigned int ipAddress, std::string_view name);

private:
    std::function<void(unsigned int, std::string_view)> onFound;
    void* cancelRequest = nullptr;
};

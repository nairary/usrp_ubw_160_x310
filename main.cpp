#include "Application.h"
#include "USRPDevice.h"
#include <memory>

// Main code
int main()
{

    std::unique_ptr<Application> app = std::make_unique<Application>(1920, 1080);
   /* std::unique_ptr<USRPDevice> usrp_device = std::make_unique<USRPDevice>();
    usrp_device->StartStream();
    std::vector<float> data(32768);
    std::vector<float> localData(32768);
    bool is_data_ready = false;
    std::mutex mutex;
    std::thread t(&USRPDevice::GetUSRPData, usrp_device.get(), std::ref(data), std::ref(is_data_ready), std::ref(mutex));
    while(usrp_device->IsStreaming())
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (is_data_ready) {
            localData = data;
            is_data_ready = false;
        }
    }
    t.join();*/
    app->Run();
}
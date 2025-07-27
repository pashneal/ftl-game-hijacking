// Disable warning for usage of ctime
#define _CRT_SECURE_NO_WARNINGS

#include <chrono>
#include <ctime>
#include <format>
#include <iostream>
#include <string>
#include <thread>

void DisplayMessageOnInterval(const std::string& message) {

    const auto currentTime{ std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now()) };

    std::cout << std::format("{} @ {}",
        message, std::ctime(&currentTime)) << std::endl;
}

int main(int argc, char* argv[]) {

    while (true) {
        DisplayMessageOnInterval("Hello World!");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    return 0;
}

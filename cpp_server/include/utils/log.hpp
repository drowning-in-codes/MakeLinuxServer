#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <utils/utils.hpp>
class Logger {
public:
  virtual void logInfo(const std::string_view infoMessage) = 0;
  virtual ~Logger() = default;
};

class ConsoleLogger : public Logger {
public:
  void logInfo(const std::string_view message) override {
    std::string currentTime = TimeUtils::getCurrentTime("%Y-%m-%d %H:%M:%S");
    std::cout << "[INFO]" << currentTime << ": " << message << std::endl;
  }
};

class FileLogger : public Logger {
public:
  std::ofstream logFile;
  FileLogger(const std::string &logFilePath)
      : logFile(logFilePath, std::ios::app) {
    // 创建文件
    if (!logFile) {
      std::cerr << "Failed to open log file: " << logFilePath << std::endl;
      return;
    }
    // 当前时间
    auto time = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto local_time = std::localtime(&time_t);
    logFile << "Log file created on: " << local_time->tm_year + 1900 << "-"
            << local_time->tm_mon + 1 << "-" << local_time->tm_mday << " "
            << local_time->tm_hour << ":" << local_time->tm_min << ":"
            << local_time->tm_sec << std::endl;
    logFile << "File logger initialized." << std::endl;
  }

  void logInfo(const std::string_view message) override {
    // 当前时间 c++写法
    auto time = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto local_time = std::localtime(&time_t);
    logFile << "[" << std::put_time(local_time, "%Y-%m-%d %H:%M:%S") << "]"
            << "[INFO] " << message << std::endl;
  }
  ~FileLogger() {
    if (logFile.is_open()) {
      logFile.close();
    }
  }
};
#pragma once

#include "format.h"
#include "iowrap.h"
#include <iostream>
#include <memory>
#include <thread>

static const size_t LOG_COUNT = 100;

class RingLog {
public:
  RingLog() {
  }

  void log(const std::string& line) {
    m_Buffers[count % LOG_COUNT] = line;
    ++count;
  }

  std::vector<std::string> lines() const {
    std::vector<std::string> result;
    int start = count > LOG_COUNT ? (count % LOG_COUNT) : 0;
    for (int i = 0; i < LOG_COUNT; ++i) {
      result.push_back(m_Buffers[(start + i) % LOG_COUNT]);
    }
    return result;
  }

private:
  int count = 0;
  std::string m_Buffers[LOG_COUNT];
};

#ifdef NOLOG
#define LOG(...)
#define LOG_F(...)
#define LOG_BRACKET(...)
#define LOG_BRACKET_F(...)
#else
#define LOG(...) LogBracket::log(__VA_ARGS__)
#define LOG_F(pattern, ...) LogBracket::log(fmt::format(pattern, __VA_ARGS__))
#define LOG_BRACKET(...) LogBracket brack = LogBracket::create(__VA_ARGS__)
#define LOG_BRACKET_F(pattern, ...) LogBracket brack = LogBracket::create(fmt::format(pattern, __VA_ARGS__))
#define LOG_PRINT() LogBracket::print()
#endif

void debugStream(const std::shared_ptr<IOWrapper> &str);

class LogBracket {
public:
  ~LogBracket();

  static LogBracket create(const std::string &message);

  static void log(const std::string &message);

  static void print();

  static int getIndentDepth() {
    return s_Indent;
  }

protected:

  LogBracket(const std::string &message);

  static std::string indent();

private:
  static thread_local int s_Indent;
  static thread_local RingLog s_RingLog;
  std::string m_Message;
};

template <typename T> T read(std::istream &stream) {
  T res;
  stream.read(reinterpret_cast<char *>(&res), sizeof(T));
  return res;
}

template <typename T> void write(std::ostream &stream, const T &val) {
  LOG_F("write at {0}", stream.tellp());
  stream.write(reinterpret_cast<const char*>(&val), sizeof(T));
}

void printStack();
void printExceptionStack();


#include "typecast.h"
#include "util.h"
#include "format.h"
#include "dynobject.h"
#include <iostream>
#include <cassert>

#define DEF_TYPE(VAL_TYPE, TYPE_ID) \
template <> VAL_TYPE type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write) { \
  if (type != TYPE_ID) {\
    throw IncompatibleType(fmt::format("Expected {}", TYPE_ID).c_str());\
  }\
  VAL_TYPE result;\
  memcpy(reinterpret_cast<char*>(&result), index, sizeof(VAL_TYPE));\
  return result;\
}\
template <> char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const VAL_TYPE &value) {\
  if (type != TYPE_ID) {\
    throw IncompatibleType(fmt::format("Expected {}", TYPE_ID).c_str());\
  }\
  memcpy(index, reinterpret_cast<const char*>(&value), sizeof(VAL_TYPE));\
  return index + sizeof(VAL_TYPE);\
}

DEF_TYPE(int8_t, TypeId::int8);
DEF_TYPE(int16_t, TypeId::int16);
DEF_TYPE(int32_t, TypeId::int32);
DEF_TYPE(int64_t, TypeId::int64);
DEF_TYPE(uint8_t, TypeId::uint8);
DEF_TYPE(uint16_t, TypeId::uint16);
DEF_TYPE(uint32_t, TypeId::uint32);
DEF_TYPE(uint64_t, TypeId::uint64);
DEF_TYPE(float, TypeId::float32);

template <> std::string type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write) {
  if ((type != TypeId::stringz) && (type != TypeId::string)) {
    throw IncompatibleType(fmt::format("Expected string, got {}", type).c_str());
  }

  int32_t offset;
  memcpy(reinterpret_cast<char*>(&offset), index, sizeof(int32_t));
  index += sizeof(int32_t);

  auto &stream = (offset < 0)
    ? write
    : data;

  offset = std::abs(offset);
  // stream->clear();

  std::streampos curPos = stream->tellg();

  int32_t seekOffset = static_cast<int32_t>(curPos) - offset;
  if (seekOffset != 0) {
    stream->seekg(offset);
  }

  std::string result;

  int32_t size;

  if (type == TypeId::string) {
    memcpy(reinterpret_cast<char*>(&size), index, sizeof(int32_t));
    // LogBracket::log(fmt::format(">>> dynamic size {0}", size));
    result.resize(size);
    stream->read(&result[0], size);
  }
  else {
    int ch;
    size = 0;
    while ((ch = stream->get()) != '\0') {
      result.push_back(ch);
      ++size;
    }
  }

  if ((size * -1) != seekOffset) {
    stream->seekg(curPos);
  }
  // LOG_F("string read {0:x}: {1}", stream->tellg(), result);

  return result;
}

template <> std::vector<uint8_t> type_read(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write) {
  int32_t offset;
  memcpy(reinterpret_cast<char*>(&offset), index, sizeof(int32_t));
  index += sizeof(int32_t);

  auto &stream = (offset < 0)
    ? write
    : data;

  offset = std::abs(offset);

  std::streampos curPos = stream->tellg();

  int32_t seekOffset = static_cast<int32_t>(curPos) - offset;
  if (seekOffset != 0) {
    stream->seekg(offset);
  }

  std::vector<uint8_t> result;

  int32_t size;

  memcpy(reinterpret_cast<char*>(&size), index, sizeof(int32_t));
  if (size > 0) {
    result.resize(size);
    stream->read(reinterpret_cast<char*>(&result[0]), size);
  }

  if ((size * -1) != seekOffset) {
    stream->seekg(curPos);
  }

  return result;
}

template<>
char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::string &value) {
  write->seekendP();
  int32_t offset = static_cast<int32_t>(write->tellp()) * -1;

  char *pos = index;
  memcpy(pos, reinterpret_cast<char*>(&offset), sizeof(int32_t));
  pos += sizeof(int32_t);
  if (type == TypeId::string) {
    int32_t size = static_cast<int32_t>(value.length());
    memcpy(pos, reinterpret_cast<const char*>(&size), sizeof(int32_t));
    pos += sizeof(int32_t);
  }
  write->write(value.c_str(), value.size() + 1);
  return pos;
}

template<>
char *type_write(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::vector<uint8_t> &value) {
  write->seekendP();
  int32_t offset = static_cast<int32_t>(write->tellp()) * -1;

  char *pos = index;
  memcpy(pos, reinterpret_cast<char*>(&offset), sizeof(int32_t));
  pos += sizeof(int32_t);
  if (type == TypeId::string) {
    int32_t size = static_cast<int32_t>(value.size());
    memcpy(pos, reinterpret_cast<const char*>(&size), sizeof(int32_t));
    pos += sizeof(int32_t);
  }
  write->write(reinterpret_cast<const char*>(value.data()), value.size() + 1);
  return pos;
}

/*
template <>
void type_write(TypeId type, std::shared_ptr<IOWrapper> &index, std::shared_ptr<IOWrapper> &write, const uint64_t &value) {

}
*/

template <typename T> char *type_index_impl(char *index, std::shared_ptr<IOWrapper> &data, const SizeFunc &size, const DynObject *obj, bool sizeField);

template <typename T> char *type_index_num(char *index, std::shared_ptr<IOWrapper> &data) {
  try {
    data->read(index, sizeof(T));
    T x = *reinterpret_cast<T*>(index);
    LOG_F("indexed num {} at {}", *reinterpret_cast<T*>(index), data->tellg() - sizeof(T), x);
    return index + sizeof(T);
  }
  catch (const std::exception&) {
    throw;
  }
}

char *type_index_obj(char *index, std::shared_ptr<IOWrapper> &data, std::streampos dataPos, ObjSize size, const DynObject *obj) {
  // LogBracket::log(fmt::format("write index obj type {}, data {}, size {}", obj->getTypeId(), offset, size));
  int64_t pos = dataPos;
  memcpy(index, reinterpret_cast<char*>(&pos), sizeof(int64_t));
  data->seekg(dataPos + std::streamoff(size));
  return index + sizeof(int64_t);
}

template <> char *type_index_impl<std::string>(char *index, std::shared_ptr<IOWrapper> &data, const SizeFunc &sizeFunc, const DynObject *obj, bool sizeField) {
  int32_t offset = static_cast<int32_t>(data->tellg());
  if (offset < 0) {
    throw new std::runtime_error("invalid data offset");
  }

  memcpy(index, &offset, sizeof(int32_t));

  if (sizeField) {
    int32_t size = sizeFunc(*obj);
    char temp[5];
    std::streampos pos = data->tellg();
    data->read(temp, 4);
    temp[4] = '\0';
    static std::string lastStr;
    static std::streampos lastPos;

    if ((size == 4)
        && ((temp[3] < 'A') || (temp[3] > 'Z'))
        && ((temp[3] < '0') || (temp[3] > '9'))
        && (temp[3] != '_')) {
      if (memcmp(temp, "\0\0\0\0", 4) != 0) {
        exit(1);
      }
    }
    lastStr = temp;
    lastPos = pos;
    data->seekg(offset + size);
    memcpy(index + sizeof(int32_t), &size, sizeof(int32_t));
    index += sizeof(int32_t) * 2;
  } else {
    char ch;
    while ((ch = data->get()) != '\0') {}
    index += sizeof(int32_t);
  }
  // LogBracket::log(fmt::format("<<< write index impl offset {} size {}", offset, size));
  if (offset < 100) {
    LOG_F("index string offset {}", offset);
  }
  return index;
}

template <> char *type_index_impl<std::vector<uint8_t>>(char *index, std::shared_ptr<IOWrapper> &data, const SizeFunc &sizeFunc, const DynObject *obj, bool sizeField) {
  assert(sizeField == true);
  int32_t offset = static_cast<int32_t>(data->tellg());
  if (offset < 0) {
    throw new std::runtime_error("invalid data offset");
  }

  memcpy(index, &offset, sizeof(int32_t));

  ObjSize size = sizeFunc(*obj);
  std::streampos pos = data->tellg();
  data->seekg(static_cast<size_t>(offset) + size);
  memcpy(index + sizeof(int32_t), &size, sizeof(int32_t));
  index += sizeof(int32_t) * 2;

  return index;
}


char *type_index(TypeId type, const SizeFunc &size, char *index, std::shared_ptr<IOWrapper> &data, const DynObject *obj) {
  switch (type) {
    case TypeId::int8: return type_index_num<int8_t>(index, data);
    case TypeId::int16: return type_index_num<int16_t>(index, data);
    case TypeId::int32: return type_index_num<int32_t>(index, data);
    case TypeId::int64: return type_index_num<int64_t>(index, data);
    case TypeId::uint8: return type_index_num<uint8_t>(index, data);
    case TypeId::uint16: return type_index_num<uint16_t>(index, data);
    case TypeId::uint32: return type_index_num<uint32_t>(index, data);
    case TypeId::uint64: return type_index_num<uint64_t>(index, data);
    case TypeId::float32_iee754: return type_index_num<float>(index, data);
    case TypeId::stringz: return type_index_impl<std::string>(index, data, size, obj, false);
    case TypeId::string: return type_index_impl<std::string>(index, data, size, obj, true);
    case TypeId::bytes: return type_index_impl<std::vector<uint8_t>>(index, data, size, obj, true);
    case TypeId::custom: return type_index_obj(index, data, data->tellg(), size(*obj), obj);
  }
  throw std::runtime_error("invalid type");
}

std::any type_read_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write) {
  switch (type) {
    case TypeId::int8: return type_read<int8_t>(type, index, data, write);
    case TypeId::int16: return type_read<int16_t>(type, index, data, write);
    case TypeId::int32: return type_read<int32_t>(type, index, data, write);
    case TypeId::int64: return type_read<int64_t>(type, index, data, write);
    case TypeId::uint8: return type_read<uint8_t>(type, index, data, write);
    case TypeId::uint16: return type_read<uint16_t>(type, index, data, write);
    case TypeId::uint32: return type_read<uint32_t>(type, index, data, write);
    case TypeId::uint64: return type_read<uint64_t>(type, index, data, write);
    case TypeId::float32_iee754: return type_read<float>(type, index, data, write);
    case TypeId::stringz: return type_read<std::string>(type, index, data, write);
    case TypeId::string: return type_read<std::string>(type, index, data, write);
    case TypeId::bytes: return type_read<std::vector<uint8_t>>(type, index, data, write);
    // case TypeId::custom: return std::shared_ptr<DynObject>(new DynObject(type_read<DynObject>(type, index, data, write)));
    case TypeId::custom: throw std::runtime_error("not implemented");
  }
  return std::any();
}

char *type_write_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &write, const std::any &value) {
  switch (type) {
    case TypeId::int8: return type_write<int8_t>(type, index, write, flexi_cast<int8_t>(value));
    case TypeId::int16: return type_write<int16_t>(type, index, write, flexi_cast<int16_t>(value));
    case TypeId::int32: return type_write<int32_t>(type, index, write, flexi_cast<int32_t>(value));
    case TypeId::int64: return type_write<int64_t>(type, index, write, flexi_cast<int64_t>(value));
    case TypeId::uint8: return type_write<uint8_t>(type, index, write, flexi_cast<uint8_t>(value));
    case TypeId::uint16: return type_write<uint16_t>(type, index, write, flexi_cast<uint16_t>(value));
    case TypeId::uint32: return type_write<uint32_t>(type, index, write, flexi_cast<uint32_t>(value));
    case TypeId::uint64: return type_write<uint64_t>(type, index, write, flexi_cast<uint64_t>(value));
    case TypeId::float32_iee754: return type_write<float>(type, index, write, flexi_cast<float>(value));
    case TypeId::stringz: return type_write<std::string>(type, index, write, flexi_cast<std::string>(value));
    case TypeId::string: return type_write<std::string>(type, index, write, flexi_cast<std::string>(value));
    default: throw std::runtime_error("not implemented");
  }
}

template <typename T>
void stream_write(std::shared_ptr<IOWrapper> &output, const T &value);

void stream_write_strz(std::shared_ptr<IOWrapper> &output, const std::string &value) {
  output->write(value.c_str(), value.length() + 1);
}

void stream_write_str(std::shared_ptr<IOWrapper> &output, const std::string &value) {
  output->write(value.c_str(), value.length());
}

void stream_write_bytes(std::shared_ptr<IOWrapper> &output, const std::vector<uint8_t> &value) {
  output->write(reinterpret_cast<const char*>(value.data()), value.size());
}

template <typename T>
void stream_write(std::shared_ptr<IOWrapper> &output, const T &value) {
  output->write(reinterpret_cast<const char*>(&value), sizeof(T));
}

void type_copy_any(TypeId type, char *index, std::shared_ptr<IOWrapper> &output, std::shared_ptr<IOWrapper> &data, std::shared_ptr<IOWrapper> &write) {
  switch (type) {
  case TypeId::int8: stream_write(output, type_read<int8_t>(type, index, data, write)); break;
  case TypeId::int16: stream_write(output, type_read<int16_t >(type, index, data, write)); break;
  case TypeId::int32: stream_write(output, type_read<int32_t >(type, index, data, write)); break;
  case TypeId::int64: stream_write(output, type_read<int64_t >(type, index, data, write)); break;
  case TypeId::uint8: stream_write(output, type_read<uint8_t >(type, index, data, write)); break;
  case TypeId::uint16: stream_write(output, type_read<uint16_t>(type, index, data, write)); break;
  case TypeId::uint32: stream_write(output, type_read<uint32_t>(type, index, data, write)); break;
  case TypeId::uint64: stream_write(output, type_read<uint64_t>(type, index, data, write)); break;
  case TypeId::float32_iee754: stream_write(output, type_read<float>(type, index, data, write)); break;
  case TypeId::stringz: stream_write_strz(output, type_read<std::string>(type, index, data, write)); break;
  case TypeId::string: stream_write_str(output, type_read<std::string>(type, index, data, write)); break;
  case TypeId::bytes: stream_write_bytes(output, type_read<std::vector<uint8_t>>(type, index, data, write)); break;
  case TypeId::custom: throw std::runtime_error("not implemented");
  }
}


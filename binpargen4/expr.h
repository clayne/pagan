#pragma once

#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>

#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <vector>

#include "dynobject.h"
#include "flexi_cast.h"

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

template <typename T>
bool equal(const std::any &lhs, const std::any &rhs) {
  return flexi_cast<T>(lhs) == flexi_cast<T>(rhs);
}

static bool strEqual(const std::any &lhs, const std::any &rhs) {
  //std::cout << "= " << flexi_cast<std::string>(lhs) << " - " << flexi_cast<std::string>(rhs) << " -> "
  //  << strcmp(flexi_cast<std::string>(lhs).c_str(), flexi_cast<std::string>(rhs).c_str());
  return strcmp(flexi_cast<std::string>(lhs).c_str(), flexi_cast<std::string>(rhs).c_str()) == 0;
}

static const inline std::map<std::type_index, std::function<bool(const std::any &lhs, const std::any &rhs)>> comparators{
  { typeid(char), equal<char> },
  { typeid(short), equal<short> },
  { typeid(int), equal<int> },
  { typeid(long), equal<long> },
  { typeid(int64_t), equal<int64_t> },
  { typeid(std::string), strEqual },
};


static inline bool any_equal(const std::any &lhs, const std::any &rhs) {
  try {
    return comparators.at(lhs.type())(lhs, rhs);
  }
  catch (const std::out_of_range &) {
    std::cout << "types out of range" << std::endl;
    return false;
  }
  catch (const std::bad_any_cast &) {
    std::cout << "incompatible types" << std::endl;
    return false;
  }
}

struct Rearrange : std::true_type {
  template<typename... States>
  static void transform(std::unique_ptr<pegtl::parse_tree::node> &node, States &&...st) {
    if (node->children.size() == 1) {
      node = std::move(node->children.back());
    } else if (node->children.size() == 0) {
      return;
    } else {
      node->remove_content();
      auto &children = node->children;
      auto rhs = std::move(children.back());
      children.pop_back();
      auto op = std::move(children.back());
      children.pop_back();
      op->children.emplace_back(std::move(node));
      op->children.emplace_back(std::move(rhs));
      node = std::move(op);
      transform(node->children.front(), st...);
    }
  }
};

namespace ExpressionSpec {

  enum class Order : int
  {
  };

  typedef std::function<std::any(const std::string &key, uint64_t id)> VariableResolver;

  struct Operation
  {
    Order order;
    std::function<std::any(std::any, std::any)> func;
  };

  struct Stack
  {
    void push(const Operation &op)
    {
      while (!m_Operations.empty() && (m_Operations.back().order <= op.order)) {
        reduce();
      }
      m_Operations.push_back(op);
    }

    void push(const std::any &value)
    {
      m_Values.push_back(value);
    }

    std::any result()
    {
      while (!m_Operations.empty()) {
        reduce();
      }

      std::any result = m_Values.back();
      m_Values.clear();
      return result;
    }

  private:
    std::vector<Operation> m_Operations;
    std::vector<std::any> m_Values;

    void reduce()
    {
      std::any rhs = m_Values.back();
      m_Values.pop_back();
      std::any lhs = m_Values.back();
      m_Values.pop_back();
      const Operation op = m_Operations.back();
      m_Operations.pop_back();
      m_Values.push_back(op.func(lhs, rhs));
    }
  };

  class Stacks {
  public:
    Stacks()
    {
      open();
    }

    void open()
    {
      m_Values.emplace_back();
    }

    template <typename T>
    void push(const T &value)
    {
      // value can be a value or an operator
      m_Values.back().push(value);
    }

    void close()
    {
      const auto res = m_Values.back().result();
      m_Values.pop_back();
      m_Values.back().push(res);
    }

    std::any finish()
    {
      return m_Values.back().result();
    }

  private:
    std::vector<Stack> m_Values;
  };

  class Operators
  {
  public:
    Operators()
    {
      insert("*", Order(5), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) * flexi_cast<int64_t>(rhs); });
      insert("/", Order(5), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) / flexi_cast<int64_t>(rhs); });
      insert("%", Order(5), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) % flexi_cast<int64_t>(rhs); });
      insert("+", Order(6), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) + flexi_cast<int64_t>(rhs); });
      insert("-", Order(6), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) - flexi_cast<int64_t>(rhs); });
      insert("<<", Order(7), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) << flexi_cast<int64_t>(rhs); });
      insert(">>", Order(7), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >> flexi_cast<int64_t>(rhs); });
      insert("<", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) < flexi_cast<int64_t>(rhs); });
      insert(">", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) > flexi_cast<int64_t>(rhs); });
      insert("<=", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) <= flexi_cast<int64_t>(rhs); });
      insert(">=", Order(8), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >= flexi_cast<int64_t>(rhs); });
      insert("==", Order(9), [](const std::any &lhs, const std::any &rhs) { return any_equal(lhs, rhs); });
      insert("!=", Order(9), [](const std::any &lhs, const std::any &rhs) { return !any_equal(lhs, rhs); });
      insert("&", Order(10), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) & flexi_cast<int64_t>(rhs); });
      insert("^", Order(11), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) ^ flexi_cast<int64_t>(rhs); });
      insert("|", Order(12), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) | flexi_cast<int64_t>(rhs); });
      insert("&&", Order(13), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) && flexi_cast<bool>(rhs); });
      insert("||", Order(14), [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) || flexi_cast<bool>(rhs); });
    }

    void insert(const std::string& name, const Order order, const std::function<std::any(std::any, std::any) > &func)
    {
      m_Operators.insert({ name,{ order, func } });
    }

    const std::map<std::string, Operation> &ops() const
    {
      return m_Operators;
    }

  private:
    std::map<std::string, Operation> m_Operators;
  };

  using namespace tao::pegtl;

  class Ignored : public sor<space> {};

  struct Infix
  {
    using analyze_t = analysis::generic<analysis::rule_type::ANY>;

    template<apply_mode,
      rewind_mode,
      template<typename...> class Action,
      template<typename...> class Control,
      typename Input
    >
      static bool match(Input &input, const Operators &ops/*, Stacks &stacks, const VariableResolver&*/)
    {
      // return matchImpl(input, ops, stacks);
      return matchImpl(input, ops);
    }

  private:
    template<typename Input>
    static bool matchImpl(Input& in, const Operators &ops/*, Stacks &stacks*/)
    {
      std::string t;
      size_t size = in.size();
      size_t offset = 0;
      // no operator is more than 2 characters long
      while ((offset < size) && (offset < 2)) {
        char ch = in.peek_char(offset);
        ++offset;
        if (ch == ' ') {
          break;
        }

        t.push_back(ch);
      }

      auto iter = ops.ops().lower_bound(t);
      while ((t.length() > 0) && ((iter == ops.ops().end()) || (iter->first != t))) {
        t.pop_back();
        iter = ops.ops().lower_bound(t);
      }

      if (t.length() > 0) {
        // stacks.push(iter->second);
        in.bump(t.size());
        return true;
      }
      else {
        return false;
      }
    }
  };

  struct Number : seq<opt<one<'+', '-'>>, plus<digit>> {};
  struct HexNumber : seq<one<'0'>, one<'x'>, plus<xdigit>> {};
  struct String : seq<one<'"'>, star<not_one<'"'>>, one<'"'>> {};
  struct Variable : seq<sor<alpha, one<'_'>>, star<sor<alnum, one<'.'>, one<'_'>>>> {};
  struct Expression;
  struct Bracket : if_must<one<'('>, Expression, one<')'>> {};
  struct Atomic : sor<HexNumber, Number, String, Variable, Bracket> {};

  struct Expression : list<Atomic, Infix, Ignored> {};
  struct Grammar : must<Expression, eof> {};

  template<typename Rule>
  struct Action : pegtl::nothing<Rule> {};

  template <>
  struct Action<Number>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver&)
    {
      stacks.push(std::stol(input.string()));
    }
  };

  template <>
  struct Action<HexNumber>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver&)
    {
      stacks.push(std::stol(input.string(), nullptr, 16));
    }
  };

  template <>
  struct Action<String>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver&)
    {
      std::string inString = input.string();
      stacks.push(inString.substr(1, inString.length() - 2));
    }
  };

  template <>
  struct Action<Variable>
  {
    template<typename Input>
    static void apply(const Input &input, const Operators&, Stacks &stacks, const VariableResolver &variables)
    {
      stacks.push(variables(input.string()));
    }
  };

  template <>
  struct Action<one<'('>>
  {
    static void apply0(const Operators&, Stacks &stacks, const VariableResolver&)
    {
      stacks.open();
    }
  };

  template <>
  struct Action<one<')'>>
  {
    static void apply0(const Operators&, Stacks& stacks, const VariableResolver &)
    {
      stacks.close();
    }
  };

  template <typename Rule>
  using Selector = pegtl::parse_tree::selector<
    Rule,
    pegtl::parse_tree::apply_store_content::to<Number, HexNumber, String, Variable, Infix>,
    pegtl::parse_tree::apply_remove_content::to<>,
    pegtl::parse_tree::apply<Rearrange>::to<Expression>
  >;
}

class TreeNode {
public:
  TreeNode() = default;
  ~TreeNode() = default;

  std::string value() const { return "42"; }

  // all non-root nodes are initialized by calling this method
  template<typename Rule, typename Input>
  void start(const Input &in) {
    std::cout << "start " << in.source() << std::endl;
  }

  // if parsing of the rule succeeded, this method is called
  template< typename Rule, typename Input >
  void success(const Input& in) {
    std::cout << "success " << in.source() << std::endl;
  }

  // if parsing of the rule failed, this method is called
  template< typename Rule, typename Input >
  void failure(const Input& in) {
    std::cout << "failure " << in.source() << std::endl;
  }

  // if parsing succeeded and the (optional) transform call
  // did not discard the node, it is appended to its parent.
  // note that "child" is the node whose Rule just succeeded
  // and *this is the parent where the node should be appended.
  void append(std::unique_ptr<TreeNode> child) {
    std::cout << "append" << std::endl;
  }

  void emplace_back(std::unique_ptr<TreeNode> child) {
    std::cout << "emplace_back" << std::endl;
  }

private:
  TreeNode(const TreeNode&) = delete;
  TreeNode(TreeNode&&) = delete;

  TreeNode& operator=(const TreeNode&) = delete;
  TreeNode& operator=(TreeNode&&) = delete;
};

static void printNode(const pegtl::parse_tree::node &node, const std::string &indent = "") {
  if (node.is_root()) {
    std::cout << "root" << std::endl;
  } else {
    if (node.has_content()) { 
      std::cout << indent << node.id->name() << " \"" << node.content() << "\" at " << node.begin() << " to " << node.end() << " - " << node.is<ExpressionSpec::Infix>() << std::endl;
    } else {
      std::cout << indent << node.id->name() << "\" at " << node.begin() << std::endl;
    }
  }

  if (!node.children.empty()) {
    auto nextIndent = indent + "  ";
    for (auto &ch : node.children) {
      printNode(*ch, nextIndent);
    }
  }
}

static std::map<std::string, std::function<std::any(const std::any&, const std::any&)>> operators = {
  { "*", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) * flexi_cast<int64_t>(rhs); } },
  { "/", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) / flexi_cast<int64_t>(rhs); }},
  { "%", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) % flexi_cast<int64_t>(rhs); }},
  { "+", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) + flexi_cast<int64_t>(rhs); }},
  { "-", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) - flexi_cast<int64_t>(rhs); }},
  { "<<", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) << flexi_cast<int64_t>(rhs); }},
  { ">>", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >> flexi_cast<int64_t>(rhs); }},
  { "<", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) < flexi_cast<int64_t>(rhs); }},
  { ">", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) > flexi_cast<int64_t>(rhs); }},
  { "<=", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) <= flexi_cast<int64_t>(rhs); }},
  { ">=", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) >= flexi_cast<int64_t>(rhs); }},
  { "==", [](const std::any &lhs, const std::any &rhs) { return any_equal(lhs, rhs); }},
  { "!=", [](const std::any &lhs, const std::any &rhs) { return !any_equal(lhs, rhs); }},
  { "&", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) & flexi_cast<int64_t>(rhs); }},
  { "^", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) ^ flexi_cast<int64_t>(rhs); }},
  { "|", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<int64_t>(lhs) | flexi_cast<int64_t>(rhs); }},
  { "&&", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) && flexi_cast<bool>(rhs); }},
  { "||", [](const std::any &lhs, const std::any &rhs) { return flexi_cast<bool>(lhs) || flexi_cast<bool>(rhs); }},
};


static std::any evalNode(const pegtl::parse_tree::node &node, const ExpressionSpec::VariableResolver &resolver) {
  if (node.is_root()) {
    return evalNode(*node.children.front(), resolver);
  }
  else if (node.is<ExpressionSpec::Infix>()) {
    const auto &iter = operators.find(node.content());

    std::any lhs = evalNode(*(node.children.at(0)), resolver);
    std::any rhs = evalNode(*(node.children.at(1)), resolver);
    return (iter->second)(lhs, rhs);
  } else if (node.is<ExpressionSpec::Variable>()) {
    return resolver(node.content(), reinterpret_cast<uint64_t>(node.m_begin.data));
  } else if (node.is<ExpressionSpec::HexNumber>()) {
    return strtol(node.content().c_str(), nullptr, 16);
  } else if (node.is<ExpressionSpec::Number>()) {
    return strtol(node.content().c_str(), nullptr, 10);
  } else if (node.is<ExpressionSpec::String>()) {
    const std::string &content = node.content();
    return std::string(content, 1, content.length() - 2);
  } else {
    return node.content();
  }
}

std::vector<std::string> splitVariable(const std::string &input);

template <typename T>
std::function<T(const DynObject &)> makeFuncImpl(const std::string &code) {
  ExpressionSpec::Operators operators;
  // TODO these are raw pointers that will never get cleaned. Since they are required in the
  // function we return, to clean up we'd have to wrap std::function I think, which probably isn't
  // worth it since we don't expect the function to be cleaned until the process ends
  pegtl::string_input<> *expressionString = new pegtl::string_input<>(code, "source");
  auto tree =
    pegtl::parse_tree::parse<ExpressionSpec::Grammar, ExpressionSpec::Selector>(*expressionString, operators).release();

  std::map<uint64_t, std::vector<std::string>> variables;

  return [tree, variables](const DynObject &obj) mutable -> T {
    ExpressionSpec::VariableResolver resolver = [&obj, &variables](const std::string &key, uint64_t id) -> std::any {
      auto varIter = variables.find(id);
      if (varIter == variables.end()) {
        variables.insert(std::pair<uint64_t, std::vector<std::string>>(id, splitVariable(key)));
        // variables[id] = splitVariable(key);
        varIter = variables.find(id);
        // varIter = variables.insert(std::make_pair(id, splitVariable(key))).first;
      }
      ObjectIndex *idx = obj.getIndex();
      return obj.getAny(varIter->second.begin(), varIter->second.end());
    };

    std::any res = evalNode(*tree, resolver);
    return flexi_cast<T>(res);
  };
}

template <typename T>
inline std::function<T(const DynObject &)> makeFunc(const std::string &code) {
  return makeFuncImpl<T>(code);
}

template <>
inline std::function<ObjSize(const DynObject &)> makeFunc(const std::string &code) {
  char *endPtr = nullptr;
  long num = std::strtol(code.c_str(), &endPtr, 10);
  if (*endPtr == '\0') {
    return [num](const DynObject &) -> ObjSize { return num; };
  }

  return makeFuncImpl<ObjSize>(code);
}

# orm-lite 高级用法

orm-lite 是一个基于 C++26 反射的轻量级 ORM 库，提供零样本板SQL查询生成、SQL注入防护以及完整的CRUD操作。

## 目录

1. [快速开始](#快速开始)
2. [C++26反射支持](#c26反射支持)
3. [SQL注入防护](#sql注入防护)
4. [CRUD API](#crud-api)
5. [高级查询](#高级查询)
6. [模型定义](#模型定义)
7. [适配器实现](#适配器实现)
8. [编译器要求](#编译器要求)

---

## 快速开始

### 定义模型

```cpp
#include <orm-lite.hpp>

using namespace orm;

// 定义模型
ORM_MODEL(User) {
    int id;
    std::string name;
    std::string email;
    bool active = true;
};

ORM_MODEL(Product) {
    int id;
    std::string name;
    double price;
    int stock;
    std::string category;
};
```

### 使用 Session 执行CRUD操作

```cpp
// 创建适配器（示例使用Mock适配器）
MockDatabase db;
Session session(db);

// INSERT
User user;
user.id = 1;
user.name = "John Doe";
user.email = "john@example.com";
user.active = true;
session.insert(user);

// SELECT
auto query = session.select<User>();
std::string sql = query.where("id", SqlBuilder::Op::GT, "0")
                       .where("active", SqlBuilder::Op::EQ, "true")
                       .limit(100)
                       .to_sql();

// UPDATE
User updated_user;
updated_user.id = 1;
updated_user.name = "Jane Doe";
updated_user.email = "jane@example.com";
updated_user.active = true;
session.update<User>(updated_user, {{"id", "'1'"}});

// DELETE
session.erase<User>({{"id", "'1'"}});
```

---

## C++26反射支持

### 编译期反射特性

orm-lite 利用 C++26 的 `std::meta` 提供编译期反射：

```cpp
#include <meta>

// 获取模型字段数量
template<typename T>
constexpr size_t field_count() {
    return std::meta::nonstatic_data_members_of(^^T).size();
}

static_assert(field_count<User>() == 4);  // id, name, email, active
```

### 自动获取字段名

```cpp
template<typename T>
constexpr auto get_column_names() {
    std::vector<std::string> columns;
    for (constexpr auto member : std::meta::nonstatic_data_members_of(^^T)) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(member)>, std::nilmeta>) {
            if (auto name = std::meta::identifier_of(member); !name.empty()) {
                columns.push_back(std::string(name));
            }
        }
    }
    return columns;
}

// 使用示例
auto cols = get_column_names<User>();
// cols = {"id", "name", "email", "active"}
```

### 反射约束

```cpp
template<typename T>
concept Reflectable = requires {
    std::meta::is_reflectable<^^T>;
    requires std::meta::is_class_type(^^T);
};

// 在函数中使用
template<typename T>
requires Reflectable<T>
bool insert_model(const T& model) {
    // 编译期检查T是否可反射
}
```

### 反射驱动的序列化

```cpp
template<typename T>
constexpr auto serialize_to_sql(const T& model) {
    std::vector<std::pair<std::string, std::string>> values;

    for (constexpr auto member : std::meta::nonstatic_data_members_of(^^T)) {
        if constexpr (!std::is_same_v<std::decay_t<decltype(member)>, std::nilmeta>) {
            if (auto name = std::meta::identifier_of(member); !name.empty()) {
                auto& value = model.*member;
                if constexpr (std::is_same_v<std::decay_t<decltype(value)>, std::string>) {
                    values.emplace_back(std::string(name), SqlString::quote(value));
                } else if constexpr (std::is_arithmetic_v<std::decay_t<decltype(value)>>) {
                    values.emplace_back(std::string(name), std::to_string(value));
                } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, bool>) {
                    values.emplace_back(std::string(name), value ? "true" : "false");
                }
            }
        }
    }
    return values;
}
```

---

## SQL注入防护

### 字符串转义

```cpp
// 基础转义
std::string safe = SqlString::escape("O'Reilly'; DROP TABLE users;--");
// 结果: "O''Reilly''; DROP TABLE users;--"

// 引号包裹
std::string quoted = SqlString::quote("O'Reilly");
// 结果: "'O''Reilly'"
```

### 标识符引用

```cpp
// 引用表名、列名
std::string col = SqlString::quote_identifier("user");
// 结果: "\"user\""
```

### 参数化查询

```cpp
class MyDatabase : public adapter::Database {
    bool execute_parameterized(const std::string& sql,
                                const std::vector<std::string>& params) override {
        std::string statement = sql;
        size_t pos = 0;
        for (const auto& param : params) {
            size_t found = statement.find("?", pos);
            if (found == std::string::npos) break;
            statement.replace(found, 1, SqlString::quote(param));
            pos = found + param.size() + 2;
        }
        return execute(statement);
    }
};
```

### 完整的转义函数

```cpp
// 支持以下字符的转义
std::string SqlString::escape(const std::string& s) {
    std::string result;
    for (char c : s) {
        switch (c) {
            case '\'': result += "''"; break;       // 单引号
            case '"':  result += "\"\""; break;      // 双引号
            case '\\': result += "\\\\"; break;      // 反斜杠
            case '\0': result += "\\0"; break;      // 空字符
            case '\b': result += "\\b"; break;      // 退格
            case '\n': result += "\\n"; break;      // 换行
            case '\r': result += "\\r"; break;      // 回车
            case '\t': result += "\\t"; break;      // 制表符
            default:   result += c; break;
        }
    }
    return result;
}
```

---

## CRUD API

### INSERT

#### 使用反射自动序列化

```cpp
template<typename T>
requires Reflectable<T>
bool insert(const T& model) {
    auto [columns, values] = serialize_model(model);
    std::string sql = "INSERT INTO " + get_table_name<T>() +
                      " (" + columns + ") VALUES (" + values + ")";
    return db_->execute(sql);
}
```

#### 手动指定列值

```cpp
// 对于非反射类型
template<typename T>
bool insert(const T& model, const std::vector<std::pair<std::string, std::string>>& columns_values) {
    std::string sql = SqlBuilder().build_insert(get_table_name<T>(), columns_values);
    return db_->execute(sql);
}
```

#### 使用示例

```cpp
User user;
user.id = 1;
user.name = "John Doe";
user.email = "john@example.com";

session.insert(user);
// 生成SQL: INSERT INTO User (id, name, email, active) VALUES (1, 'John Doe', 'john@example.com', true)
```

### SELECT

#### 基础查询

```cpp
// 获取SelectBuilder
auto query = session.select<User>();

// 链式调用构建查询
std::string sql = query.where("id", SqlBuilder::Op::GT, "0")
                       .where("active", SqlBuilder::Op::EQ, "true")
                       .order_by("name")
                       .limit(100)
                       .to_sql();
```

#### 指定列

```cpp
template<typename T>
requires Reflectable<T>
SqlBuilder select(const std::vector<std::string>& columns) {
    SqlBuilder builder;
    std::string cols;
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) cols += ", ";
        cols += SqlString::quote_identifier(columns[i]);
    }
    builder.select(cols).from(get_table_name<T>());
    return builder;
}
```

#### 使用示例

```cpp
auto query = session.select<User>({"id", "name", "email"});
std::string sql = query.where("id", SqlBuilder::Op::EQ, "1").to_sql();
```

### UPDATE

#### 更新单个模型

```cpp
template<typename T>
requires Reflectable<T>
SqlBuilder update(const T& model) {
    SqlBuilder builder;
    auto values = serialize_model(model);
    // 返回builder供后续添加条件
    return builder;
}
```

#### 更新带条件

```cpp
template<typename T>
requires Reflectable<T>
bool update_with_conditions(const T& model,
                             const std::vector<std::pair<std::string, std::string>>& conditions) {
    auto values = serialize_model(model);
    std::string sql = SqlBuilder().build_update(get_table_name<T>(), values, conditions);
    return db_->execute(sql);
}
```

#### 使用示例

```cpp
User user;
user.id = 1;
user.name = "Jane Doe";
user.email = "jane@example.com";

session.update_with_condition(user, "id", "1");
// 生成SQL: UPDATE User SET name = 'Jane Doe', email = 'jane@example.com' WHERE id = '1'
```

### DELETE

#### 删除带条件

```cpp
template<typename T>
requires Reflectable<T>
bool erase_with_conditions(const std::vector<std::pair<std::string, std::string>>& conditions) {
    std::string sql = SqlBuilder().build_delete(get_table_name<T>(), conditions);
    return db_->execute(sql);
}
```

#### 根据ID删除

```cpp
template<typename T>
requires Reflectable<T>
bool erase_by_id(const std::variant<int, long, long long, std::string>& id) {
    std::string id_val;
    if (auto i = std::get_if<int>(&id)) id_val = std::to_string(*i);
    else if (auto s = std::get_if<std::string>(&id)) id_val = SqlString::quote(*s);

    return erase_with_conditions({{"id", id_val}});
}
```

#### 使用示例

```cpp
// 按ID删除
session.erase_by_id<User>(42);

// 按条件删除
session.erase_with_condition<User>("id", "42");
```

---

## 高级查询

### 复合条件

```cpp
SqlBuilder builder;
auto sql = builder.select("*").from("user")
                  .where("id", SqlBuilder::Op::GT, "10")
                  .where("active", SqlBuilder::Op::EQ, "true")
                  .where("name", SqlBuilder::Op::LIKE, "A%")
                  .order_by("name")
                  .limit(100)
                  .build();

// 生成SQL: SELECT * FROM user WHERE id > 10 AND active = 'true' AND name LIKE 'A%' ORDER BY name LIMIT 100
```

### 分页查询

```cpp
// 使用offset和limit
auto sql = builder.select("*").from("user")
                  .order_by("id")
                  .offset(10)   // 跳过前10条
                  .limit(5)     // 每页5条
                  .build();
```

### 分组聚合

```cpp
auto sql = builder.select("category, COUNT(*) as count")
                  .from("product")
                  .group_by("category")
                  .having("COUNT(*) > 5")
                  .build();
```

### 降序排序

```cpp
auto sql = builder.select("*").from("user")
                  .order_by_desc("created_at")
                  .build();
```

### IN 子句

```cpp
// 字符串列表
auto sql1 = builder.select("*").from("user")
                    .where("name", SqlBuilder::Op::IN,
                           std::vector<std::string>{"Alice", "Bob", "Charlie"})
                    .build();

// 整数列表
auto sql2 = builder.select("*").from("product")
                    .where("id", SqlBuilder::Op::IN,
                           std::vector<int>{1, 2, 3, 4, 5})
                    .build();
```

### 空值检查

```cpp
auto sql = builder.select("*").from("user")
                  .where("phone", SqlBuilder::Op::IS_NULL)
                  .build();
```

---

## 模型定义

### ORM_MODEL 宏详解

```cpp
ORM_MODEL(name) {
    // 字段定义
    int id;
    std::string name;
    double price;
    bool active = true;  // 可指定默认值
};
```

### 生成的类型

```cpp
// 生成的User结构体
struct User {
    int id;
    std::string name;
    std::string email;
    bool active;
};

// 生成的ModelTraits特化
template<> struct detail::ModelTraits<User> {
    static constexpr std::string_view table_name = "User";
};
```

### 字段类型映射

| C++ 类型 | SQL 类型 |
|---------|---------|
| int, long, long long, unsigned int | INTEGER |
| double, float | REAL |
| std::string | TEXT |
| bool | BOOLEAN |

---

## 适配器实现

### 创建自定义适配器

```cpp
class MyDatabase : public adapter::Database {
public:
    bool connect() override {
        // 连接数据库逻辑
        return true;
    }

    bool execute(const std::string& sql) override {
        // 执行SQL语句
        // 可以返回成功/失败
        return true;
    }
};
```

### PostgreSQL 适配器示例

```cpp
#include <libpq-fe.h>

class PostgresAdapter : public adapter::Database {
public:
    PostgresAdapter(const std::string& conninfo) : conninfo_(conninfo) {}

    bool connect() override {
        conn_ = PQconnectdb(conninfo_.c_str());
        return PQstatus(conn_) == CONNECTION_OK;
    }

    bool execute(const std::string& sql) override {
        auto res = PQexec(conn_, sql.c_str());
        bool ok = PQresultStatus(res) == PGRES_TUPLES_OK ||
                  PQresultStatus(res) == PGRES_COMMAND_OK;
        PQclear(res);
        return ok;
    }

    ~PostgresAdapter() {
        if (conn_) PQfinish(conn_);
    }

private:
    PGconn* conn_ = nullptr;
    std::string conninfo_;
};
```

### SQLite 适配器示例

```cpp
#include <sqlite3.h>

class SQLiteAdapter : public adapter::Database {
public:
    SQLiteAdapter(const std::string& path) : db_path_(path) {}

    bool connect() override {
        return sqlite3_open(db_path_.c_str(), &db_) == SQLITE_OK;
    }

    bool execute(const std::string& sql) override {
        char* errmsg = nullptr;
        bool ok = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg) == SQLITE_OK;
        if (!ok && errmsg) sqlite3_free(errmsg);
        return ok;
    }

    ~SQLiteAdapter() {
        if (db_) sqlite3_close(db_);
    }

private:
    sqlite3* db_ = nullptr;
    std::string db_path_;
};
```

---

## 编译器要求

### 支持的编译器

ORM-lite 需要支持 C++26 及其反射特性：

- **GCC 14+**（需 `-std=c++2c -freflection` 标志）
- **Clang 18+**（需 `-std=c++2c` 标志）
- **MSVC 2025 18+**（实验性支持）

### CMakeLists.txt 配置

```cmake
cmake_minimum_required(VERSION 3.20)
project(orm-lite VERSION 1.0.0 LANGUAGES CXX)

# C++26 with reflection support
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags for C++26 reflection
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-std=c++2c -freflection)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-std=c++2c)
endif()
```

### 编译器标志说明

| 编译器 | 标志 | 说明 |
|-------|------|------|
| GCC | `-std=c++2c -freflection` | 启用C++26和反射 |
| Clang | `-std=c++2c` | 启用C++26（Clang 18+ 内置反射） |
| MSVC | `/std:c++26` | 启用C++26（实验性） |

---

## 许可证

本项目采用 MIT 许可证，欢迎自由使用、修改和分发。
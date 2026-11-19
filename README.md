# orm-lite

**C++26 reflection-powered lightweight ORM - compile-time SQL generation with zero boilerplate**

[![C++26](https://img.shields.io/badge/C%2B%2B26-active-green)](https://isocpp.org/std/status)
[![Stars](https://img.shields.io/github/stars/zkzWilliamzhou/orm-lite?style=social)](https://github.com/zkzWilliamzhou/orm-lite)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](./LICENSE)

## 🎯 Why This Exists

传统C++ ORM痛点：
- 手写SQL拼接字符串
- 映射文件/元数据
- 运行时查询构建慢

**orm-lite 用C++26反射自动生成SQL，编译期验证。**

## ✨ Key Features

- **零样板映射**：`ORM_MODEL(User)` 自动映射表结构
- **编译期SQL生成**：查询在编译期验证
- **PostgreSQL/MySQL/SQLite兼容**：统一API
- **自动事务管理**：RAII事务
- **反射驱动查询**：类型安全查询构建
- **SQL注入防护**：自动字符串转义

## 💻 Quick Start

```cpp
// 定义模型
ORM_MODEL(User) {
    int id;
    std::string name;
    std::string email;
    std::optional<std::string> phone;
};

// 创建会话
MockDatabase db;
Session session(db);

// 插入
auto user = User{.id = 1, .name = "Alice", .email = "alice@example.com"};
session.insert(user);

// 查询
auto users = session.select<User>()
    .where("name", SqlBuilder::Op::LIKE, "A%")
    .order_by("id")
    .to_sql();

// 更新
User updated = user;
updated.name = "Alice Updated";
session.update<User>(updated, {{"id", "1"}});

// 删除
session.erase<User>({{"id", "1"}});
```

## 📊 生成的SQL

### INSERT
```sql
INSERT INTO User (id, name, email, phone) VALUES (1, 'Alice', 'alice@example.com', NULL)
```

### SELECT
```sql
SELECT id, name, email, phone FROM User WHERE name LIKE 'A%' ORDER BY id
```

### UPDATE
```sql
UPDATE User SET name = 'Alice Updated', email = 'alice@new.com' WHERE id = 1
```

### DELETE
```sql
DELETE FROM User WHERE id = 1
```

## 🔧 设计哲学

### 1. 编译期表结构推导
```cpp
// 根据结构体自动推导表名和列名
// User -> User 表
// id -> id 列
// name -> name 列
```

### 2. 类型到SQL类型的映射
| C++类型 | SQL类型 | 约束 |
|---------|---------|------|
| int | INTEGER | 主键/外键 |
| std::string | TEXT | NOT NULL |
| double | REAL | - |
| bool | BOOLEAN | - |

### 3. SQL注入防护
```cpp
// 自动转义特殊字符
SqlString::escape("O'Reilly");  // "O''Reilly"
SqlString::quote("O'Reilly");   // "'O''Reilly'"
```

## 🤝 数据库适配器

### 创建自定义适配器
```cpp
class MyDatabase : public orm::adapter::Database {
public:
    bool connect() override { /* 连接逻辑 */ return true; }
    bool execute(const std::string& sql) override { /* 执行SQL */ return true; }
};
```

### PostgreSQL适配器示例
```cpp
class PostgresAdapter : public orm::adapter::Database {
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

private:
    PGconn* conn_ = nullptr;
    std::string conninfo_;
};
```

## 🏗️ 编译期验证

### 类型不匹配
```cpp
// 编译期检查条件类型
session.select<User>().where("id", SqlBuilder::Op::EQ, "not-a-number");
// 整数条件接受int类型
```

## 🔗 与C++26反射集成

```cpp
// 反射查询表结构
constexpr auto cols = std::meta::nonstatic_data_members_of(^^User);
for (constexpr auto col : cols) {
    constexpr auto name = std::meta::identifier_of(col);
    // 自动生成列列表
}
```

## 📄 许可证

MIT License
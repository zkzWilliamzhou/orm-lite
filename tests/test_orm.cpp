// test_orm.cpp - orm-lite unit tests
// Build with: g++ -std=c++2c -I../include test_orm.cpp -o test_orm

#include <orm-lite.hpp>
#include <cassert>
#include <iostream>

using namespace orm;

// Mock database adapter for testing
class MockDatabase : public adapter::Database {
public:
    bool connect() override { return true; }

    bool execute(const std::string& sql) override {
        last_sql_ = sql;
        executed_ = true;
        std::cout << "Executed: " << sql << "\n";
        return true;
    }

    const std::string& last_sql() const { return last_sql_; }
    bool was_executed() const { return executed_; }
    void reset() { executed_ = false; last_sql_.clear(); }

private:
    std::string last_sql_;
    bool executed_ = false;
};

// Test model
ORM_MODEL(User) {
    int id;
    std::string name;
    std::string email;
    bool active = true;
};
ORM_REGISTER(User, id, name, email, active);

// Test model with more fields
ORM_MODEL(Product) {
    int id;
    std::string name;
    double price;
    int stock;
    std::string category;
};
ORM_REGISTER(Product, id, name, price, stock, category);

// ============================================================================
// SQL Builder Tests
// ============================================================================

void test_sql_builder_select() {
    SqlBuilder builder;
    auto sql = builder.select("id, name, email")
                      .from("user")
                      .where("active", SqlBuilder::Op::EQ, "true")
                      .order_by("id")
                      .limit(10)
                      .build();

    std::cout << "Generated SQL: " << sql << "\n";

    assert(sql.find("SELECT id, name, email") != std::string::npos);
    assert(sql.find("FROM user") != std::string::npos);
    assert(sql.find("WHERE active = 'true'") != std::string::npos);
    assert(sql.find("ORDER BY id") != std::string::npos);
    assert(sql.find("LIMIT 10") != std::string::npos);

    std::cout << "✓ test_sql_builder_select passed\n";
}

void test_sql_builder_conditions() {
    SqlBuilder builder;

    // Test LIKE
    auto sql1 = builder.select("*").from("user")
                      .where("name", SqlBuilder::Op::LIKE, "A%")
                      .build();
    assert(sql1.find("LIKE 'A%'") != std::string::npos);

    // Test IS NULL
    builder = SqlBuilder();
    auto sql2 = builder.select("*").from("user")
                      .where("phone", SqlBuilder::Op::IS_NULL)
                      .build();
    assert(sql2.find("IS NULL") != std::string::npos);

    // Test numeric comparison
    builder = SqlBuilder();
    auto sql3 = builder.select("*").from("product")
                      .where("price", SqlBuilder::Op::GT, "100")
                      .build();
    assert(sql3.find("> '100'") != std::string::npos);

    std::cout << "✓ test_sql_builder_conditions passed\n";
}

void test_op_conversions() {
    SqlBuilder builder;

    // Test equality
    auto sql_eq = builder.select("*").from("test")
                        .where("col", SqlBuilder::Op::EQ, "val")
                        .build();
    assert(sql_eq.find("= 'val'") != std::string::npos);

    // Test not equal
    auto sql_ne = builder.select("*").from("test")
                        .where("col", SqlBuilder::Op::NE, "val")
                        .build();
    assert(sql_ne.find("!= 'val'") != std::string::npos);

    // Test less than
    auto sql_lt = builder.select("*").from("test")
                        .where("col", SqlBuilder::Op::LT, "val")
                        .build();
    assert(sql_lt.find("< 'val'") != std::string::npos);

    // Test greater than or equal
    auto sql_ge = builder.select("*").from("test")
                        .where("col", SqlBuilder::Op::GE, "val")
                        .build();
    assert(sql_ge.find(">=") != std::string::npos);

    std::cout << "✓ test_op_conversions passed\n";
}

void test_complex_query() {
    SqlBuilder builder;
    std::string sql = builder.select("id, name, email")
                             .from("user")
                             .where("id", SqlBuilder::Op::GT, "10")
                             .where("active", SqlBuilder::Op::EQ, "true")
                             .where("name", SqlBuilder::Op::LIKE, "A%")
                             .order_by("name")
                             .limit(100)
                             .build();

    std::cout << "Complex SQL: " << sql << "\n";

    assert(sql.find("SELECT id, name, email") != std::string::npos);
    assert(sql.find("FROM user") != std::string::npos);
    assert(sql.find("AND") != std::string::npos);
    assert(sql.find("ORDER BY name") != std::string::npos);
    assert(sql.find("LIMIT 100") != std::string::npos);

    std::cout << "✓ test_complex_query passed\n";
}

void test_field_operations() {
    SqlBuilder builder;

    // Test offset
    auto sql1 = builder.select("*").from("user")
                       .offset(10)
                       .limit(5)
                       .build();
    assert(sql1.find("OFFSET 10") != std::string::npos);
    assert(sql1.find("LIMIT 5") != std::string::npos);

    std::cout << "✓ test_field_operations passed\n";
}

void test_order_by() {
    SqlBuilder builder;

    auto sql1 = builder.select("*").from("user")
                       .order_by("name")
                       .build();
    assert(sql1.find("ORDER BY name") != std::string::npos);

    auto sql2 = builder.select("*").from("user")
                       .order_by_desc("created_at")
                       .build();
    assert(sql2.find("ORDER BY created_at DESC") != std::string::npos);

    std::cout << "✓ test_order_by passed\n";
}

void test_in_operator() {
    SqlBuilder builder;

    // Test IN with strings
    auto sql1 = builder.select("*").from("user")
                    .where("name", SqlBuilder::Op::IN,
                           std::vector<std::string>{"Alice", "Bob", "Charlie"})
                    .build();
    assert(sql1.find("IN") != std::string::npos);

    // Test IN with integers
    builder = SqlBuilder();
    auto sql2 = builder.select("*").from("product")
                    .where("id", SqlBuilder::Op::IN,
                           std::vector<int>{1, 2, 3, 4, 5})
                    .build();
    assert(sql2.find("IN (1, 2, 3, 4, 5)") != std::string::npos);

    std::cout << "✓ test_in_operator passed\n";
}

void test_comparison_operators() {
    SqlBuilder builder;

    auto sql_lt = builder.select("*").from("product")
                         .where("price", SqlBuilder::Op::LT, "10.0")
                         .build();
    assert(sql_lt.find("< '10.0'") != std::string::npos);

    auto sql_le = builder.select("*").from("product")
                         .where("price", SqlBuilder::Op::LE, "10.0")
                         .build();
    assert(sql_le.find("<=") != std::string::npos);

    auto sql_gt = builder.select("*").from("product")
                         .where("price", SqlBuilder::Op::GT, "10.0")
                         .build();
    assert(sql_gt.find("> '10.0'") != std::string::npos);

    auto sql_ge = builder.select("*").from("product")
                         .where("price", SqlBuilder::Op::GE, "10.0")
                         .build();
    assert(sql_ge.find(">=") != std::string::npos);

    auto sql_ne = builder.select("*").from("product")
                         .where("status", SqlBuilder::Op::NE, "discontinued")
                         .build();
    assert(sql_ne.find("!= 'discontinued'") != std::string::npos);

    std::cout << "✓ test_comparison_operators passed\n";
}

void test_boolean_values() {
    SqlBuilder builder;

    auto sql = builder.select("*").from("user")
                      .where("active", SqlBuilder::Op::EQ, true)
                      .build();
    assert(sql.find("active = true") != std::string::npos);

    builder = SqlBuilder();
    auto sql2 = builder.select("*").from("user")
                       .where("active", SqlBuilder::Op::EQ, false)
                       .build();
    assert(sql2.find("active = false") != std::string::npos);

    std::cout << "✓ test_boolean_values passed\n";
}

// ============================================================================
// SQL Injection Protection Tests
// ============================================================================

void test_sql_injection_protection() {
    // Test string escaping
    std::string malicious = "O'Reilly'; DROP TABLE users;--";
    std::string escaped = SqlString::escape(malicious);

    std::cout << "Original: " << malicious << "\n";
    std::cout << "Escaped: " << escaped << "\n";

    assert(escaped.find("O''Reilly") != std::string::npos);
    assert(escaped.find("DROP TABLE") != std::string::npos);

    // Test quoting
    std::string quoted = SqlString::quote(malicious);
    assert(quoted.front() == '\'' && quoted.back() == '\'');

    // Test identifier quoting
    std::string ident = SqlString::quote_identifier("user");
    assert(ident == "\"user\"");

    std::cout << "✓ test_sql_injection_protection passed\n";
}

void test_sql_string_class() {
    SqlString ss("test'value");
    auto sv = ss.c_str();
    assert(std::string_view(sv) == "test'value");

    std::cout << "✓ test_sql_string_class passed\n";
}

// ============================================================================
// Session and CRUD Tests
// ============================================================================

void test_session_insert() {
    MockDatabase db;
    Session session(db);

    User user;
    user.id = 1;
    user.name = "Test User";
    user.email = "test@example.com";
    user.active = true;

    bool result = session.insert(user);
    assert(result);
    assert(db.was_executed());

    std::cout << "✓ test_session_insert passed\n";
}

void test_session_select() {
    MockDatabase db;
    Session session(db);

    auto query = session.select<User>();
    std::string sql = query.where("id", SqlBuilder::Op::GT, "0")
                           .where("active", SqlBuilder::Op::EQ, "true")
                           .limit(100)
                           .to_sql();

    std::cout << "Select SQL: " << sql << "\n";

    assert(sql.find("SELECT") != std::string::npos);
    assert(sql.find("FROM") != std::string::npos);

    std::cout << "✓ test_session_select passed\n";
}

void test_session_update() {
    MockDatabase db;
    Session session(db);

    User user;
    user.id = 1;
    user.name = "Updated Name";
    user.email = "updated@example.com";
    user.active = false;

    bool result = session.update(user, {{"id", "1"}});
    assert(result);
    assert(db.was_executed());

    std::cout << "✓ test_session_update passed\n";
}

void test_session_delete() {
    MockDatabase db;
    Session session(db);

    bool result = session.erase<User>({{"id", "1"}});
    assert(result);
    assert(db.was_executed());

    std::cout << "✓ test_session_delete passed\n";
}

void test_session_delete_by_id() {
    MockDatabase db;
    Session session(db);

    bool result = session.erase_by_id<User>(42);
    assert(result);
    assert(db.was_executed());

    std::cout << "✓ test_session_delete_by_id passed\n";
}

// ============================================================================
// Insert/Update/Delete SQL Builder Tests
// ============================================================================

void test_insert_sql_builder() {
    SqlBuilder builder;
    auto sql = builder.build_insert("User", {
        {"id", "1"},
        {"name", SqlString::quote("John Doe")},
        {"email", SqlString::quote("john@example.com")}
    });

    std::cout << "Insert SQL: " << sql << "\n";

    assert(sql.find("INSERT INTO User") != std::string::npos);
    assert(sql.find("John Doe") != std::string::npos);

    std::cout << "✓ test_insert_sql_builder passed\n";
}

void test_update_sql_builder() {
    SqlBuilder builder;
    auto sql = builder.build_update("User", {
        {"name", SqlString::quote("Jane Doe")},
        {"email", SqlString::quote("jane@example.com")}
    }, {
        {"id", "42"}
    });

    std::cout << "Update SQL: " << sql << "\n";

    assert(sql.find("UPDATE User") != std::string::npos);
    assert(sql.find("SET") != std::string::npos);
    assert(sql.find("WHERE") != std::string::npos);

    std::cout << "✓ test_update_sql_builder passed\n";
}

void test_delete_sql_builder() {
    SqlBuilder builder;
    auto sql = builder.build_delete("User", {
        {"id", "42"}
    });

    std::cout << "Delete SQL: " << sql << "\n";

    assert(sql.find("DELETE FROM User") != std::string::npos);
    assert(sql.find("WHERE") != std::string::npos);

    std::cout << "✓ test_delete_sql_builder passed\n";
}

// ============================================================================
// Reflection Tests
// ============================================================================

void test_column_extraction() {
    auto cols = detail::get_column_names_impl<User>();
    assert(cols.size() == 4);
    assert(cols[0] == "id");
    assert(cols[1] == "name");
    assert(cols[2] == "email");
    assert(cols[3] == "active");

    std::cout << "✓ test_column_extraction passed\n";
}

void test_field_count() {
    constexpr size_t user_fields = detail::field_count<User>();
    assert(user_fields == 4);

    constexpr size_t product_fields = detail::field_count<Product>();
    assert(product_fields == 5);

    std::cout << "✓ test_field_count passed\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "=== orm-lite Tests ===\n\n";

    // SQL Builder Tests
    std::cout << "--- SQL Builder Tests ---\n";
    test_sql_builder_select();
    test_sql_builder_conditions();
    test_op_conversions();
    test_complex_query();
    test_field_operations();
    test_order_by();
    test_in_operator();
    test_comparison_operators();
    test_boolean_values();

    // SQL Injection Protection Tests
    std::cout << "\n--- SQL Injection Protection Tests ---\n";
    test_sql_injection_protection();
    test_sql_string_class();

    // Session and CRUD Tests
    std::cout << "\n--- Session and CRUD Tests ---\n";
    test_session_insert();
    test_session_select();
    test_session_update();
    test_session_delete();
    test_session_delete_by_id();

    // SQL Builder Insert/Update/Delete Tests
    std::cout << "\n--- SQL Builder Insert/Update/Delete Tests ---\n";
    test_insert_sql_builder();
    test_update_sql_builder();
    test_delete_sql_builder();

    // Reflection Tests
    std::cout << "\n--- Reflection Tests ---\n";
    test_column_extraction();
    test_field_count();

    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
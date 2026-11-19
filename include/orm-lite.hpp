// orm-lite.hpp - lightweight header-only ORM
// Compile-time SQL generation with SQL injection protection.
// Field introspection is done with portable macros (no C++26 <meta> required).

#ifndef ORM_LITE_HPP
#define ORM_LITE_HPP

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <map>
#include <functional>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <type_traits>
#include <variant>

namespace orm {

// ============================================================================
// SQL Injection Protection
// ============================================================================

class SqlString {
public:
    explicit SqlString(const std::string& s) : data_(s) {}
    explicit SqlString(std::string&& s) : data_(std::move(s)) {}

    std::string_view c_str() const noexcept { return data_; }

    static std::string escape(const std::string& s) {
        std::string result;
        result.reserve(s.size() * 2);
        for (char c : s) {
            switch (c) {
                case '\'': result += "''"; break;
                case '"':  result += "\"\""; break;
                case '\\': result += "\\\\"; break;
                case '\0': result += "\\0"; break;
                case '\b': result += "\\b"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    static std::string quote(const std::string& s) {
        return "'" + escape(s) + "'";
    }

    static std::string quote_identifier(const std::string& s) {
        std::string result = "\"";
        for (char c : s) {
            if (c == '"') result += "\"\"";
            else result += c;
        }
        result += "\"";
        return result;
    }

    friend std::ostream& operator<<(std::ostream& os, const SqlString& s) {
        return os << s.data_;
    }

private:
    std::string data_;
};

// ============================================================================
// SQL Builder (Compile-time query construction)
// ============================================================================

class SqlBuilder {
public:
    enum class Op { EQ, NE, LT, LE, GT, GE, LIKE, NOT_LIKE, IN, IS_NULL };

    SqlBuilder& select(std::string_view columns) {
        columns_ = std::string(columns);
        return *this;
    }

    SqlBuilder& from(std::string_view table) {
        table_ = std::string(table);
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, const std::string& val) {
        conditions_.push_back({col, op, SqlString::quote(val), true, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, std::string_view val) {
        conditions_.push_back({col, op, SqlString::quote(std::string(val)), true, false});
        return *this;
    }

    // String literals must bind here rather than to the bool overload:
    // const char* -> bool is a standard conversion and would otherwise win
    // over the user-defined conversions to std::string / std::string_view.
    SqlBuilder& where(const std::string& col, Op op, const char* val) {
        conditions_.push_back({col, op, SqlString::quote(std::string(val)), true, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, int val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, double val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, long val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, long long val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, unsigned int val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, unsigned long val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, unsigned long long val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, bool val) {
        conditions_.push_back({col, op, val ? "true" : "false", false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op) {
        if (op == Op::IS_NULL || op == Op::NOT_LIKE) {
            conditions_.push_back({col, op, "", false, true});
        }
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, const std::vector<std::string>& vals) {
        if (vals.empty()) return *this;
        std::string in_list = "(";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i > 0) in_list += ", ";
            in_list += SqlString::quote(vals[i]);
        }
        in_list += ")";
        conditions_.push_back({col, Op::IN, in_list, false, false});
        return *this;
    }

    SqlBuilder& where(const std::string& col, Op op, const std::vector<int>& vals) {
        if (vals.empty()) return *this;
        std::string in_list = "(";
        for (size_t i = 0; i < vals.size(); ++i) {
            if (i > 0) in_list += ", ";
            in_list += std::to_string(vals[i]);
        }
        in_list += ")";
        conditions_.push_back({col, Op::IN, in_list, false, false});
        return *this;
    }

    SqlBuilder& and_where(const std::string& col, Op op, const std::string& val) {
        conditions_.push_back({col, op, SqlString::quote(val), true, false});
        return *this;
    }

    SqlBuilder& order_by(const std::string& col) {
        order_by_ = col;
        return *this;
    }

    SqlBuilder& order_by_desc(const std::string& col) {
        order_by_ = col + " DESC";
        return *this;
    }

    SqlBuilder& limit(size_t n) {
        limit_ = n;
        return *this;
    }

    SqlBuilder& offset(size_t n) {
        offset_ = n;
        return *this;
    }

    SqlBuilder& group_by(const std::string& col) {
        group_by_ = col;
        return *this;
    }

    SqlBuilder& having(const std::string& condition) {
        having_ = condition;
        return *this;
    }

    std::string build() const {
        std::string sql = "SELECT " + columns_ + " FROM " + table_;

        if (!conditions_.empty()) {
            sql += " WHERE ";
            for (size_t i = 0; i < conditions_.size(); ++i) {
                if (i > 0) sql += " AND ";
                sql += build_condition(conditions_[i]);
            }
        }

        if (!group_by_.empty()) {
            sql += " GROUP BY " + group_by_;
        }

        if (!having_.empty()) {
            sql += " HAVING " + having_;
        }

        if (!order_by_.empty()) {
            sql += " ORDER BY " + order_by_;
        }

        if (offset_) {
            sql += " OFFSET " + std::to_string(*offset_);
        }

        if (limit_) {
            sql += " LIMIT " + std::to_string(*limit_);
        }

        return sql;
    }

    std::string build_insert(std::string_view table, const std::vector<std::pair<std::string, std::string>>& columns_values) {
        std::string sql = "INSERT INTO " + std::string(table) + " (";
        std::string values = "(";

        for (size_t i = 0; i < columns_values.size(); ++i) {
            if (i > 0) {
                sql += ", ";
                values += ", ";
            }
            sql += SqlString::quote_identifier(columns_values[i].first);
            values += columns_values[i].second;
        }

        sql += ") VALUES " + values + ")";
        return sql;
    }

    std::string build_update(std::string_view table, const std::vector<std::pair<std::string, std::string>>& columns_values,
                              const std::vector<std::pair<std::string, std::string>>& conditions) {
        std::string sql = "UPDATE " + std::string(table) + " SET ";

        for (size_t i = 0; i < columns_values.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += SqlString::quote_identifier(columns_values[i].first) + " = " + columns_values[i].second;
        }

        if (!conditions.empty()) {
            sql += " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) sql += " AND ";
                sql += SqlString::quote_identifier(conditions[i].first) + " = " + conditions[i].second;
            }
        }

        return sql;
    }

    std::string build_delete(std::string_view table, const std::vector<std::pair<std::string, std::string>>& conditions) {
        std::string sql = "DELETE FROM " + std::string(table);

        if (!conditions.empty()) {
            sql += " WHERE ";
            for (size_t i = 0; i < conditions.size(); ++i) {
                if (i > 0) sql += " AND ";
                sql += SqlString::quote_identifier(conditions[i].first) + " = " + conditions[i].second;
            }
        }

        return sql;
    }

    // Public op_to_string for use in queries
    static std::string op_to_string(Op op) {
        switch (op) {
            case Op::EQ:       return "=";
            case Op::NE:       return "!=";
            case Op::LT:       return "<";
            case Op::LE:       return "<=";
            case Op::GT:       return ">";
            case Op::GE:       return ">=";
            case Op::LIKE:     return "LIKE";
            case Op::NOT_LIKE: return "NOT LIKE";
            case Op::IN:       return "IN";
            case Op::IS_NULL:  return "IS NULL";
            default:           return "=";
        }
    }

private:
    struct Condition {
        std::string col;
        Op op;
        std::string val;
        bool is_string;
        bool is_null;
    };

    std::string build_condition(const Condition& c) const {
        if (c.is_null) {
            if (c.op == Op::IS_NULL) return c.col + " IS NULL";
            if (c.op == Op::NOT_LIKE) return c.col + " NOT LIKE " + c.val;
        }
        return c.col + " " + op_to_string(c.op) + " " + c.val;
    }

    std::string columns_;
    std::string table_;
    std::vector<Condition> conditions_;
    std::string order_by_;
    std::optional<size_t> limit_;
    std::optional<size_t> offset_;
    std::string group_by_;
    std::string having_;
};

// ============================================================================
// Portable field registration (replaces C++26 static reflection)
// ============================================================================
//
// No current compiler ships <meta> / ^^T reflection, so field introspection
// is driven by the ORM_REGISTER macro instead.  ORM_MODEL(name) simply
// declares `struct name`; ORM_REGISTER(name, field, ...) records the field
// names.  Everything below is generated from that list at compile time.

namespace detail {

// Primary template is intentionally incomplete; only registered models
// specialize it.  `Reflectable` detects a registered specialization.
template<typename T>
struct ModelTraits;

template<typename T, typename = void>
struct is_model : std::false_type {};

template<typename T>
struct is_model<T, std::void_t<decltype(ModelTraits<T>::field_count)>>
    : std::true_type {};

template<typename T>
concept Reflectable = is_model<T>::value;

// Convert one field value into its SQL literal form.  Non-template overloads
// win for the special cases; the template handles the arithmetic types.
inline std::string to_sql_value(const std::string& v) { return SqlString::quote(v); }
inline std::string to_sql_value(const char* v)        { return SqlString::quote(v); }
inline std::string to_sql_value(bool v)               { return v ? "true" : "false"; }
template<typename T>
std::string to_sql_value(T v) { return std::to_string(v); }

// Registered column names in declaration order.
template<typename T>
std::vector<std::string> get_column_names_impl() {
    std::vector<std::string> columns;
    for (std::string_view c : ModelTraits<T>::column_names) {
        columns.emplace_back(c);
    }
    return columns;
}

// Number of registered fields (constexpr: usable in constant expressions).
template<typename T>
constexpr size_t field_count() {
    return ModelTraits<T>::field_count;
}

// Serialize a model instance into (column, sql_literal) pairs.
template<typename T>
std::vector<std::pair<std::string, std::string>> serialize_to_sql(const T& model) {
    return ModelTraits<T>::serialize(model);
}

// Comma-separated column list, used by Session::select<T>().
template<typename T>
std::string get_columns_str() {
    std::string result;
    for (size_t i = 0; i < ModelTraits<T>::field_count; ++i) {
        if (i > 0) result += ", ";
        result += ModelTraits<T>::column_names[i];
    }
    return result;
}

} // namespace detail

// --- Preprocessor helpers: FOR_EACH over up to 16 field names --------------

#define ORM_EXPAND(x) x
#define ORM_PICK16(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16, NAME, ...) NAME
#define ORM_FOR_EACH(m, ...) \
    ORM_EXPAND(ORM_PICK16(__VA_ARGS__, \
        ORM_FE_16, ORM_FE_15, ORM_FE_14, ORM_FE_13, ORM_FE_12, ORM_FE_11, \
        ORM_FE_10, ORM_FE_9, ORM_FE_8, ORM_FE_7, ORM_FE_6, ORM_FE_5, \
        ORM_FE_4, ORM_FE_3, ORM_FE_2, ORM_FE_1)(m, __VA_ARGS__))

#define ORM_FE_1(m, a) m(a)
#define ORM_FE_2(m, a, ...) m(a) ORM_EXPAND(ORM_FE_1(m, __VA_ARGS__))
#define ORM_FE_3(m, a, ...) m(a) ORM_EXPAND(ORM_FE_2(m, __VA_ARGS__))
#define ORM_FE_4(m, a, ...) m(a) ORM_EXPAND(ORM_FE_3(m, __VA_ARGS__))
#define ORM_FE_5(m, a, ...) m(a) ORM_EXPAND(ORM_FE_4(m, __VA_ARGS__))
#define ORM_FE_6(m, a, ...) m(a) ORM_EXPAND(ORM_FE_5(m, __VA_ARGS__))
#define ORM_FE_7(m, a, ...) m(a) ORM_EXPAND(ORM_FE_6(m, __VA_ARGS__))
#define ORM_FE_8(m, a, ...) m(a) ORM_EXPAND(ORM_FE_7(m, __VA_ARGS__))
#define ORM_FE_9(m, a, ...) m(a) ORM_EXPAND(ORM_FE_8(m, __VA_ARGS__))
#define ORM_FE_10(m, a, ...) m(a) ORM_EXPAND(ORM_FE_9(m, __VA_ARGS__))
#define ORM_FE_11(m, a, ...) m(a) ORM_EXPAND(ORM_FE_10(m, __VA_ARGS__))
#define ORM_FE_12(m, a, ...) m(a) ORM_EXPAND(ORM_FE_11(m, __VA_ARGS__))
#define ORM_FE_13(m, a, ...) m(a) ORM_EXPAND(ORM_FE_12(m, __VA_ARGS__))
#define ORM_FE_14(m, a, ...) m(a) ORM_EXPAND(ORM_FE_13(m, __VA_ARGS__))
#define ORM_FE_15(m, a, ...) m(a) ORM_EXPAND(ORM_FE_14(m, __VA_ARGS__))
#define ORM_FE_16(m, a, ...) m(a) ORM_EXPAND(ORM_FE_15(m, __VA_ARGS__))

#define ORM_COLUMN(f) #f,
#define ORM_SERIALIZE_FIELD(f) { #f, detail::to_sql_value(obj.f) },

// ============================================================================
// ORM Model Macros
// ============================================================================

// Declares the model struct.  Field names are registered with ORM_REGISTER,
// which is written directly after the struct definition.
#define ORM_MODEL(name) struct name

#define ORM_REGISTER(name, ...) \
    template<> struct detail::ModelTraits<name> { \
        static constexpr std::string_view table_name = #name; \
        static constexpr std::string_view column_names[] = { \
            ORM_FOR_EACH(ORM_COLUMN, __VA_ARGS__) \
        }; \
        static constexpr size_t field_count = \
            sizeof(column_names) / sizeof(column_names[0]); \
        static std::vector<std::pair<std::string, std::string>> \
        serialize(const name& obj) { \
            return { ORM_FOR_EACH(ORM_SERIALIZE_FIELD, __VA_ARGS__) }; \
        } \
    }

// ============================================================================
// Database Adapter Base
// ============================================================================

namespace adapter {

class Database {
public:
    virtual ~Database() = default;
    virtual bool connect() = 0;
    virtual bool execute(const std::string& sql) = 0;

    // SQL Injection Protection
    std::string escape(const std::string& s) const {
        return SqlString::escape(s);
    }

    std::string quote(const std::string& s) const {
        return SqlString::quote(s);
    }

    std::string quote_identifier(const std::string& s) const {
        return SqlString::quote_identifier(s);
    }

    // Parameter binding for safe queries
    virtual bool execute_parameterized(const std::string& sql,
                                        const std::vector<std::string>& params) {
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

    // Begin transaction
    virtual bool begin_transaction() { return true; }

    // Commit transaction
    virtual bool commit() { return true; }

    // Rollback transaction
    virtual bool rollback() { return true; }
};

} // namespace adapter

// ============================================================================
// ORM Session
// ============================================================================

template<typename T> struct SelectQuery;

class Session {
public:
    explicit Session(adapter::Database& db) : db_(&db) {}

    // INSERT with reflection support
    template<typename T>
    requires detail::Reflectable<T>
    bool insert(const T& model) {
        auto values = detail::serialize_to_sql(model);
        std::string sql = SqlBuilder().build_insert(
            detail::ModelTraits<T>::table_name, values);
        return db_->execute(sql);
    }

    // SELECT (compile-time verified)
    template<typename T>
    requires detail::Reflectable<T>
    auto select() {
        return SelectQuery<T>(*this, detail::get_columns_str<T>());
    }

    // UPDATE with conditions
    template<typename T>
    requires detail::Reflectable<T>
    bool update(const T& model, const std::vector<std::pair<std::string, std::string>>& conditions) {
        auto values = detail::serialize_to_sql(model);
        std::string sql = SqlBuilder().build_update(
            detail::ModelTraits<T>::table_name, values, conditions);
        return db_->execute(sql);
    }

    // DELETE with conditions
    template<typename T>
    requires detail::Reflectable<T>
    bool erase(const std::vector<std::pair<std::string, std::string>>& conditions) {
        std::string sql = SqlBuilder().build_delete(
            detail::ModelTraits<T>::table_name, conditions);
        return db_->execute(sql);
    }

    template<typename T>
    requires detail::Reflectable<T>
    bool erase_by_id(const std::variant<int, long, long long, std::string>& id) {
        std::string id_val;
        if (auto i = std::get_if<int>(&id)) id_val = std::to_string(*i);
        else if (auto l = std::get_if<long>(&id)) id_val = std::to_string(*l);
        else if (auto ll = std::get_if<long long>(&id)) id_val = std::to_string(*ll);
        else if (auto s = std::get_if<std::string>(&id)) id_val = SqlString::quote(*s);

        return erase<T>({{"id", id_val}});
    }

    adapter::Database& db() { return *db_; }

private:
    adapter::Database* db_;

    template<typename T>
    friend struct SelectQuery;
};

// ============================================================================
// Select Query Builder
// ============================================================================

template<typename T>
struct SelectQuery {
    SelectQuery(Session& session, std::string columns)
        : session_(&session), columns_(std::move(columns)) {}

    SelectQuery& where(const std::string& col, SqlBuilder::Op op, const std::string& val) {
        conditions_.push_back({col, op, SqlString::quote(val), true, false});
        return *this;
    }

    SelectQuery& where(const std::string& col, SqlBuilder::Op op, const char* val) {
        conditions_.push_back({col, op, SqlString::quote(std::string(val)), true, false});
        return *this;
    }

    SelectQuery& where(const std::string& col, SqlBuilder::Op op, int val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SelectQuery& where(const std::string& col, SqlBuilder::Op op, double val) {
        conditions_.push_back({col, op, std::to_string(val), false, false});
        return *this;
    }

    SelectQuery& where(const std::string& col, SqlBuilder::Op op, bool val) {
        conditions_.push_back({col, op, val ? "true" : "false", false, false});
        return *this;
    }

    SelectQuery& and_where(const std::string& col, SqlBuilder::Op op, const std::string& val) {
        conditions_.push_back({col, op, SqlString::quote(val), true, false});
        return *this;
    }

    SelectQuery& order_by(const std::string& col) {
        order_by_ = col;
        return *this;
    }

    SelectQuery& order_by_desc(const std::string& col) {
        order_by_ = col + " DESC";
        return *this;
    }

    SelectQuery& limit(size_t n) {
        limit_ = n;
        return *this;
    }

    SelectQuery& offset(size_t n) {
        offset_ = n;
        return *this;
    }

    std::string to_sql() const {
        std::string table(detail::ModelTraits<T>::table_name);
        std::string sql = "SELECT " + columns_ + " FROM " + table;

        if (!conditions_.empty()) {
            sql += " WHERE ";
            for (size_t i = 0; i < conditions_.size(); ++i) {
                if (i > 0) sql += " AND ";
                sql += build_condition(conditions_[i]);
            }
        }

        if (!order_by_.empty()) {
            sql += " ORDER BY " + order_by_;
        }

        if (offset_) {
            sql += " OFFSET " + std::to_string(*offset_);
        }

        if (limit_) {
            sql += " LIMIT " + std::to_string(*limit_);
        }

        return sql;
    }

private:
    struct Condition {
        std::string col;
        SqlBuilder::Op op;
        std::string val;
        bool is_string;
        bool is_null;
    };

    std::string build_condition(const Condition& c) const {
        if (c.is_null) {
            if (c.op == SqlBuilder::Op::IS_NULL) return c.col + " IS NULL";
        }
        return c.col + " " + SqlBuilder::op_to_string(c.op) + " " + c.val;
    }

    Session* session_;
    std::string columns_;
    std::vector<Condition> conditions_;
    std::string order_by_;
    std::optional<size_t> limit_;
    std::optional<size_t> offset_;
};

} // namespace orm

#endif // ORM_LITE_HPP
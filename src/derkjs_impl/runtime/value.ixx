module;

#include <cstddef>
#include <concepts>
#include <utility>
#include <memory>
#include <optional>
#include <flat_map>
#include <string>

export module runtime.value;

/// TODO: 1st, Refactor Object into an ObjectBase interface with Boolean, Number, Object, and Function values.
/// TODO: 2nd, Add comparison support for `Value`.
export namespace DerkJS {
    template <typename Obj>
    class Prototype;

    template <typename V>
    class Prototype {
    public:
        enum class FreezeFlag : uint8_t {
            yes,
            no
        };

    private:
        std::flat_map<std::string, V> m_shape_props;
        std::vector<int> m_instance_ids;
        std::shared_ptr<Prototype> m_parent_p;
        bool m_modifiable;

    public:
        Prototype(FreezeFlag frozen_flag)
        : m_shape_props {}, m_instance_ids {}, m_parent_p {}, m_modifiable {frozen_flag == FreezeFlag::no} {}

        Prototype(FreezeFlag frozen_flag, std::shared_ptr<Prototype<V>> parent_p)
        : m_shape_props {}, m_instance_ids {}, m_parent_p {parent_p}, m_modifiable {frozen_flag == FreezeFlag::no} {}

        [[nodiscard]] auto get_properties() noexcept -> std::flat_map<std::string, V>& {
            return m_shape_props;
        }

        [[nodiscard]] auto set_property(const std::string& name, std::same_as<V> auto&& value) {
            m_shape_props[name] = std::forward<V>(value);
        }

        [[nodiscard]] auto get_instance_ids() const noexcept -> const std::vector<int>& {
            return m_instance_ids;
        }

        void record_instance_id(int id) {
            m_instance_ids.emplace_back(id);
        }

        [[maybe_unused]] auto remove_instance_id(int id) -> bool {
            if (auto inst_id_it = std::find(m_instance_ids.begin(), m_instance_ids.end(), id); inst_id_it != m_instance_ids.end()) {
                m_instance_ids.erase(inst_id_it);

                return true;
            }

            return false;
        }

        [[nodiscard]] auto get_parent() noexcept -> Prototype<V>*;

        [[nodiscard]] auto is_modifiable() const noexcept -> bool {
            return m_modifiable;
        }
    };

    template <typename Value>
    class Object {
    private:
        std::flat_map<std::string, Value> m_props;
        std::shared_ptr<Prototype<Value>> m_parent_proto;
        std::string m_kind_name;

    public:
        Object(const std::string& kind_name, std::shared_ptr<Prototype<Value>> proto)
        : m_props {}, m_parent_proto {proto}, m_kind_name {kind_name} {
            for (const auto& proto_shape = m_parent_proto.get_properties(); const auto& [protop_name, protop_value] : proto_shape) {
                m_props[protop_name] = protop_value;
            }
        }

        [[nodiscard]] auto get_prototype() -> Prototype<Value>* {
            return m_parent_proto.get();
        }

        [[nodiscard]] auto get_property(const std::string& name) -> Value* {
            if (auto name_p = std::find(m_props.begin(), m_props.end(), name); name_p != m_props.end()) {
                return std::addressof(*name_p);
            }

            return nullptr;
        }

        void set_property(const std::string& name, std::same_as<Value> auto&& value) {
            m_props[name] = std::forward<Value>(value);
        }

        template <typename VM>
        [[nodiscard]] auto try_invoke(VM& vm, void* current_ctx) -> bool {
            return vm.invoke_bc_fn(current_ctx, this);
        }
    };

    struct JSNullOpt {};
    struct JSNaNOpt {};

    enum class ValueTag : uint8_t {
        undefined,
        null,
        boolean,
        num_nan,
        num_i32,
        num_f64,
        object,
    };

    class Value {
    public:
        static constexpr auto dud_undefined_char_v = '\x00';
        static constexpr auto dud_null_char_v = '\x10';
        static constexpr auto dud_nan_char_v = '\x01';

    private:
        union {
            char dud;
            bool b;
            int i;
            double d;
            Object<Value>* obj_p;
        } m_data;

        ValueTag m_tag;

    public:
        constexpr Value() noexcept
        : m_data {}, m_tag {ValueTag::undefined} {
            m_data.dud = dud_undefined_char_v;
        }

        constexpr Value([[maybe_unused]] JSNullOpt opt) noexcept
        : m_data {}, m_tag {ValueTag::null} {
            m_data.dud = dud_null_char_v;
        }

        constexpr Value([[maybe_unused]] JSNaNOpt opt) noexcept
        : m_data {}, m_tag {ValueTag::num_nan} {
            m_data.dud = dud_nan_char_v;
        }

        constexpr Value(bool b) noexcept
        : m_data {}, m_tag {ValueTag::boolean} {
            m_data.b = b;
        }

        constexpr Value(int i) noexcept
        : m_data {}, m_tag {ValueTag::num_i32} {
            m_data.i = i;
        }

        Value(double d) noexcept
        : m_data {}, m_tag {ValueTag::num_f64} {
            m_data.d = d;
        }

        constexpr Value(Object<Value>* object_p) noexcept
        : m_data {}, m_tag {ValueTag::object} {
            m_data.obj_p = object_p;
        }

        [[nodiscard]] constexpr auto get_tag() const noexcept -> ValueTag {
            return m_tag;
        }

        [[nodiscard]] constexpr auto is_truthy() const noexcept -> bool {
            if (m_tag == ValueTag::undefined || m_tag == ValueTag::null || m_tag == ValueTag::num_nan) {
                return false;
            } else if (m_tag == ValueTag::boolean) {
                return m_data.b;
            } else if (m_tag == ValueTag::num_i32) {
                return m_data.i != 0;
            } else if (m_tag == ValueTag::num_f64) {
                return m_data.d != 0.0;
            } else {
                return true;
            }
        }

        [[nodiscard]] constexpr auto is_falsy() const noexcept -> bool {
            return !is_truthy();
        }

        explicit constexpr operator bool(this auto&& self) noexcept {
            return self.is_truthy();
        }

        [[nodiscard]] constexpr auto is_nan() const noexcept -> bool {
            if (m_tag != ValueTag::num_nan) {
                return false;
            }

            return (m_data.dud & 0x1) == 1;
        }

        [[nodiscard]] auto operator==(const Value& other) const noexcept -> bool {
            if (const auto lhs_tag = get_tag(), rhs_tag = other.get_tag(); lhs_tag != rhs_tag) {
                return false;
            } else if (lhs_tag == ValueTag::undefined || lhs_tag == ValueTag::null) {
                return true;
            } else if (lhs_tag == ValueTag::num_i32) {
                return m_data.i == other.m_data.i;
            } else if (lhs_tag == ValueTag::num_f64) {
                return m_data.d == other.m_data.d;
            } else if (lhs_tag == ValueTag::boolean) {
                /// NOTE: As per https://262.ecma-international.org/5.1/#sec-11.9.6, the boolean values in this case must be both T / both F. This is actually the negation of a Boolean XOR.
                return (m_data.b ^ other.m_data.b) == 0;
            } else if (lhs_tag == ValueTag::object) {
                return m_data.obj_p == other.m_data.obj_p;
            } else {
                return false;
            }
        }

        /// NOTE: This partly implements the Abstract Relational Comparison logic from https://262.ecma-international.org/5.1/#sec-11.8.5, but it only implements case 3 for now.
        [[nodiscard]] auto operator<(const Value& other) const noexcept -> bool {
            // Case 3: convert both sides to Number values if possible. No String values supported yet.
            if (const auto lhs_tag = get_tag(); lhs_tag == ValueTag::num_f64) {
                return to_num_f64().value_or(0.0) < other.to_num_f64().value_or(0.0);
            } else {
                return to_num_i32().value_or(0) < other.to_num_i32().value_or(0);
            }
        }

        /// NOTE: see `DerkJS::Value::operator<()` documentation.
        [[nodiscard]] auto operator>(const Value& other) const noexcept -> bool {
            // Case 3: convert both sides to Number values if possible. No String values supported yet.
            if (const auto lhs_tag = get_tag(); lhs_tag == ValueTag::num_f64) {
                return to_num_f64().value_or(0.0) > other.to_num_f64().value_or(0.0);
            } else {
                return to_num_i32().value_or(0) > other.to_num_i32().value_or(0);
            }
        }

        [[nodiscard]] auto operator<=(const Value& other) const noexcept -> bool {
            return !this->operator>(other);
        }

        [[nodiscard]] auto operator>=(const Value& other) const noexcept -> bool {
            return !this->operator<(other);
        }

        /// NOTE: This is an ad-hoc implementation for now, so I'll fix this later by the ES5 spec.
        [[nodiscard]] auto operator%(const Value& other) -> Value {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                return Value {JSNaNOpt {}};
            } else if (lhs_tag == ValueTag::num_i32) {
                if (const auto rhs_i32_v = other.to_num_i32(); !rhs_i32_v) {
                    return Value {JSNaNOpt {}};
                } else if (*rhs_i32_v == 0) {
                    return Value {JSNaNOpt {}};
                } else {   
                    return Value {to_num_i32().value() % rhs_i32_v.value()};
                }
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] auto operator%=(const Value& other) -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32) {
                if (const auto rhs_i32_v = other.to_num_i32(); !rhs_i32_v) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else if (*rhs_i32_v == 0) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else { 
                    m_data.i %= rhs_i32_v.value();
                }
            } else {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            }

            return *this;
        }

        /// REF: https://262.ecma-international.org/5.1/#sec-11.5.1
        [[nodiscard]] auto operator*(const Value& other) -> Value {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                return Value {JSNaNOpt {}};
            } else if (lhs_tag == ValueTag::num_i32) {
                const auto lhs_i32 = to_num_i32();
                const auto rhs_i32 = other.to_num_i32();

                return Value {lhs_i32.value() * rhs_i32.value()};
            } else if (lhs_tag == ValueTag::num_f64) {
                const auto lhs_f64 = to_num_f64();
                const auto rhs_f64 = other.to_num_f64();

                return Value {lhs_f64.value() * rhs_f64.value()};
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] auto operator*=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32 && rhs_tag == lhs_tag) {
                m_data.i *= other.to_num_i32().value();
            } else if (lhs_tag == ValueTag::num_f64 && rhs_tag == lhs_tag) {
                m_data.i *= other.to_num_f64().value();
            } else {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            }

            return *this;
        }

        /// REF: https://262.ecma-international.org/5.1/#sec-11.5.2
        /// NOTE: Does not follow the `Infinity` portions of the algorithm for brevity.
        [[nodiscard]] auto operator/(const Value& other) -> Value {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                return Value {JSNaNOpt {}};
            } else if (lhs_tag == ValueTag::num_i32) {
                if (const auto rhs_i32_v = other.to_num_i32(); !rhs_i32_v) {
                    return Value {JSNaNOpt {}};
                } else if (*rhs_i32_v == 0) {
                    return Value {JSNaNOpt {}};
                } else {   
                    return Value {to_num_i32().value() / rhs_i32_v.value()};
                }
            } else if (lhs_tag == ValueTag::num_f64) {
                if (const auto rhs_f64_v = other.to_num_f64(); !rhs_f64_v) {
                    return Value {JSNaNOpt {}};
                } else if (const auto rhs_f64 = *rhs_f64_v; rhs_f64 == 0.0) {
                    return Value {JSNaNOpt {}};
                } else {   
                    return Value {to_num_f64().value() / rhs_f64};
                }
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] auto operator/=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32) {
                if (const auto rhs_i32_v = other.to_num_i32(); !rhs_i32_v) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else if (*rhs_i32_v == 0) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else {   
                    m_data.i /= other.to_num_i32().value();
                }
            } else if (lhs_tag == ValueTag::num_f64) {
                if (const auto rhs_f64_v = other.to_num_f64(); !rhs_f64_v) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else if (*rhs_f64_v == 0) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else {   
                    m_data.d /= other.to_num_f64().value();
                }
            } else {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            }

            return *this;
        }

        /// REF: https://262.ecma-international.org/5.1/#sec-11.6.1
        /// NOTE: Does not implement `String` or `Infinity` cases for brevity. Both operands will just be converted to numbers.
        [[nodiscard]] auto operator+(const Value& other) -> Value {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();
            
            if (lhs_tag == ValueTag::num_i32) {
                return Value {to_num_i32().value() + other.to_num_i32().value()};
            } else if (lhs_tag == ValueTag::num_f64) {
                return Value {to_num_f64().value() + other.to_num_f64().value()};
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] auto operator+=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();
            
            if (lhs_tag == ValueTag::num_i32) {
                m_data.i += other.to_num_i32().value();
            } else if (lhs_tag == ValueTag::num_f64) {
                m_data.d += other.to_num_f64().value();
            } else {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            }

            return *this;
        }

        /// REF: https://262.ecma-international.org/5.1/#sec-11.6.1
        /// NOTE: Does not implement `Infinity` cases for brevity.
        [[nodiscard]] auto operator-(const Value& other) -> Value {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();
            
            if (lhs_tag == ValueTag::num_i32) {
                return Value {to_num_i32().value() - other.to_num_i32().value()};
            } else if (lhs_tag == ValueTag::num_f64) {
                return Value {to_num_f64().value() - other.to_num_f64().value()};
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] auto operator-=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();
            
            if (lhs_tag == ValueTag::num_i32) {
                m_data.i -= other.to_num_i32().value();
            } else if (lhs_tag == ValueTag::num_f64) {
                m_data.d -= other.to_num_f64().value();
            } else {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            }

            return *this;
        }

        [[nodiscard]] auto to_boolean() const noexcept -> std::optional<bool> {
            return is_truthy();
        }

        [[nodiscard]] auto to_num_i32() const noexcept -> std::optional<int> {
            switch (m_tag) {
            case ValueTag::undefined: return {};
            case ValueTag::null: return 0;
            case ValueTag::boolean: return m_data.b ? 1 : 0 ;
            case ValueTag::num_i32: return m_data.i;
            case ValueTag::num_f64: return static_cast<int>(m_data.d);
            case ValueTag::object: // TODO: add obj_p->to_num_i32();
            default: return {};
            }
        }

        [[nodiscard]] auto to_num_f64() const noexcept -> std::optional<double> {
            switch (m_tag) {
            case ValueTag::undefined: return {};
            case ValueTag::null: return 0.0;
            case ValueTag::boolean: return m_data.b ? 1.0 : 0.0 ;
            case ValueTag::num_i32: return static_cast<double>(m_data.i);
            case ValueTag::num_f64: return m_data.d;
            case ValueTag::object: // TODO: add obj_p->to_num_f64();
            default: return {};
            }
        }

        [[nodiscard]] auto to_string() const noexcept -> std::optional<std::string> {
            switch (m_tag) {
            case ValueTag::undefined: return "undefined";
            case ValueTag::null: return "null";
            case ValueTag::boolean: return std::string { m_data.b ? "true" : "false" };
            case ValueTag::num_i32: return std::to_string(m_data.i);
            case ValueTag::num_f64: return std::to_string(m_data.d);
            case ValueTag::object: return "[Object object]";
            case ValueTag::num_nan: default: return "NaN";
            }
        }

        [[nodiscard]] auto to_object() const noexcept -> std::optional<Object<Value>*> {
            return nullptr;
        }
    };
}

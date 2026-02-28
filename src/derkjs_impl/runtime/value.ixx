module;

#include <cstddef>
#include <utility>
#include <optional>
#include <string>
#include <string_view>

export module runtime.value;

export import runtime.objects;

/// TODO: make Boolean, Number, Object, and Function as built-in objects registered to the interpreter before running.
export namespace DerkJS {
    struct Value;
    class Object;

    struct JSNullOpt {};
    struct JSNaNOpt {};
    struct JSProtoKeyOpt {};

    enum class ValueTag : uint8_t {
        undefined,
        null,
        boolean,
        num_nan,
        num_i32,
        num_f64,
        object,
        val_ref,
        proto_key
    };

    /// TODO: add support for Value holding a Value* as a reference.
    class Value {
    public:
        static constexpr auto dud_undefined_char_v = '\x00';
        static constexpr auto dud_null_char_v = '\x10';
        static constexpr auto dud_nan_char_v = '\x01';
        static constexpr auto dud_proto_key_v = '\xff';

    private:
        union {
            char dud;
            bool b;
            int i;
            double d;
            ObjectBase<Value>* obj_p;
            Value* ref_p;
        } m_data;

        ValueTag m_tag;
        uint8_t m_parent_flags; // flags from PropertyDescriptor if any

    public:
        constexpr Value(uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::undefined}, m_parent_flags {parent_flags} {
            m_data.dud = dud_undefined_char_v;
        }

        constexpr Value([[maybe_unused]] JSNullOpt opt, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::null}, m_parent_flags {parent_flags} {
            m_data.dud = dud_null_char_v;
        }

        constexpr Value([[maybe_unused]] JSNaNOpt opt, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::num_nan}, m_parent_flags {parent_flags} {
            m_data.dud = dud_nan_char_v;
        }

        constexpr Value([[maybe_unused]] JSProtoKeyOpt opt, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::proto_key}, m_parent_flags {parent_flags} {
            m_data.dud = dud_proto_key_v;
        }

        constexpr Value(bool b, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::boolean}, m_parent_flags {parent_flags} {
            m_data.b = b;
        }

        constexpr Value(int i, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::num_i32}, m_parent_flags {parent_flags} {
            m_data.i = i;
        }

        Value(double d, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::num_f64}, m_parent_flags {parent_flags} {
            m_data.d = d;
        }

        explicit constexpr Value([[maybe_unused]] decltype(nullptr) nullptr_v) noexcept
        : m_data {}, m_tag {ValueTag::object}, m_parent_flags {std::to_underlying(AttrMask::unused)} {
            m_data.obj_p = nullptr;
        }

        constexpr Value(ObjectBase<Value>* object_p, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::object}, m_parent_flags {parent_flags} {
            m_data.obj_p = object_p;
        }

        constexpr Value(Value* value_p, uint8_t parent_flags = std::to_underlying(AttrMask::unused)) noexcept
        : m_data {}, m_tag {ValueTag::val_ref}, m_parent_flags {parent_flags} {
            m_data.ref_p = value_p;
        }

        [[nodiscard]] constexpr auto get_tag() const noexcept -> ValueTag {
            return m_tag;
        }

        [[nodiscard]] constexpr auto is_valid_object_ref() const noexcept -> bool {
            if (m_tag == ValueTag::object) {
                return m_data.obj_p != nullptr;
            }

            return false;
        }

        [[nodiscard]] constexpr auto get_typename() const noexcept -> std::string_view {
            switch (m_tag) {
            case ValueTag::undefined: return "undefined";
            case ValueTag::null: return "object";
            case ValueTag::boolean: return "boolean";
            case ValueTag::num_nan: case ValueTag::num_i32: case ValueTag::num_f64: return "number";
            case ValueTag::val_ref: return m_data.ref_p->get_typename();
            case ValueTag::object: return m_data.obj_p->get_typename();
            default: return "unknown";
            }
        }

        constexpr auto is_assignable_ref() const noexcept -> bool {
            return m_tag == ValueTag::val_ref && get_parent_flag<AttrMask::writable>();
        }

        constexpr auto is_configurable_ref() const noexcept -> bool {
            return m_tag == ValueTag::val_ref && get_parent_flag<AttrMask::configurable>();
        }

        template <AttrMask M>
        [[nodiscard]] constexpr auto get_parent_flag(this auto&& self) noexcept -> bool {
            if constexpr (M == AttrMask::writable || M == AttrMask::unused) {
                return self.m_parent_flags & static_cast<uint8_t>(M);
            } else if constexpr (M == AttrMask::configurable) {
                return (self.m_parent_flags & static_cast<uint8_t>(M)) >> 1;
            } else if constexpr (M == AttrMask::enumerable) {
                return (self.m_parent_flags & static_cast<uint8_t>(M)) >> 2;
            } else if constexpr (M == AttrMask::is_data_desc) {
                return (self.m_parent_flags & static_cast<uint8_t>(M)) >> 3;
            } else if constexpr (M == AttrMask::is_parent_frozen) {
                return (self.m_parent_flags & static_cast<uint8_t>(M)) >> 4;
            }

            return self.m_parent_flags == 0x00;
        }

        [[nodiscard]] constexpr auto get_parent_flags(this auto&& self) noexcept -> uint8_t {
            return self.m_parent_flags;
        }

        constexpr void update_parent_flags(uint8_t parent_object_flags) noexcept {
            m_parent_flags = parent_object_flags;
        }

        [[nodiscard]] constexpr auto is_prototype_key() const noexcept -> bool {
            return m_tag == ValueTag::proto_key;
        }

        [[nodiscard]] constexpr auto is_reference() const noexcept -> bool {
            return m_tag == ValueTag::val_ref || m_tag == ValueTag::object;
        }

        [[nodiscard]] constexpr auto is_truthy() const noexcept -> bool {
            if (m_tag == ValueTag::undefined || m_tag == ValueTag::null || m_tag == ValueTag::num_nan) {
                return false;
            } else if (m_tag == ValueTag::boolean) {
                return m_data.b;
            } else if (m_tag == ValueTag::num_i32) {
                return m_data.i ^ 0;
            } else if (m_tag == ValueTag::num_f64) {
                return m_data.d != 0.0;
            } else if (m_tag == ValueTag::val_ref) {
                return (m_data.ref_p) ? m_data.ref_p->is_truthy() : false;
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
            return m_tag == ValueTag::num_nan;
        }

        [[nodiscard]] constexpr auto operator==(const Value& other) const noexcept -> bool {
            if (const auto lhs_tag = get_tag(), rhs_tag = other.get_tag(); lhs_tag != rhs_tag && !is_reference() && !other.is_reference()) {
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
                /// TODO: add object comparison algorithm!
                return m_data.obj_p->operator==(*other.m_data.obj_p);
            } else if (lhs_tag == ValueTag::val_ref) {
                return m_data.ref_p->operator==(other.deep_clone());
            } else {
                return false;
            }
        }

        /// NOTE: This partly implements the Abstract Relational Comparison logic from https://262.ecma-international.org/5.1/#sec-11.8.5, but it only implements case 3 for now.
        [[nodiscard]] constexpr auto operator<(const Value& other) const noexcept -> bool {
            // Case 3: convert both sides to Number values if possible. No String values supported yet.
            if (const auto lhs_tag = get_tag(); lhs_tag == ValueTag::num_f64) {
                return to_num_f64().value_or(0.0) < other.to_num_f64().value_or(0.0);
            } else if (m_tag == ValueTag::val_ref) {
                return m_data.ref_p->operator<(other);
            } else {
                return to_num_i32().value_or(0) < other.to_num_i32().value_or(0);
            }
        }

        /// NOTE: see `DerkJS::Value::operator<()` documentation.
        [[nodiscard]] constexpr auto operator>(const Value& other) const noexcept -> bool {
            // Case 3: convert both sides to Number values if possible. No String values supported yet.
            if (const auto lhs_tag = get_tag(); lhs_tag == ValueTag::num_f64) {
                return to_num_f64().value_or(0.0) > other.to_num_f64().value_or(0.0);
            } else if (m_tag == ValueTag::val_ref) {
                return m_data.ref_p->operator>(other);
            } else {
                return to_num_i32().value_or(0) > other.to_num_i32().value_or(0);
            }
        }

        [[nodiscard]] constexpr auto operator<=(const Value& other) const noexcept -> bool {
            return !this->operator>(other);
        }

        [[nodiscard]] constexpr auto operator>=(const Value& other) const noexcept -> bool {
            return !this->operator<(other);
        }

        [[maybe_unused]] auto increment() noexcept -> Value& {
            switch (m_tag) {
            case ValueTag::null:
                m_data.i = 1;
                m_tag = ValueTag::num_i32;
                break;
            case ValueTag::boolean:
                m_data.i = static_cast<int>(m_data.b) + 1;
                m_tag = ValueTag::num_i32;
                break;
            case ValueTag::num_i32:
                ++m_data.i;
                break;
            case ValueTag::num_f64:
                ++m_data.d;
                break;
            case ValueTag::object:
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
                break;
            case ValueTag::val_ref:
                return m_data.ref_p->increment();
            default:
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
                break;
            }

            return *this;
        }

        [[maybe_unused]] auto decrement() noexcept -> Value& {
            switch (m_tag) {
            case ValueTag::null:
                m_data.i = -1;
                m_tag = ValueTag::num_i32;
                break;
            case ValueTag::boolean:
                m_data.i = static_cast<int>(m_data.b) - 1;
                m_tag = ValueTag::num_i32;
                break;
            case ValueTag::num_i32:
                --m_data.i;
                break;
            case ValueTag::num_f64:
                --m_data.d;
                break;
            case ValueTag::object:
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
                break;
            case ValueTag::val_ref:
                return m_data.ref_p->decrement();
            default:
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
                break;
            }

            return *this;
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
            } else if (m_tag == ValueTag::val_ref && m_data.ref_p) {
                m_data.ref_p->operator%=(other);
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

        [[maybe_unused]] constexpr auto operator*=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32) {
                m_data.i *= other.to_num_i32().value();
            } else if (lhs_tag == ValueTag::num_f64) {
                m_data.d *= other.to_num_f64().value();
            } else if (m_tag == ValueTag::val_ref) {
                m_data.ref_p->operator*=(other);
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
                if (auto rhs_i32_v = other.to_num_i32(); !rhs_i32_v) {
                    return Value {JSNaNOpt {}};
                } else if (*rhs_i32_v == 0.0) {
                    return Value {JSNaNOpt {}};
                } else {   
                    return Value {*rhs_i32_v / rhs_i32_v.value()};
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

        [[maybe_unused]] constexpr auto operator/=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();

            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32) {
                if (auto rhs_i32_v = other.to_num_i32(); !rhs_i32_v) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else if (*rhs_i32_v == 0) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else {   
                    m_data.i /= rhs_i32_v.value();
                }
            } else if (lhs_tag == ValueTag::num_f64) {
                if (auto rhs_f64_v = other.to_num_f64(); !rhs_f64_v) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else if (*rhs_f64_v == 0.0) {
                    m_data.dud = dud_nan_char_v;
                    m_tag = ValueTag::num_nan;
                } else {
                    m_data.d /= rhs_f64_v.value();
                }
            } else if (m_tag == ValueTag::val_ref) {
                m_data.ref_p->operator/=(other);
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
            
            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                return Value {JSNaNOpt {}};
            } else if (lhs_tag == ValueTag::num_i32) {
                return Value {to_num_i32().value() + other.to_num_i32().value()};
            } else if (lhs_tag == ValueTag::num_f64) {
                return Value {to_num_f64().value() + other.to_num_f64().value()};
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] constexpr auto operator+=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();
            
            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32) {
                m_data.i += other.to_num_i32().value();
            } else if (lhs_tag == ValueTag::num_f64) {
                m_data.d += other.to_num_f64().value();
            } else if (lhs_tag == ValueTag::val_ref) {
                m_data.ref_p->operator+=(other);
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
            
            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                return Value {JSNaNOpt {}};
            } else if (lhs_tag == ValueTag::num_i32) {
                return Value {to_num_i32().value() - other.to_num_i32().value()};
            } else if (lhs_tag == ValueTag::num_f64) {
                return Value {to_num_f64().value() - other.to_num_f64().value()};
            } else {
                return Value {JSNaNOpt {}};
            }
        }

        [[maybe_unused]] constexpr auto operator-=(const Value& other) noexcept -> Value& {
            const auto lhs_tag = get_tag();
            const auto rhs_tag = other.get_tag();
            
            if (lhs_tag == ValueTag::num_nan || rhs_tag == ValueTag::num_nan) {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            } else if (lhs_tag == ValueTag::num_i32) {
                m_data.i -= other.to_num_i32().value();
            } else if (lhs_tag == ValueTag::num_f64) {
                m_data.d -= other.to_num_f64().value();
            } else if (m_tag == ValueTag::val_ref) {
                m_data.ref_p->operator-=(other);
            } else {
                m_data.dud = dud_nan_char_v;
                m_tag = ValueTag::num_nan;
            }

            return *this;
        }

        [[nodiscard]] constexpr auto to_boolean() const noexcept -> std::optional<bool> {
            return is_truthy();
        }

        [[nodiscard]] constexpr auto to_num_i32() const noexcept -> std::optional<int> {
            switch (m_tag) {
            case ValueTag::null: return 0;
            case ValueTag::boolean: return static_cast<int>(m_data.b);
            case ValueTag::num_i32: return m_data.i;
            case ValueTag::num_f64: return static_cast<int>(m_data.d);
            case ValueTag::object: return {};
            case ValueTag::val_ref: return m_data.ref_p->to_num_i32();
            default: return {};
            }
        }

        [[nodiscard]] auto to_num_f64() const noexcept -> std::optional<double> {
            switch (m_tag) {
            case ValueTag::null: return 0.0;
            case ValueTag::boolean: return m_data.b ? 1.0 : 0.0 ;
            case ValueTag::num_i32: return static_cast<double>(m_data.i);
            case ValueTag::num_f64: return m_data.d;
            case ValueTag::object: {
                std::string text = m_data.obj_p->as_string();

                if (text.empty()) {
                    return 0.0;
                }

                try {
                    return std::stod(text);
                } catch (const std::exception& e) {
                    return {};
                }
            };
            case ValueTag::val_ref: return m_data.ref_p->to_num_f64();
            default: return {};
            }
        }

        [[nodiscard]] auto to_string() const noexcept -> std::string {
            switch (m_tag) {
            case ValueTag::undefined: return "undefined";
            case ValueTag::null: return "null";
            case ValueTag::boolean: return std::string { m_data.b ? "true" : "false" };
            case ValueTag::num_nan: default: return "NaN";
            case ValueTag::num_i32: return std::to_string(m_data.i);
            case ValueTag::num_f64: return std::to_string(m_data.d);
            case ValueTag::object: return m_data.obj_p->as_string();
            case ValueTag::val_ref: return m_data.ref_p->to_string();
            }
        }

        [[nodiscard]] auto to_object() noexcept -> ObjectBase<Value>* {
            if (m_tag == ValueTag::object) {
                return m_data.obj_p;
            } else if (m_tag == ValueTag::val_ref) {
                return m_data.ref_p->to_object();
            }

            return nullptr;
        }

        [[nodiscard]] auto get_value_ref() noexcept -> Value* {
            if (m_tag == ValueTag::val_ref) {
                return m_data.ref_p;
            }

            return nullptr;
        }

        [[nodiscard]] auto deep_clone() const -> Value {
            switch (m_tag) {
            // copy object reference for avoiding ownership problems
            case ValueTag::object: return m_data.obj_p;
            case ValueTag::val_ref: return m_data.ref_p->deep_clone();
            case ValueTag::undefined:
            case ValueTag::null:
            case ValueTag::boolean:
            case ValueTag::num_nan: 
            case ValueTag::num_i32:
            case ValueTag::num_f64:
            default:
                return *this;
            }
        }
    };

    class Object : public ObjectBase<Value> {
    private:
        PropPool<Value, Value> m_own_properties;
        Value m_prototype;
        uint8_t m_flags;

    public:
        /// NOTE: Creates mutable instances of anonymous objects. Pass the `flag_prototype_v | flag_extensible_v` if needed for Foo.prototype!
        Object(ObjectBase<Value>* proto_p, uint8_t flags = std::to_underlying(AttrMask::unused))
        : m_own_properties {}, m_prototype {(proto_p) ? Value {proto_p} : Value {}}, m_flags {flags} {
            m_prototype.update_parent_flags(m_flags);
        }

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "object";
        }

        [[nodiscard]] auto get_typename() const noexcept -> std::string_view override {
            return "object";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return m_flags & flag_extensible_v;
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return (m_flags & flag_prototype_v) >> 7;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_prototype.to_object();
        }

        [[nodiscard]] auto get_instance_prototype() noexcept -> ObjectBase<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_seq_items() noexcept -> std::vector<Value>* override {
            return nullptr;
        }

        auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_properties;
        }

        [[nodiscard]] auto get_property_value(const Value& key, bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                /// TODO: fix this to not get __proto__- instead get a new m_instance_prototype field.
                return PropertyDescriptor<Value> {&key, &m_prototype, this, m_flags};
            } else if (auto property_entry_it = std::find_if(m_own_properties.begin(), m_own_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key;
            }); property_entry_it != m_own_properties.end()) {
                return PropertyDescriptor<Value> {&key, &property_entry_it->item, this, static_cast<uint8_t>(m_flags & property_entry_it->flags)};
            } else if ((m_flags & std::to_underlying(AttrMask::writable)) && allow_filler) {
                return PropertyDescriptor<Value> {
                    &key,
                    &m_own_properties.emplace_back(
                        key, Value {},
                        static_cast<uint8_t>(m_flags & std::to_underlying(AttrMask::unused))
                    ).item,
                    this,
                    m_flags
                };
            } else if (auto prototype_p = m_prototype.to_object(); prototype_p) {
                return prototype_p->get_property_value(key, allow_filler);
            }

            return PropertyDescriptor<Value> {};
        }

        void freeze() noexcept override {
            m_flags = std::to_underlying(AttrMask::is_parent_frozen);

            for (auto& entry : m_own_properties) {
                entry.item.update_parent_flags(m_flags);
            }

            m_prototype.update_parent_flags(m_flags);

            if (auto prototype_object_p = m_prototype.to_object(); prototype_object_p) {
                prototype_object_p->freeze();
            }
        }

        [[maybe_unused]] auto set_property_value(const Value& key, const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);

            return &(property_desc = value);
        }

        [[maybe_unused]] auto del_property_value([[maybe_unused]] const Value& key) -> bool override {
            return false; // TODO
        }

        void update_on_accessor_mut([[maybe_unused]] Value* accessor_value_p, [[maybe_unused]] const Value& value) override {}

        [[nodiscard]] auto call([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc, [[maybe_unused]] bool has_this_arg) -> bool override {
            return false;
        }

        [[nodiscard]] auto call_as_ctor([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }

        /// NOTE: Due to the Value repr needing a non-owning but mutable ObjectBase<Value>* pointer to something managed by a heap cell's unique ptr, having a raw pointer is unavoidable. This may not be so bad since the PolyPool<ObjectBase<Value>> in the VM can quickly manage it once it arrives via `PolyPool<ObjectBase<V>>::add_item()`.
        [[nodiscard]] auto clone() -> ObjectBase<Value>* override {
            auto self_clone = new Object {m_prototype.to_object()};

            self_clone->get_own_prop_pool() = m_own_properties;

            return self_clone;
        }

        [[nodiscard]] auto as_string() const -> std::string override {
            return "[object Object]";
        }

        [[nodiscard]] auto opaque_iter() const noexcept -> OpaqueIterator override {
            return OpaqueIterator {};
        }

        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            if (this == &other) {
                return true;
            }

            return false;
        }

        [[nodiscard]] auto operator<(const ObjectBase& other) const noexcept -> bool override {
            if (get_class_name() != other.get_class_name()) {
                return false;
            }

            // TODO

            return true;
        }

        [[nodiscard]] auto operator>(const ObjectBase& other) const noexcept -> bool override {
            if (get_class_name() != other.get_class_name()) {
                return false;
            }

            // TODO

            return true;
        }
    };
}

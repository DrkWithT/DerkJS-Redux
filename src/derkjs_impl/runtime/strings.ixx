module;

#include <utility>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <format>

export module runtime.strings;

import runtime.objects;
import runtime.value;

export namespace DerkJS {
    /// TODO: implement dynamic strings later!
    class StaticString : public ObjectBase<Value>, public StringBase {
    public:
        static constexpr auto max_length_v = 8;
        static constexpr auto flag_extensible_v = 0b00000001;
        static constexpr auto flag_frozen_v = 0b00000010;
        static constexpr auto flag_prototype_v = 0b10000000;

    private:
        PropPool<PropertyHandle<Value>, Value> m_own_props;
        ObjectBase<Value>* m_proto;
        char m_data[max_length_v];
        uint8_t m_length;
        uint8_t m_flags;

        [[nodiscard]] constexpr auto is_full() const noexcept -> bool {
            return m_length >= max_length_v;
        }

    public:
        constexpr StaticString(ObjectBase<Value>* proto_ref, const char* cstr, int length) noexcept
        : m_data {}, m_length (std::min(length, max_length_v)), m_flags {0x00} {
            std::copy_n(cstr, length, m_data);
        }

        //// BEGIN ObjectBase overrides

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "string";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return m_flags & flag_extensible_v;
        }

        [[nodiscard]] auto is_frozen() const noexcept -> bool override {
            return (m_flags & flag_frozen_v) >> 1;
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return (m_flags & flag_prototype_v) >> 7;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_proto;
        }

        auto get_own_prop_pool() const noexcept -> const PropPool<PropertyHandle<Value>, Value>& override {
            return m_own_props;
        }

        [[nodiscard]] auto get_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> Value* override {
            return nullptr;
        }

        [[maybe_unused]] auto set_property_value([[maybe_unused]] const PropertyHandle<Value>& handle, const Value& value) -> Value* override {
            return nullptr;
        }

        [[maybe_unused]] auto del_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> bool override {
            return false;
        }

        [[nodiscard]] auto call([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }

        [[nodiscard]] auto call_as_ctor([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }

        [[nodiscard]] auto clone() const -> ObjectBase<Value>* override {
            // Due to the Value repr needing an ObjectBase<Value>* ptr, the Value clone method returns a Value vs. `std::unique_ptr<Value>`, and the VM only stores Value-s on its stack, having a raw pointer is unavoidable. This may not be so bad since the PolyPool<ObjectBase<Value>> in the VM can quickly own it via `add_item()`.
            auto self_clone = new StaticString {m_proto, m_data, m_length};

            /// TODO: Add properties like .length as needed.

            return self_clone;
        }

        [[nodiscard]] auto as_string() const -> std::string override {
            std::string_view data_slice {m_data, m_data + m_length};

            return std::format("{}", data_slice);
        }

        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            return this == &other || as_str_view() == other.as_string();
        }

        [[nodiscard]] auto operator<(const ObjectBase& other) const noexcept -> bool override {
            if (get_class_name() != other.get_class_name()) {
                return false;
            }

            return as_str_view() < other.as_string();
        }

        [[nodiscard]] auto operator>(const ObjectBase& other) const noexcept -> bool override {
            if (get_class_name() != other.get_class_name()) {
                return false;
            }

            return as_str_view() > other.as_string();
        }


        //// BEGIN StringBase overrides

        auto is_empty() const noexcept -> bool override {
            return m_length < 1;
        }

        auto get_slice([[maybe_unused]] int start, [[maybe_unused]] int length) -> std::unique_ptr<StringBase> override {
            return {}; // todo
        }

        auto get_length() const noexcept -> int override {
            return m_length;
        }

        void append_front(const std::string& s) override {
            ; // todo
        }

        void append_back(const std::string& s) override {
            ; // todo
        }

        /// NOTE: This is for String.prototype.indexOf()
        auto find_substr_pos([[maybe_unused]] const StringBase* other_view) const noexcept -> int override {
            return -1; // todo
        }

        [[nodiscard]] auto as_str_view() const noexcept -> std::string_view override {
            return std::string_view {m_data, static_cast<std::size_t>(m_length)};
        }
    };


    /// NOTE: create special copy, move, and destructor ops since std::unique_ptr may recursively release in the destructor.
    class DynamicString : public ObjectBase<Value>, public StringBase {
    public:
        static constexpr auto flag_extensible_v = 0b00000001;
        static constexpr auto flag_frozen_v = 0b00000010;
        static constexpr auto flag_prototype_v = 0b10000000;

    private:
        PropPool<PropertyHandle<Value>, Value> m_own_props;
        std::string m_data;
        ObjectBase<Value>* m_proto;
        uint8_t m_flags;

    public:
        DynamicString(std::string s)
        : m_own_props {}, m_data (std::move(s)), m_proto {nullptr}, m_flags {0x00} {}

        /// BEGIN ObjectBase overrides

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "string";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto is_frozen() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() const noexcept -> const PropPool<PropertyHandle<Value>, Value>& override {
            return m_own_props;
        }

        [[nodiscard]] auto get_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> Value* override {
            return nullptr;
        }

        [[nodiscard]] auto set_property_value([[maybe_unused]] const PropertyHandle<Value>& handle, const Value& value) -> Value* override {
            return nullptr;
        }

        [[nodiscard]] auto del_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> bool override {
            return false;
        }

        [[maybe_unused]] auto call([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }

        [[nodiscard]] auto call_as_ctor([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        [[nodiscard]] auto clone() const -> ObjectBase<Value>* override {
            auto cloned_self = new DynamicString {m_data};

            return cloned_self;
        }

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        [[nodiscard]] auto as_string() const -> std::string override {
            return m_data;
        }

        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            return this == &other || m_data == other.as_string();
        }

        [[nodiscard]] auto operator<(const ObjectBase& other) const noexcept -> bool override {
            return m_data < other.as_string();
        }

        [[nodiscard]] auto operator>(const ObjectBase& other) const noexcept -> bool override {
            return m_data > other.as_string();
        }

        /// BEGIN StringBase overrides

        [[nodiscard]] auto is_empty() const noexcept -> bool override {
            return m_data.empty();
        }

        [[nodiscard]] auto get_slice(int start, int length) -> std::unique_ptr<StringBase> override {
            return std::make_unique<DynamicString>(m_data.substr(start, length));
        }

        [[nodiscard]] auto get_length() const noexcept -> int override {
            return m_data.size();
        }

        void append_front(const std::string& s) override {
            std::string old_data = std::move(m_data);
            m_data = s;
            m_data.append_range(old_data);
        }

        void append_back(const std::string& s) override {
            m_data.append_range(s);
        }

        /// NOTE: This is for String.prototype.indexOf()
        [[nodiscard]] auto find_substr_pos([[maybe_unused]] const StringBase* other_view) const noexcept -> int override {
            return -1; // TODO: implement!
        }

        [[nodiscard]] auto as_str_view() const noexcept -> std::string_view override {
            return std::string_view {m_data.c_str(), m_data.c_str() + m_data.length()};
        }
    };
}
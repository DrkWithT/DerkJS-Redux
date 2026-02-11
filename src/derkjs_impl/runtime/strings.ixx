module;

#include <utility>
#include <memory>
#include <vector>
#include <string>
#include <string_view>

export module runtime.strings;

import runtime.value;

export namespace DerkJS {
    /// NOTE: create special copy, move, and destructor ops since std::unique_ptr may recursively release in the destructor.
    class DynamicString : public ObjectBase<Value>, public StringBase {
    public:
        static constexpr auto flag_extensible_v = 0b00000001;
        static constexpr auto flag_frozen_v = 0b00000010;
        static constexpr auto flag_prototype_v = 0b10000000;

    private:
        PropPool<Value, Value> m_own_props;
        std::string m_data;
        Value m_prototype;
        uint8_t m_flags;

    public:
        DynamicString(ObjectBase<Value>* prototype_p, const std::string& s)
        : m_own_props {}, m_data {s}, m_prototype {prototype_p}, m_flags {0x00} {}

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

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_prototype.to_object();
        }

        [[nodiscard]] auto get_seq_items() noexcept -> std::vector<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_props;
        }

        [[nodiscard]] auto get_property_value([[maybe_unused]] const Value& key, [[maybe_unused]] bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {&key, &m_proto};
            } else if (auto property_entry_it = std::find_if(m_own_props.begin(), m_own_props.end(), [&key](const auto& descriptor) -> bool {
                return descriptor.get_key() == key;
            }); property_entry_it != m_own_props.end()) {
                return PropertyDescriptor<Value> {*property_entry_it};
            } else if (allow_filler) {
                return PropertyDescriptor<Value> {
                    m_own_props.emplace_back(
                        key, Value {},
                        static_cast<uint8_t>(AttrMask::writable) | static_cast<uint8_t>(AttrMask::configurable)
                    )
                };
            } else if (auto prototype_p = m_proto.to_object(); prototype_p) {
                return prototype_p->get_property_value(key, allow_filler);
            }

            return PropertyDescriptor<Value> {};
        }

        void freeze() noexcept override {
            m_flags |= flag_frozen_v;
        }

        [[nodiscard]] auto set_property_value([[maybe_unused]] const Value& key, const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, m_flags != flag_frozen_v);

            return &(property_desc = value);
        }

        [[nodiscard]] auto del_property_value([[maybe_unused]] const Value& key) -> bool override {
            return false;
        }

        [[maybe_unused]] auto call([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc, [[maybe_unused]] bool has_this_arg) -> bool override {
            return false;
        }

        [[nodiscard]] auto call_as_ctor([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        [[nodiscard]] auto clone() -> ObjectBase<Value>* override {
            auto cloned_self = new DynamicString {m_prototype.to_object(), m_data};

            return cloned_self;
        }

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        [[nodiscard]] auto as_string() const -> std::string override {
            return m_data;
        }

        [[nodiscard]] auto opaque_iter() const noexcept -> OpaqueIterator override {
            return OpaqueIterator {reinterpret_cast<const std::byte*>(m_data.data()), m_data.size() * sizeof(decltype(*m_data.data()))};
        }

        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            if (this == &other) {
                return true;
            }

            OpaqueIterator self_it {reinterpret_cast<const std::byte*>(m_data.data()), m_data.length() * sizeof(decltype(*m_data.data()))};
            OpaqueIterator other_it = other.opaque_iter();

            if (self_it.count() != other_it.count()) {
                return false;
            }

            while (self_it.alive()) {
                if (other_it != self_it) {
                    return false;
                }

                ++self_it;
                ++other_it;
            }

            return true;
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
            return std::make_unique<DynamicString>(m_prototype.to_object(), m_data.substr(start, length));
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
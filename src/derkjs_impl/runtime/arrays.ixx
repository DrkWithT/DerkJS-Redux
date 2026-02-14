module;

#include <type_traits>
#include <utility>
#include <string>
#include <vector>
#include <sstream>

export module runtime.arrays;

import runtime.value;

export namespace DerkJS {
    class Array : public ObjectBase<Value> {
    private:
        // Holds non-integral key properties.
        PropPool<Value, Value> m_own_properties;
        // Stores properties (items) accessed by integral keys. These are the sequential items. Sparse arrays can also be implemented via pushing `Value {}` repeatedly.
        std::vector<Value> m_items;
        // Holds a prototype reference.
        Value m_prototype;
        uint8_t m_flags;

        void fill_gap_to_n(int index, bool should_default) {
            int item_count = m_items.size();

            if (!should_default || (m_flags & std::to_underlying(AttrMask::writable)) == 0) {
                return;
            }

            while (item_count <= index) {
                m_items.emplace_back(Value {});
                ++item_count;
            }
        }

        [[nodiscard]] auto get_item(int index, bool should_default) -> Value* {
            int item_count = m_items.size();

            if (!should_default && (index < 0 || index >= item_count)) {
                return nullptr;
            }

            fill_gap_to_n(index, should_default);

            return &m_items.at(index);
        }

        [[nodiscard]] auto put_item(int index, const Value& value) -> Value* {
            fill_gap_to_n(index, true);
            m_items[index] = value;

            return &m_items[index];
        }
    public:
        Array(ObjectBase<Value>* prototype_p) noexcept (std::is_nothrow_default_constructible_v<Value>)
        : m_own_properties {}, m_items {}, m_prototype {prototype_p}, m_flags {std::to_underlying(AttrMask::unused)} {
            m_prototype.update_parent_flags(m_flags);
        }

        [[nodiscard]] auto items() noexcept -> std::vector<Value>& {
            return m_items;
        }

        [[nodiscard]] auto items() const noexcept -> const std::vector<Value>& {
            return m_items;
        }

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "Array";
        }

        [[nodiscard]] auto get_typename() const noexcept -> std::string_view override {
            return "object";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return true;
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_prototype.to_object();
        }

        [[nodiscard]] auto get_seq_items() noexcept -> std::vector<Value>* override {
            return &m_items;
        }

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_properties;
        }

        [[maybe_unused]] auto get_property_value([[maybe_unused]] const Value& key, [[maybe_unused]] bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {&key, &m_prototype, m_flags};
            } else if (key.get_tag() == ValueTag::num_i32) {
                return PropertyDescriptor<Value> {&key, get_item(key.to_num_i32().value(), true), m_flags};
            } else if (auto property_entry_it = std::find_if(m_own_properties.begin(), m_own_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key;
            }); property_entry_it != m_own_properties.end()) {
                return PropertyDescriptor<Value> {&key, &property_entry_it->item, m_flags};
            } else if ((m_flags & std::to_underlying(AttrMask::writable)) && !m_prototype && allow_filler) {
                return PropertyDescriptor<Value> {
                    &key,
                    &m_own_properties.emplace_back(
                        key, Value {},
                        static_cast<uint8_t>(AttrMask::writable) | static_cast<uint8_t>(AttrMask::configurable)
                    ).item,
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

        [[maybe_unused]] auto set_property_value([[maybe_unused]] const Value& key, [[maybe_unused]] const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);

            return &(property_desc = value);
        }

        [[maybe_unused]] auto del_property_value([[maybe_unused]] const Value& key) -> bool override {
            return false; // TODO
        }

        [[nodiscard]] auto call([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc, [[maybe_unused]] bool has_this_arg) -> bool override {
            return false;
        }

        [[nodiscard]] auto call_as_ctor([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            return false;
        }


        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        [[nodiscard]] auto clone() -> ObjectBase<Value>* override {
            return new Array {*this};
        }

        [[nodiscard]] auto opaque_iter() const noexcept -> OpaqueIterator override {
            return OpaqueIterator {reinterpret_cast<const std::byte*>(m_items.data()), m_items.size() * sizeof(decltype(*m_items.data()))};
        }

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        [[nodiscard]] auto as_string() const -> std::string override {
            std::ostringstream sout {};

            if (!m_items.empty()) {
                for (int pending_items = m_items.size(); const auto& temp_item : m_items) {
                    sout << temp_item.to_string().value();
                    --pending_items;
                    
                    if (pending_items <= 0) {
                        break;
                    }
                    
                    sout << ',';
                }
            } else {
                sout << "[Array (empty)]";
            }

            return sout.str();
        }

        /// TODO: Add ordered Object / Array iterators via `get_property_iterator(PropSearchPolicy p)` for this, avoiding a dynamic_cast.
        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            if (&other == this) {
                return true;
            }

            auto self_iter = OpaqueIterator {reinterpret_cast<const std::byte*>(m_items.data()), m_items.size() * sizeof(decltype(*m_items.data()))};
            auto other_iter = other.opaque_iter();

            if (self_iter.count() != other_iter.count()) {
                return false;
            }

            while (self_iter.alive()) {
                if (self_iter != other_iter) {
                    return false;
                }

                ++self_iter;
                ++other_iter;
            }

            return true;
        }

        [[nodiscard]] auto operator<(const ObjectBase& other) const noexcept -> bool override {
            if (other.get_class_name() != "Array") {
                return false;
            }

            const auto& other_as_array = dynamic_cast<const Array&>(other);
            const std::vector<Value>& self_items = m_items;
            const std::vector<Value>& other_items = other_as_array.m_items;

            return self_items.size() < other_items.size();
        }

        [[nodiscard]] auto operator>(const ObjectBase& other) const noexcept -> bool override {
            if (other.get_class_name() != "Array") {
                return false;
            }

            const auto& other_as_array = dynamic_cast<const Array&>(other);
            const std::vector<Value>& self_items = m_items;
            const std::vector<Value>& other_items = other_as_array.m_items;

            return self_items.size() > other_items.size();
        }
    };
}
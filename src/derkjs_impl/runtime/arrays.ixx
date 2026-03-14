module;

#include <type_traits>
#include <utility>
#include <string>
#include <span>
#include <vector>
#include <sstream>

export module runtime.arrays;

import runtime.value;

namespace DerkJS {
    auto handle_length_change(ObjectBase<Value>* object_p, const Value& next_length) -> bool;

    export class Array : public ObjectBase<Value> {
    private:
        // Holds non-integral key properties.
        PropPool<Value, Value> m_own_properties;
        // Stores properties (items) accessed by integral keys. These are the sequential items. Sparse arrays can also be implemented via pushing `Value {}` repeatedly.
        std::vector<Value> m_items;
        // Holds [[Prototype]] reference.
        Value m_prototype;
        uint8_t m_flags;

        void fill_gap_to_n(int index, bool should_default) {
            if (!should_default || (m_flags & std::to_underlying(AttrMask::writable)) == 0) {
                return;
            }

            m_items.resize(index + 1, Value {});
        }

        [[nodiscard]] auto get_item(int index, bool should_default) -> Value* {
            int item_count = m_items.size();

            if (index >= 0 && index < item_count) {
                return &m_items.at(index);
            } else if (
                should_default && (m_flags & std::to_underlying(AttrMask::writable)) == std::to_underlying(AttrMask::writable)
            ) {
                fill_gap_to_n(index, should_default);
                return &m_items.at(index);
            }

            return nullptr;
        }

    public:
        Array(ObjectBase<Value>* prototype_p, const Value& length_key, const Value& initial_length_v) noexcept (std::is_nothrow_default_constructible_v<Value>)
        : m_own_properties {}, m_items {}, m_prototype {prototype_p}, m_flags {std::to_underlying(AttrMask::defaults)} {
            const auto length_prop_flags = static_cast<uint8_t>(std::to_underlying(AttrMask::accessor) | m_flags);

            m_prototype.update_flags(m_flags);
            m_own_properties.emplace_back(PropEntry<Value, Value> {
                .key = length_key,
                .item = initial_length_v,
                .handler_p = nullptr
            });
        }

        Array(ObjectBase<Value>* prototype_p, const Value& length_key, const std::span<Value>& item_slice) noexcept (std::is_nothrow_default_constructible_v<Value>)
        : m_own_properties {}, m_items {}, m_prototype {prototype_p}, m_flags {std::to_underlying(AttrMask::defaults)} {
            const auto length_prop_flags = static_cast<uint8_t>(std::to_underlying(AttrMask::accessor) | m_flags);

            m_prototype.update_flags(m_flags);

            auto& length_value_ref = m_own_properties.emplace_back(PropEntry<Value, Value> {
                .key = length_key,
                .item = Value {0},
                .handler_p = handle_length_change
            }).item;
            auto item_count = 0;

            for (const auto& item_value : item_slice) {
                set_property_value(Value {item_count}, item_value);
                item_count++;
            }

            length_value_ref = Value {item_count};
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

        [[nodiscard]] auto flag(AttrMask flag_mask) const noexcept -> bool override {
            return (m_flags & std::to_underlying(flag_mask)) == std::to_underlying(flag_mask);
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_prototype.to_object();
        }

        [[nodiscard]] auto get_instance_prototype() noexcept -> ObjectBase<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_seq_items() noexcept -> std::vector<Value>* override {
            return &m_items;
        }

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_properties;
        }

        [[maybe_unused]] auto get_property_value(const Value& key, [[maybe_unused]] bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {key, &m_prototype, this}; // TODO: add instance-specific prototype support?
            } else if (key.get_tag() == ValueTag::num_i32) {
                return PropertyDescriptor<Value> {key, get_item(key.to_num_i32().value(), allow_filler), this};
            } else if (auto property_entry_it = std::find_if(m_own_properties.begin(), m_own_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key || prop.key.compare_as_object(key);
            }); property_entry_it != m_own_properties.end()) {
                return PropertyDescriptor<Value> {key, &property_entry_it->item, this};
            } else if ((m_flags & std::to_underlying(AttrMask::writable)) && allow_filler) {
                return PropertyDescriptor<Value> {
                    key,
                    &m_own_properties.emplace_back(
                        key, Value {}, nullptr
                    ).item,
                    this
                };
            } else if (auto prototype_p = m_prototype.to_object(); prototype_p) {
                return prototype_p->get_property_value(key, allow_filler);
            }

            return PropertyDescriptor<Value> {};
        }

        void freeze() noexcept override {
            m_flags = std::to_underlying(AttrMask::frozen);

            for (auto& entry : m_own_properties) {
                entry.item.update_flags(m_flags);
            }

            m_prototype.update_flags(m_flags);

            if (auto prototype_object_p = m_prototype.to_object(); prototype_object_p) {
                prototype_object_p->freeze();
            }
        }

        [[maybe_unused]] auto set_property_value([[maybe_unused]] const Value& key, [[maybe_unused]] const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);

            return (property_desc.set_value(key, value))
                ? property_desc.ref_value()
                : nullptr;
        }

        [[maybe_unused]] auto del_property_value([[maybe_unused]] const Value& key) -> bool override {
            return false; // TODO
        }

        //! TODO: I should not have this silently fail... Return a bool at least.
        void update_on_accessor_mut([[maybe_unused]] const Value& key, const Value& value) override {
            if (auto property_entry_it = std::find_if(m_own_properties.begin(), m_own_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key || prop.key.compare_as_object(key);
            }); property_entry_it != m_own_properties.end() && property_entry_it->handler_p != nullptr) {
                property_entry_it->handler_p(this, value);
            }
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
                    sout << temp_item.to_string();
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

        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            if (&other == this) {
                return true;
            }

            auto other_as_array = dynamic_cast<const Array*>(&other);

            if (!other_as_array) {
                return false;
            } else if (const auto& other_items = other_as_array->items(); m_items.size() != other_items.size()) {
                return false;
            } else {
                for (int parallel_pos = 0; const auto& other_item : other_items) {
                    if (other_item != m_items.at(parallel_pos)) {
                        return false;
                    }

                    ++parallel_pos;
                }
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

    [[nodiscard]] auto handle_length_change(ObjectBase<Value>* object_p, const Value& next_length) -> bool {
        if (auto object_as_array_p = dynamic_cast<Array*>(object_p); object_as_array_p) {
            const int next_length_i32 = next_length.to_num_i32().value_or(0);

            if (const int old_length = object_as_array_p->get_seq_items()->size(); next_length_i32 > old_length) {
                object_as_array_p->get_seq_items()->resize(next_length_i32 + 1, Value {});
            } else if (next_length_i32 < old_length) {
                object_as_array_p->get_seq_items()->resize(next_length_i32);
            }

            return true;
        }

        return false;
    }
}
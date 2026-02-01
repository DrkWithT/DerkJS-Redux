module;

#include <type_traits>
#include <utility>
#include <string>
#include <vector>
#include <map>
#include <sstream>

export module runtime.arrays;

import runtime.objects;
import runtime.value;

export namespace DerkJS {
    class Array : public ObjectBase<Value> {
    private:
        // Holds non-integral key properties.
        PropPool<PropertyHandle<Value>, Value> m_props;
        // Stores properties (items) accessed by integral keys. These are the sequential items. Sparse arrays can also be implemented via pushing `Value {}` repeatedly.
        std::vector<Value> m_items;
        // Holds a prototype reference.
        Value m_prototype;

        void fill_gap_to_n(int index, bool should_default) {
            int item_count = m_items.size();

            if (!should_default) {
                return;
            }

            while (item_count <= index) {
                m_items.emplace_back(Value {});
                ++item_count;
            }
        }

        [[nodiscard]] auto get_item(int index, bool should_default) -> Value* {
            int item_count = m_items.size();

            fill_gap_to_n(index, should_default);

            if (!should_default && (index < 0 || index >= item_count)) {
                return nullptr;
            }

            return &m_items.at(index);
        }

        [[nodiscard]] auto put_item(int index, const Value& value) -> Value* {
            fill_gap_to_n(index, true);
            m_items[index] = value;

            return &m_items[index];
        }
    public:
        Array(ObjectBase<Value>* prototype_p) noexcept (std::is_nothrow_default_constructible_v<Value>)
        : m_props {}, m_items {}, m_prototype {prototype_p} {}

        [[nodiscard]] decltype(auto) items(this auto&& self) noexcept {
            return self.m_items;
        }

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "Array";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return true;
        }

        [[nodiscard]] auto is_frozen() const noexcept -> bool override {
            return false;
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

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<PropertyHandle<Value>, Value>& override {
            return m_props;
        }

        [[nodiscard]] auto get_property_value([[maybe_unused]] const PropertyHandle<Value>& handle, [[maybe_unused]] bool allow_filler) -> Value* override {
            if (handle.is_proto_key()) {
                return &m_prototype;
            } else if (Value key = handle.get_key_value().deep_clone(); key.get_tag() == ValueTag::num_i32) {
                return get_item(key.to_num_i32().value(), allow_filler);
            } else if (auto property_entry_it = std::find_if(m_props.begin(), m_props.end(), [&handle](const auto& prop_pair) -> bool {
                return prop_pair.first == handle;
            }); property_entry_it != m_props.end()) {
                return &property_entry_it->second;
            } else if (auto prototype_p = m_prototype.to_object(); prototype_p) {
                return prototype_p->get_property_value(handle, allow_filler);
            }

            return nullptr;
        }

        [[maybe_unused]] auto set_property_value([[maybe_unused]] const PropertyHandle<Value>& handle, [[maybe_unused]] const Value& value) -> Value* override {
            if (handle.is_proto_key()) {
                m_prototype = value;
                return &m_prototype;
            } else if (Value key = handle.get_key_value().deep_clone(); key.get_tag() == ValueTag::num_i32) {
                return put_item(key.to_num_i32().value(), value);
            } else if (auto old_prop_it = std::find_if(m_props.begin(), m_props.end(), [&handle](const auto& prop_pair) -> bool {
                return prop_pair.first == handle;
            }); old_prop_it != m_props.end()) {
                old_prop_it->second = value;
                return &old_prop_it->second;
            } else {
                m_props.emplace_back(handle, value);
                return &m_props.back().second;
            }
        }

        [[maybe_unused]] auto del_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> bool override {
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
        /// TODO: Add non-numeric property testing.
        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            if (other.get_class_name() != "Array") {
                return false;
            }

            const auto& other_as_array = dynamic_cast<const Array&>(other);
            const std::vector<Value>& self_items = m_items;
            const std::vector<Value>& other_items = other_as_array.m_items;

            if (int self_size = self_items.size(); self_size == other_items.size()) {
                for (int check_index = 0; check_index < self_size; ++check_index) {
                    if (self_items.at(check_index) != other_items.at(check_index)) {
                        return false;
                    }
                }

                return true;
            }

            return false;
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
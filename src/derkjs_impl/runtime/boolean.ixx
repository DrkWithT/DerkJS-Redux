module;

#include <utility>
#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

export module runtime.boolean;

import runtime.value;

namespace DerkJS {
    export class BooleanBox : public ObjectBase<Value> {
    private:
        PropPool<Value, Value> m_properties;
        Value m_prototype;
        Value m_value;
        uint8_t m_flags;

    public:
        BooleanBox(bool b, ObjectBase<Value>* prototype_p)
        : m_properties {}, m_prototype {prototype_p, std::to_underlying(AttrMask::defaults) | std::to_underlying(AttrMask::property)}, m_value {b}, m_flags {std::to_underlying(AttrMask::defaults)} {}

        /// Begin self methods ///

        [[nodiscard]] auto get_native_value() const noexcept -> const Value& {
            return m_value;
        }

        [[nodiscard]] auto get_native_value() noexcept -> Value& {
            return m_value;
        }

        [[nodiscard]] auto to_str_view(this auto&& self) -> std::string_view {
            return (self.m_value) ? "true" : "false";
        }

        /// Begin ObjectBase<Value> overrides ///

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "Boolean";
        }

        [[nodiscard]] auto get_typename() const noexcept -> std::string_view override {
            return "Boolean";
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
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_properties;
        }

        [[nodiscard]] auto get_property_value(const Value& key, bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {key, &m_prototype, this};
            } else if (auto property_entry_it = std::find_if(m_properties.begin(), m_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key || prop.key.compare_as_object(key);
            }); property_entry_it != m_properties.end()) {
                return PropertyDescriptor<Value> {key, &property_entry_it->item, this};
            } else if ((m_flags & std::to_underlying(AttrMask::writable)) && allow_filler) {
                return PropertyDescriptor<Value> {
                    key,
                    &m_properties.emplace_back(
                        key, Value {JSUndefOpt {}, std::to_underlying(AttrMask::defaults) | std::to_underlying(AttrMask::property)}, nullptr
                    ).item,
                    this
                };
            } else if (auto prototype_p = m_prototype.to_object(); prototype_p) {
                return prototype_p->get_property_value(key, allow_filler);
            }

            return PropertyDescriptor<Value> {};
        }

        void freeze() noexcept override {
            m_flags = std::to_underlying(AttrMask::frozen_property);

            for (auto& entry : m_properties) {
                entry.item.update_flags(m_flags);
            }

            m_prototype.update_flags(m_flags);

            if (auto prototype_object_p = m_prototype.to_object(); prototype_object_p) {
                prototype_object_p->freeze();
            }
        }

        [[nodiscard]] auto set_property_value(const Value& key, const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);
            auto value_copy = value;

            value_copy.set_flag<AttrMask::property>();

            return (property_desc.set_value(key, value_copy))
                ? property_desc.ref_value()
                : nullptr;
        }

        void update_on_accessor_mut([[maybe_unused]] const Value& accessor_value_p, const Value& value) override {}

        [[nodiscard]] auto call(void* opaque_ctx_p, int argc, bool has_this_arg) -> bool override {
            return false;
        }

        [[nodiscard]] auto call_as_ctor(void* opaque_ctx_p, int argc) -> bool override {
            return false;
        }

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        [[nodiscard]] auto clone() -> ObjectBase<Value>* override {
            return new BooleanBox {*this};
        }

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        [[nodiscard]] auto as_string() const -> std::string override {
            return (m_value) ? "true" : "false";
        }

        // [[nodiscard]] auto field_iter() noexcept -> FieldIterator<PropertyDescriptor<V>> override {
        // ;}

        [[nodiscard]] auto opaque_iter() const noexcept -> OpaqueIterator override {
            return {};
        }

        [[nodiscard]] auto operator==(const ObjectBase<Value>& other) const noexcept -> bool override {
            if (&other == this) {
                return true;
            }

            if (const auto other_as_bool_box_p = dynamic_cast<const BooleanBox*>(&other); other_as_bool_box_p) {
                return m_value == other_as_bool_box_p->get_native_value();
            }

            return false;
        }

        [[nodiscard]] auto operator<(const ObjectBase<Value>& other) const noexcept -> bool override {
            if (&other == this) {
                return false;
            }

            if (const auto other_as_bool_box_p = dynamic_cast<const BooleanBox*>(&other); other_as_bool_box_p) {
                return m_value < other_as_bool_box_p->get_native_value();
            }

            return false;
        }

        [[nodiscard]] auto operator>(const ObjectBase<Value>& other) const noexcept -> bool override {
            if (&other == this) {
                return false;
            }

            if (const auto other_as_bool_box_p = dynamic_cast<const BooleanBox*>(&other); other_as_bool_box_p) {
                return m_value > other_as_bool_box_p->get_native_value();
            }

            return false;
        }
    };
}
module;

#include <utility>
#include <algorithm>
#include <vector>
#include <string>
#include <string_view>

export module runtime.object;

import runtime.value;

namespace DerkJS {
    export class Object : public ObjectBase<Value> {
    private:
        PropPool<Value, Value> m_own_properties;
        Value m_prototype;
        uint8_t m_flags;

    public:
        /// NOTE: Creates mutable instances of anonymous objects. Pass the `flag_prototype_v | flag_extensible_v` if needed for Foo.prototype!
        Object(ObjectBase<Value>* proto_p, uint8_t flags = std::to_underlying(AttrMask::defaults))
        : m_own_properties {}, m_prototype {proto_p, std::to_underlying(AttrMask::defaults) | std::to_underlying(AttrMask::configurable)}, m_flags {flags} {
            m_prototype.update_flags(m_flags);
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

        auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_properties;
        }

        [[nodiscard]] auto get_property_value(const Value& key, bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {key, &m_prototype, this};
            } else if (auto property_entry_it = std::find_if(m_own_properties.begin(), m_own_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key || prop.key.compare_as_object(key);
            }); property_entry_it != m_own_properties.end()) {
                return PropertyDescriptor<Value> {key, &property_entry_it->item, this};
            } else if ((m_flags & std::to_underlying(AttrMask::writable)) && allow_filler) {
                return PropertyDescriptor<Value> {
                    key,
                    &m_own_properties.emplace_back(
                        key,
                        Value {
                            JSUndefOpt {}, std::to_underlying(AttrMask::defaults) | std::to_underlying(AttrMask::configurable)
                        },
                        nullptr
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

            for (auto& entry : m_own_properties) {
                entry.item.update_flags(m_flags);
            }

            m_prototype.update_flags(m_flags);

            if (auto prototype_object_p = m_prototype.to_object(); prototype_object_p) {
                prototype_object_p->freeze();
            }
        }

        [[maybe_unused]] auto set_property_value(const Value& key, const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);
            auto value_copy = value;

            value_copy.set_flag<AttrMask::property>();

            return (property_desc.set_value(key, value_copy))
                ? property_desc.ref_value()
                : nullptr;
        }

        void update_on_accessor_mut([[maybe_unused]] const Value& accessor_value_p, [[maybe_unused]] const Value& value) override {}

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
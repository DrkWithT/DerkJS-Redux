module;

#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory>
#include <vector>
#include <string>
#include <string_view>

export module runtime.objects;

import meta.enums;

export namespace DerkJS {
    /// NOTE: forward declaration of JS object interface.
    template <typename V>
    class ObjectBase;

    template <typename V>
    using AccessorHandler = bool(*)(ObjectBase<V>*, const V&);

    template <typename K, typename V>
    struct PropEntry {
        K key;
        V item;
        AccessorHandler<V> handler_p;
    };

    /**
     * @brief Alias for any object's property pool. Note that a prototype object has its own pool that instances refer to.
     * 
     * @tparam K property key as Value
     * @tparam V property item as Value
     */
    template <typename K, typename V>
    using PropPool = std::vector<PropEntry<K, V>>;

    enum class AttrMask : uint8_t {
        frozen = 0x00,
        writable = 0x01,
        configurable = 0x02,
        enumerable = 0x04,
        accessor = 0x08,
        property = 0x10,
        frozen_property = frozen | property,
        defaults = writable | configurable
    };

    /**
     * @brief Implementation of the data JS property descriptor needed for proper member access semantics as per https://262.ecma-international.org/5.1/#sec-8.10. However, plain old data descriptors handle local value references for variables.
     * @tparam V See `DerkJS::Value` in `./src/derkjs_impl/runtime/value.ixx`.
     */
    template <typename V>
    class PropertyDescriptor {
    private:
        static constexpr auto dud_item = V {};

        V m_key;                // copy of the object key
        V m_value;              // Value wrapping a pointer to property's Value
        ObjectBase<V>* m_base;  // pointer of parent object holding property

        [[nodiscard]] constexpr auto has_item(this auto&& self) noexcept -> bool {
            return self.m_base != nullptr;
        }

    public:
        //? NOTE: Initializes a dud property descriptor, usually from an invalid property name.
        explicit constexpr PropertyDescriptor() noexcept
        : m_key {JSUndefOpt {}}, m_value {JSUndefOpt {}}, m_base {nullptr} {}

        constexpr PropertyDescriptor(const V& key, const V& value, ObjectBase<V>* self_p) noexcept
        : m_key {key}, m_value {value}, m_base {self_p} {}

        explicit constexpr operator bool(this auto&& self) noexcept {
            return self.has_item();
        }

        [[nodiscard]] constexpr auto get_key(this auto&& self) noexcept -> V {
            return self.m_key;
        }

        [[nodiscard]] constexpr auto get_base_object() const noexcept -> const ObjectBase<V>* {
            return m_base;
        }

        [[nodiscard]] constexpr auto get_base_object_mut() noexcept -> ObjectBase<V>* {
            return m_base;
        }

        //? NOTE: retrieves the property by cloned value. This is different vs. ref_value().
        [[nodiscard]] constexpr auto get_value(this auto&& self) -> V {
            return (self.has_item())
                ? self.m_value
                : V {};
        }

        //? NOTE: tries updating the referenced property value, returning true on success.
        [[nodiscard]] constexpr auto set_value(const V& key, const V& arg) -> bool {
            if (!has_item()) {
                return false;
            } else if (m_value.template flag<AttrMask::writable>()) {
                *m_value.get_value_ref() = arg;

                if (m_value.template flag<AttrMask::accessor>()) {
                    //? NOTE: only accessors may require any side effects done. 
                    m_base->update_on_accessor_mut(key, arg);
                }

                return true;
            }

            return false;
        }

        //? NOTE: attempts to get the property value by pointer since JS references behave akin to pointers.
        [[nodiscard]] constexpr auto ref_value() -> V* {
            return m_value.get_value_ref();
        }
    };

    /**
     * @brief For comparing bytes of opaque data. Useful for JS String comparisons??
     */
    class OpaqueIterator {
    private:
        using byte_ptr = const std::byte*;

        byte_ptr m_current;
        byte_ptr m_end;

    public:
        constexpr OpaqueIterator() noexcept
        : m_current {nullptr}, m_end {nullptr} {}

        explicit constexpr OpaqueIterator(byte_ptr start, std::size_t end_offset) noexcept
        : m_current {start}, m_end {start + end_offset} {}

        constexpr auto alive() const noexcept -> bool {
            return m_current != m_end;
        }

        constexpr auto count() const noexcept -> std::size_t {
            return m_end - m_current;
        }

        constexpr auto operator==(const OpaqueIterator& other) const noexcept -> bool {
            return *m_current == *other.m_current;
        }

        [[maybe_unused]] auto operator++() noexcept -> OpaqueIterator& {
            if (alive()) {
                m_current++;
            }

            return *this;
        }
    };

    /// NOTE: indexes into an array of `ObjectBase<Value>` pointers, each for a JS built-in.
    enum class BuiltInObjects : uint8_t {
        /// Prototypes (TODO: refactor these out in favor of polyfilled ctors and intrinsics, etc.)
        boolean,
        number,
        str,
        object,
        array,
        function,

        /// Extra property names for quick access (TODO: refactor these out in favor of polyfilled ctors and intrinsics, etc.)
        extra_length_key, // Not a prototype, but I have to make "length" easily accessible for array creations.
        extra_msg_key, // Not a prototype, but patched in for `ErrorXYZ`.
        extra_name_key, // Not a prototype either, but patched in for `ErrorXYZ`.

        /// Built-In error ctors here...
        error_ctor,
        syntax_error_ctor,
        type_error_ctor,
        last
    };

    /**
     * @brief This virtual base class is an interface for all "objects" in DerkJS. Concrete sub-types from `ObjectBase` include `Object`s. Though all objects have a "template" object with the default values to properties of their type-structure- The prototype! For now, let's assume instances are clones of the prototype's "template".
     */
    template <typename V>
    class ObjectBase {
    public:
        virtual ~ObjectBase() = default;

        virtual auto get_unique_addr() noexcept -> void* = 0;
        virtual auto get_class_name() const noexcept -> std::string = 0;
        virtual auto get_typename() const noexcept -> std::string_view = 0;
        virtual auto flag(AttrMask flag_mask) const noexcept -> bool = 0;

        virtual auto get_prototype() noexcept -> ObjectBase<V>* = 0;

        virtual auto get_instance_prototype() noexcept -> ObjectBase<V>* = 0;

        virtual auto get_seq_items() noexcept -> std::vector<V>* = 0;
        virtual auto get_own_prop_pool() noexcept -> PropPool<V, V>& = 0;
        virtual auto get_property_value(const V& key, bool allow_filler) -> PropertyDescriptor<V> = 0;

        virtual void freeze() noexcept = 0;

        virtual auto set_property_value(const V& key, const V& value) -> V* = 0;
        virtual void update_on_accessor_mut(const V& accessor_value_p, const V& value) = 0;

        virtual auto call(void* opaque_ctx_p, int argc, bool has_this_arg) -> bool = 0;
        virtual auto call_as_ctor(void* opaque_ctx_p, int argc) -> bool = 0;

        //? For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        virtual auto clone() -> ObjectBase<V>* = 0;

        //? For default printing of objects, etc. to the console (stdout). 
        virtual auto as_string() const -> std::string = 0;

        /// virtual auto field_iter() noexcept -> FieldIterator<PropertyDescriptor<V>> = 0;
        virtual auto opaque_iter() const noexcept -> OpaqueIterator = 0;

        virtual auto operator==(const ObjectBase& other) const noexcept -> bool = 0;
        virtual auto operator<(const ObjectBase& other) const noexcept -> bool = 0;
        virtual auto operator>(const ObjectBase& other) const noexcept -> bool = 0;
    };

    /**
     * @brief This virtual base class abstracts away internal string details. This just wraps any specific string for object property accesses if necessary, and the internal contents can be a tiny char[12] with automatic storage duration OR a native C++ std::string.
     */
    class StringBase {
    public:
        virtual ~StringBase() = default;

        virtual auto is_empty() const noexcept -> bool = 0;
        virtual auto get_length() const noexcept -> int = 0;
        virtual void append_front(const std::string& s) = 0;
        virtual void append_back(const std::string& s) = 0;

        /// NOTE: This is for String.prototype.indexOf()
        virtual auto find_substr_pos(const StringBase* other_view) const noexcept -> int = 0;

        virtual auto as_str_view() const noexcept -> std::string_view = 0;
    };

    template <typename ItemBase> requires (std::is_polymorphic_v<ItemBase>)
    class PolyPool {
    public:
        // ObjectOverhead ~= sizeof(decltype(PropPool<V, V>::at(0))) + sizeof(Value) + <possible-metadata> = 32 + 16 + 8;
        static constexpr std::size_t object_overhead = 32UL + 24UL + 16UL + 8UL;
        static constexpr int max_id = 32767;

    private:
        std::vector<std::unique_ptr<ItemBase>> m_items;
        std::vector<int> m_free_slots;
        std::size_t m_overhead;
        int m_next_id;
        int m_id_max;
        int m_last_tenured_id; // mark the end of preloaded native objects, etc.

    public:
        PolyPool()
        : m_items {}, m_free_slots {}, m_overhead {0UL}, m_next_id {0}, m_id_max {0}, m_last_tenured_id {-1} {}

        PolyPool(int capacity)
        : m_items {}, m_free_slots {}, m_overhead {0UL}, m_next_id {0}, m_id_max {capacity}, m_last_tenured_id {-1} {
            m_items.reserve(capacity);
            m_items.resize(capacity);
        }

        [[nodiscard]] auto get_overhead() const noexcept -> std::size_t {
            return m_overhead;
        }
    
        [[nodiscard]] auto view_items() const noexcept -> const std::vector<std::unique_ptr<ItemBase>>& {
            return m_items;
        }

        [[nodiscard]] auto items() noexcept -> const std::vector<std::unique_ptr<ItemBase>>& {
            return m_items;
        }

        void tenure_items() noexcept {
            m_last_tenured_id = m_items.size() - 1;
        }

        [[nodiscard]] auto get_next_id() noexcept -> int {
            if (m_free_slots.empty()) {
                return (m_next_id < m_id_max) ? m_next_id : -1;
            }

            const int recycled_id = m_free_slots.back();
            m_free_slots.pop_back();

            return recycled_id;
        }

        [[nodiscard]] auto get_newest_item() noexcept -> ItemBase* {
            if (m_next_id > 0) {
                return m_items[m_next_id - 1].get();
            }

            return nullptr;
        }

        [[nodiscard]] auto get_item(int id) noexcept -> ItemBase* {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return nullptr;
            }

            return m_items[id].get();
        }

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, std::remove_cvref_t<ItemKind>>)
        [[maybe_unused]] auto update_item(int id, ItemKind&& item) -> bool {
            using item_kind_type = std::remove_cvref_t<ItemKind>;

            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            m_items[id] = std::make_unique<item_kind_type>(std::forward<item_kind_type>(item));

            return true;
        }

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, ItemKind>)
        [[maybe_unused]] auto update_item(int id, ItemKind* item_p) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            m_items[id] = std::unique_ptr<ItemKind>(item_p);

            return true;
        }

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, std::remove_cvref_t<ItemKind>>)
        [[maybe_unused]] auto add_item(int id, ItemKind&& item) -> ItemBase* {
            using item_kind_type = std::remove_cvref_t<ItemKind>;

            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return nullptr;
            }

            int slot_id;

            if (!m_free_slots.empty()) {
                slot_id = m_free_slots.back();
                m_free_slots.pop_back();
            } else {
                slot_id = m_next_id;
                ++m_next_id;
            }

            m_items[slot_id] = std::make_unique<item_kind_type>(std::forward<item_kind_type>(item));
            m_overhead += object_overhead;

            return m_items[slot_id].get();
        }

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, ItemKind>)
        [[nodiscard]] auto add_item(int id, ItemKind* item_p) -> ItemBase* {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return nullptr;
            }

            int slot_id;

            if (!m_free_slots.empty()) {
                slot_id = m_free_slots.back();
                m_free_slots.pop_back();
            } else {
                slot_id = m_next_id;
                ++m_next_id;
            }

            m_items[slot_id] = std::unique_ptr<ItemKind>(item_p);
            m_overhead += object_overhead;

            return m_items[slot_id].get();
        }

        /// NOTE: This overload only exists for the `Driver` to insert property values within insertion of native objects. Please see `./src/derkjs_impl/core/driver.ixx ~ line:138`!
        [[maybe_unused]] auto add_item(int id, std::unique_ptr<ItemBase> item_sp) -> ItemBase* {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return nullptr;
            }

            int slot_id;

            if (!m_free_slots.empty()) {
                slot_id = m_free_slots.back();
                m_free_slots.pop_back();
            } else {
                slot_id = m_next_id;
                ++m_next_id;
            }

            m_items[slot_id] = std::move(item_sp);
            m_overhead += object_overhead;

            return m_items[slot_id].get();
        }

        [[maybe_unused]] auto remove_item(int id) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size()) || id <= m_last_tenured_id) {
                return false;
            }

            m_items[id] = {};
            m_free_slots.emplace_back(id);
            m_overhead -= object_overhead;

            return true;
        }
    };
}

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
    template <typename K, typename V>
    struct PropEntry {
        K key;
        V item;
        uint8_t flags;
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
        dead = 0x00,
        writable = 0b00000001,
        configurable = 0b00000010,
        enumerable = 0b00000100,
        is_data_desc = 0b00001000,
        is_parent_frozen = 0b00010000,
        unused = 0b00000011
    };

    /**
     * @brief Implementation of the data JS property descriptor needed for proper member access semantics as per https://262.ecma-international.org/5.1/#sec-8.10.
     * @note No accessor semantics are implemented for simplicity.
     * @tparam V See `DerkJS::Value` in `./src/derkjs_impl/runtime/value.ixx`.
     */
    template <typename V>
    class PropertyDescriptor {
    private:
        static constexpr auto dud_item = V {};

        const V* key_p; // Views the PropPool entry's key member.
        V* item_p;      // Refers to the property value.
        uint8_t flags;  // By the ES5 spec, this determines how properties allow some operations, but they're taken from the parent object if so.

        [[nodiscard]] constexpr auto has_item(this auto&& self) noexcept -> bool {
            return self.flags != std::to_underlying(AttrMask::dead);
        }

    public:
        /// NOTE: For dud property descriptors, likely from an invalid property name.
        explicit constexpr PropertyDescriptor() noexcept
        : key_p {nullptr}, item_p {nullptr}, flags {std::to_underlying(AttrMask::dead)} {}

        constexpr PropertyDescriptor(const V* key_p_, V* item_p_, uint8_t parent_object_flags) noexcept
        : key_p {key_p_}, item_p {item_p_}, flags {parent_object_flags} {
            if (!item_p) {
                flags = std::to_underlying(AttrMask::dead);
            }
        }

        [[nodiscard]] constexpr auto get_key() const noexcept -> const V& {
            return *key_p;
        }

        explicit constexpr operator bool(this auto&& self) noexcept {
            return self.has_item();
        }

        [[nodiscard]] constexpr auto operator&() noexcept -> V* {
            return item_p;
        }

        [[nodiscard]] constexpr auto operator*() const noexcept -> V {
            if (!has_item()) {
                return dud_item;
            }

            return V {item_p, flags};
        }

        /**
         * @brief Very roughly follows the ES5 specification, 8.12.1:
         * @note The assignment to the referenced V (Value) only works if the property was present AND it's marked as writable.
         * @param value 
         * @return PropertyDescriptor& 
         */
        [[nodiscard]] auto operator=(const V& value) noexcept -> PropertyDescriptor& {
            if (get_flag<AttrMask::writable>()) {
                *item_p = value;
            }

            return *this;
        }

        template <AttrMask M>
        [[nodiscard]] constexpr auto get_flag() const noexcept -> bool {
            if constexpr (M == AttrMask::writable || M == AttrMask::unused) {
                return flags & static_cast<uint8_t>(M);
            } else if constexpr (M == AttrMask::configurable) {
                return (flags & static_cast<uint8_t>(M)) >> 1;
            } else if constexpr (M == AttrMask::enumerable) {
                return (flags & static_cast<uint8_t>(M)) >> 2;
            } else if constexpr (M == AttrMask::is_data_desc) {
                return (flags & static_cast<uint8_t>(M)) >> 3;
            } else if constexpr (M == AttrMask::is_parent_frozen) {
                return (flags & static_cast<uint8_t>(M)) >> 4;
            }

            return flags == 0x00;
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

    /**
     * @brief This virtual base class is an interface for all "objects" in DerkJS. Concrete sub-types from `ObjectBase` include `Object`s. Though all objects have a "template" object with the default values to properties of their type-structure- The prototype! For now, let's assume instances are clones of the prototype's "template".
     */
    template <typename V>
    class ObjectBase {
    public:
        static constexpr auto flag_extensible_v = 0b00000001;
        static constexpr auto flag_frozen_v = 0b00000010;
        static constexpr auto flag_prototype_v = 0b10000000;

        virtual ~ObjectBase() = default;

        virtual auto get_unique_addr() noexcept -> void* = 0;
        virtual auto get_class_name() const noexcept -> std::string = 0;
        virtual auto is_extensible() const noexcept -> bool = 0;
        virtual auto is_prototype() const noexcept -> bool = 0;

        virtual auto get_prototype() noexcept -> ObjectBase<V>* = 0;
        virtual auto get_seq_items() noexcept -> std::vector<V>* = 0;
        virtual auto get_own_prop_pool() noexcept -> PropPool<V, V>& = 0;
        virtual auto get_property_value(const V& key, bool allow_filler) -> PropertyDescriptor<V> = 0;

        virtual void freeze() noexcept = 0;

        virtual auto set_property_value(const V& key, const V& value) -> V* = 0;
        virtual auto del_property_value(const V& key) -> bool = 0;

        virtual auto call(void* opaque_ctx_p, int argc, bool has_this_arg) -> bool = 0;
        virtual auto call_as_ctor(void* opaque_ctx_p, int argc) -> bool = 0;

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        virtual auto clone() -> ObjectBase<V>* = 0;

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        virtual auto as_string() const -> std::string = 0;

        // virtual auto field_iter() noexcept -> FieldIterator<PropertyDescriptor<V>> = 0;
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
        virtual auto get_slice(int start, int length) -> std::unique_ptr<StringBase> = 0;
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

        void update_tenure_count() {
            ++m_last_tenured_id;
        }

        [[nodiscard]] auto get_next_id() noexcept -> int {
            if (m_free_slots.empty()) {
                return (m_next_id < m_id_max) ? m_next_id : -1;
            }

            const int recycled_id = m_free_slots.back();
            m_free_slots.pop_back();

            return recycled_id;
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

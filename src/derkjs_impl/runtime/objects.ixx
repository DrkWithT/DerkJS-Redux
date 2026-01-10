module;

#include <cstdint>
#include <type_traits>
#include <utility>
#include <memory>

#include <vector>
#include <map>
#include <string>
#include <string_view>

export module runtime.objects;

export namespace DerkJS {
    template <typename ValueType>
    struct PropertyHandle;

    /**
     * @brief Alias for any object's property pool. Note that a prototype object has its own pool that instances duplicate their structure from.
     * 
     * @tparam V This parameter must be a `Value` wrapper from `derkjs_impl/runtime/value.ixx`.
     */
    template <typename H, typename V>
    using PropPool = std::map<H, V>;

    /**
     * @brief This virtual base class is an interface for all "objects" in DerkJS. Concrete sub-types from `ObjectBase` include `Object`s. Though all objects have a "template" object with the default values to properties of their type-structure- The prototype! For now, let's assume instances are clones of the prototype's "template".
     */
    template <typename V>
    class ObjectBase {
    public:
        virtual ~ObjectBase() = default;

        virtual auto get_unique_addr() noexcept -> void* = 0;
        virtual auto get_class_name() const noexcept -> std::string = 0;
        virtual auto is_extensible() const noexcept -> bool = 0;
        virtual auto is_frozen() const noexcept -> bool = 0;
        virtual auto is_prototype() const noexcept -> bool = 0;

        virtual auto get_prototype() noexcept -> ObjectBase<V>* = 0;
        virtual auto get_own_prop_pool() const noexcept -> const PropPool<PropertyHandle<V>, V>& = 0;
        virtual auto get_property_value(const PropertyHandle<V>& handle) -> V* = 0;
        virtual auto set_property_value(const PropertyHandle<V>& handle, const V& value) -> V* = 0;
        virtual auto del_property_value(const PropertyHandle<V>& handle) -> bool = 0;
        // virtual auto get_property_handle() -> PropertyHandle = 0;

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        virtual auto clone() const -> ObjectBase<V>* = 0;

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        virtual auto as_string() const -> std::string = 0;

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
        virtual void append_front(const StringBase* other_view) = 0;
        virtual void append_back(const StringBase* other_view) = 0;

        /// NOTE: This is for String.prototype.indexOf()
        virtual auto find_substr_pos(const StringBase* other_view) const noexcept -> int = 0;

        virtual auto operator==(const StringBase& other) const noexcept -> bool = 0;
        virtual auto operator<(const StringBase& other) const noexcept -> bool = 0;
        virtual auto operator>(const StringBase& other) const noexcept -> bool = 0;

        virtual auto as_str_view() const noexcept -> std::string_view = 0;
    };

    template <typename ItemBase> requires (std::is_polymorphic_v<ItemBase>)
    class PolyPool {
    public:
        static constexpr int max_id = 32767;

    private:
        std::vector<std::unique_ptr<ItemBase>> m_items;
        std::vector<int> m_free_slots;

    public:
        PolyPool(int capacity)
        : m_items(capacity), m_free_slots {} {}
    
        [[nodiscard]] auto view_items() const noexcept -> const std::vector<std::unique_ptr<ItemBase>>& {
            return m_items;
        }

        [[nodiscard]] auto get_next_id() noexcept -> int {
            if (m_free_slots.empty()) {
                return m_items.size();
            }

            const int recycled_id = m_free_slots.size();
            m_free_slots.pop_back();

            return recycled_id;
        }

        [[nodiscard]] auto get_item(int id) noexcept -> ItemBase* {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return nullptr;
            }

            return m_items[id].get();
        }

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, ItemKind>)
        [[maybe_unused]] auto update_item(int id, ItemKind&& item) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            m_items[id] = std::make_unique<ItemKind>(std::forward<ItemKind>(item));

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

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, ItemKind>)
        [[nodiscard]] auto add_item(int id, ItemKind&& item) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            int slot_id;

            if (!m_free_slots.empty()) {
                slot_id = m_free_slots.back();
                m_free_slots.pop_back();
            } else {
                slot_id = m_items.size();
            }

            m_items[slot_id] = std::make_unique<ItemKind>(std::forward<ItemKind>(item));

            return true;
        }

        template <typename ItemKind> requires (std::is_base_of_v<ItemBase, ItemKind>)
        [[nodiscard]] auto add_item(int id, ItemKind* item_p) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            int slot_id;

            if (!m_free_slots.empty()) {
                slot_id = m_free_slots.back();
                m_free_slots.pop_back();
            } else {
                slot_id = m_items.size();
            }

            m_items[slot_id] = std::unique_ptr<ItemKind>(item_p);

            return true;
        }

        [[nodiscard]] auto remove_item(int id) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            m_items[id] = {};
            m_free_slots.emplace_back(id);

            return true;
        }
    };

    enum class PropertyHandleTag : uint8_t {
        nil, // dud handle
        key, // string ID indexing into string pool -> compare indexed strings in access
        index, // number -> an Array's own properties -> int to Value
    };

    enum class PropertyHandleFlag : uint8_t {
        configurable = 0b00000001,
        writable = 0b00000010,
        enumerable = 0b00000100
    };

    template <typename T>
    concept ComparableKeyKind = requires (T lhs, T rhs) {
        {auto(lhs == rhs)} -> std::same_as<bool>;
        {auto(lhs < rhs)} -> std::same_as<bool>;
        {auto(lhs > rhs)} -> std::same_as<bool>;
    };

    template <typename T, typename ValueT>
    concept JSObjectKind = std::is_base_of_v<ObjectBase<ValueT>, T>;

    template <typename KeyType>
    struct PropertyHandle {
        void* parent_obj_p; // must refer to the enclosing object's address in memory: helps distinguish properties between different object shapes / types with matching names / IDs
        KeyType* opaque_key_p; // MUST be a `Value*`, but this is templated to avoid circular type referencing
        PropertyHandleTag tag; // discriminator for whether the property is an index (for own props. in Arrays) vs. a string (for any Object property)
        uint8_t flags;

        constexpr PropertyHandle() noexcept
        : parent_obj_p {}, opaque_key_p {}, tag {PropertyHandleTag::nil}, flags {0x00} {}

        constexpr PropertyHandle(void* parent_obj_p_, KeyType* key_value_p, PropertyHandleTag tag_, uint8_t flags_) noexcept
        : parent_obj_p {parent_obj_p_}, opaque_key_p {key_value_p}, tag {tag_}, flags {flags_} {}

        explicit constexpr operator bool(this auto&& self) noexcept {
            return self.parent_obj_p_ != nullptr && self.opaque_key_p != nullptr && self.tag != PropertyHandleTag::nil;
        }

        template <PropertyHandleFlag Phf>
        [[nodiscard]] constexpr auto get_flag_of() const noexcept {
            if constexpr (Phf == PropertyHandleFlag::configurable || Phf == PropertyHandleFlag::writable || Phf == PropertyHandleFlag::enumerable) {
                return flags & static_cast<uint8_t>(Phf);
            }

            return false;
        }

        [[nodiscard]] constexpr auto operator==(const PropertyHandle& other) const noexcept -> bool {
            return parent_obj_p == other.parent_obj_p && *opaque_key_p == *other.opaque_key_p;
        }

        [[nodiscard]] constexpr auto operator<(const PropertyHandle& other) const noexcept -> bool {
            return parent_obj_p < other.parent_obj_p && *opaque_key_p < *other.opaque_key_p;
        }

        [[nodiscard]] constexpr auto operator>(const PropertyHandle& other) const noexcept -> bool {
            return parent_obj_p > other.parent_obj_p && *opaque_key_p > *other.opaque_key_p;
        }
    };
}

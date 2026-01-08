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
    enum class PropertyHandleTag : uint8_t {
        nil, // dud handle
        key, // string ID indexing into string pool -> compare indexed strings in access
        index, // number -> an Array's own properties -> int to Value
    };

    enum class PropertyHandleFlags : uint8_t {
        configurable = 0b00000001,
        writable = 0b00000010,
        enumerable = 0b00000100
    };

    struct PropertyHandle {
        static constexpr auto dud_id = -1;

        void* parent_obj_p; // must refer to the enclosing object's address in memory: helps distinguish properties between different object shapes / types with matching names / IDs
        int prop_id; // a unique scalar: could be an index / pooled string ID
        PropertyHandleTag tag; // discriminator for whether the property is an index (for own props. in Arrays) vs. a string (for any Object property)
        uint8_t flags;

        constexpr PropertyHandle() noexcept
        : parent_obj_p {}, prop_id {dud_id}, tag {PropertyHandleTag::nil}, flags {0x00} {}

        constexpr PropertyHandle(void* parent_obj_p_, int id_, PropertyHandleTag tag_, uint8_t flags_) noexcept
        : parent_obj_p {parent_obj_p_}, prop_id {id_}, tag {tag_}, flags {flags_} {}

        explicit constexpr operator bool(this auto&& self) noexcept {
            return self.parent_obj_p_ != nullptr && self.prop_id != dud_id && self.tag != PropertyHandleTag::nil;
        }

        [[nodiscard]] constexpr auto get_flag_of(std::same_as<PropertyHandleFlags> auto quality_flag) const noexcept {
            if constexpr (quality_flag == PropertyHandleFlags::configurable) {
                return flags & static_cast<uint8_t>(quality_flag);
            } else if constexpr (quality_flag == PropertyHandleFlags::writable) {
                return flags & static_cast<uint8_t>(quality_flag);
            } else if constexpr (quality_flag == PropertyHandleFlags::enumerable) {
                return flags & static_cast<uint8_t>(quality_flag);
            } else {
                return false;
            }
        }

        [[nodiscard]] constexpr auto operator==(const PropertyHandle& other) const noexcept -> bool {
            return parent_obj_p == other.parent_obj_p && prop_id == other.prop_id && static_cast<std::size_t>(tag) == static_cast<std::size_t>(other.tag);
        }

        [[nodiscard]] constexpr auto operator<(const PropertyHandle& other) const noexcept -> bool {
            return parent_obj_p < other.parent_obj_p && prop_id < other.prop_id && static_cast<std::size_t>(tag) < static_cast<std::size_t>(other.tag);
        }

        [[nodiscard]] constexpr auto operator>(const PropertyHandle& other) const noexcept -> bool {
            return parent_obj_p > other.parent_obj_p && prop_id > other.prop_id && static_cast<std::size_t>(tag) > static_cast<std::size_t>(other.tag);
        }
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
        virtual auto is_extensible() const noexcept -> bool = 0;
        virtual auto is_frozen() const noexcept -> bool = 0;
        virtual auto is_prototype() const noexcept -> bool = 0;

        virtual auto get_prototype() noexcept -> ObjectBase<V>* = 0;
        virtual auto get_property_value(const PropertyHandle& handle) -> V* = 0;
        virtual auto set_property_value(const PropertyHandle& handle, const V& value) -> V* = 0;
        virtual auto del_property_value(const PropertyHandle& handle) -> bool = 0;
        // virtual auto get_property_handle() -> PropertyHandle = 0;

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        virtual auto clone() const -> ObjectBase<V>* = 0;

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        virtual auto as_string() const -> std::string = 0;
    };

    /**
     * @brief Alias for any object's property pool. Note that a prototype object has its own pool that instances duplicate their structure from.
     * 
     * @tparam V This parameter must be a `Value` wrapper from `derkjs_impl/runtime/value.ixx`.
     */
    template <typename V>
    using PropPool = std::map<PropertyHandle, V>;

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

        [[nodiscard]] auto get_item(int id) noexcept -> ItemBase* {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return nullptr;
            }

            return m_items.data() + id;
        }

        [[maybe_unused]] auto update_item(int id, std::same_as<ItemBase> auto&& item) -> bool {
            if (id < 0 || id >= static_cast<int>(m_items.size())) {
                return false;
            }

            m_items[id] = std::forward<ItemBase>(item);

            return true;
        }

        [[nodiscard]] auto add_item(int id, std::same_as<ItemBase> auto&& item) -> bool {
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

            m_items[slot_id] = std::forward<ItemBase>(item);

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
}

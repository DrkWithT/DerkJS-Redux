module;

#include <type_traits>

export module meta.enums;

export namespace DerkJS::Meta {
    template <typename E> requires (std::is_integral_v<decltype(E::last)>)
    [[nodiscard]] constexpr auto capped_enum_n() noexcept -> std::size_t {
        return static_cast<std::size_t>(E::last);
    }

    template <typename E, typename EnFirst, typename ... EnRest> requires (std::is_scoped_enum_v<E> && std::is_same_v<E, EnFirst>)
    [[nodiscard]] constexpr auto mask_enum_vals(EnFirst val, EnRest ... rest) noexcept {
        using mask_v = std::underlying_type_t<E>;

        return ((static_cast<mask_v>(val)) | ... | (static_cast<mask_v>(rest)));
    }
}
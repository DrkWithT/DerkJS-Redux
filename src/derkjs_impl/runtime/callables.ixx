module;

#include <cstdint>
#include <utility>
#include <format>
#include <string>

export module runtime.callables;

import runtime.objects;
import runtime.value;
import runtime.bytecode;
import runtime.vm;

export namespace DerkJS {
    /**
     * @brief Wraps a native function pointer that is then invoked on the VM context (see `./src/derkjs_impl/runtime/vm.ixx` for `ExternVMCtx`) and some metadata (see `NativeFunction::native_func_p`). This allows for C++ functions that interact with the JS environment.
     */
    class NativeFunction : public ObjectBase<Value> {
    public:
        /// Points to a native, top-level C++ function for DerkJS. Args: JS VM's context, an optional property pool for the wrapping `NativeFunction`, and argument count passed (to compute the base offset of locals). After the call, the native MUST NOT mutate RBP or RIP at all.
        using native_func_p = bool(*)(ExternVMCtx*, PropPool<PropertyHandle<Value>, Value>*, int);

    private:
        PropPool<PropertyHandle<Value>, Value> m_own_props;
        native_func_p m_native_ptr;

    public:
        NativeFunction(native_func_p procedure_ptr) noexcept
        : m_own_props {}, m_native_ptr {procedure_ptr} {}

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "NativeFunction";
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

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<V>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() const noexcept -> const PropPool<PropertyHandle<V>, V>& override {
            return m_own_props;
        }

        [[nodiscard]] auto get_property_value(const PropertyHandle<V>& handle) -> V* override {
            // if (auto property_entry_it = m_own_props.find(handle); property_entry_it != m_own_props.end()) {
            //     return &property_entry_it->second;
            // }
            // return m_proto->get_property_value(handle);
            return nullptr;
        }

        [[nodiscard]] auto set_property_value(const PropertyHandle<V>& handle, const V& value) -> V* override {
            // m_own_props[handle] = value;
            // return &m_own_props[handle];
            return nullptr;
        }

        [[nodiscard]] auto del_property_value(const PropertyHandle<V>& handle) -> bool override {
            // return m_own_props.erase(m_own_props.find(handle)) != m_own_props.end();
            return false;
        }

        /// NOTE: the `opaque_ctx_p` MUST actually point to an `ExternVMCtx` and MUST NOT own the context.
        [[nodiscard]] auto call(void* opaque_ctx_p, int argc) -> bool override {
            return m_native_ptr(reinterpret_cast<ExternVMCtx*>(opaque_ctx_p), &m_own_props, argc);
        }

        /// NOTE: this only makes another wrapper around the C++ function ptr, but the raw pointer must be quickly owned by the VM heap (`PolyPool<ObjectBase<Value>>`)!
        [[nodiscard]] auto clone() const -> ObjectBase<V>* override {
            auto copied_native_fn_p = new NativeFunction {m_native_ptr};

            return copied_native_fn_p;
        }

        /// NOTE: unlike standard ECMAScript, this just prints `[Function <name>]`
        [[nodiscard]] auto as_string() const -> std::string override {
            return std::format("[{} native-fn-ptr({})]", get_class_name(), m_native_ptr);
        }

        /// NOTE: does an equality check for stored C++ function addresses
        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            return m_native_ptr == other.m_native_ptr;
        }

        /// NOTE: yields false- this is a dud
        [[nodiscard]] auto operator<(const ObjectBase& other) const noexcept -> bool override {
            return false;
        }

        /// NOTE: yields false- this is a dud
        [[nodiscard]] auto operator>(const ObjectBase& other) const noexcept -> bool override {
            return false;
        }
    };
}

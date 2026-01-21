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
        /// Points to a native, top-level C++ function for DerkJS. Args: JS VM's context, an optional property pool for the wrapping `NativeFunction`, and argument count passed (to compute the base offset of locals). Importantly, the native MUST NOT mutate RBP, RSP, or RIP at all.
        using native_func_p = bool(*)(ExternVMCtx*, PropPool<PropertyHandle<Value>, Value>*, int);

    private:
        PropPool<PropertyHandle<Value>, Value> m_own_properties;
        native_func_p m_native_ptr;

    public:
        NativeFunction(native_func_p procedure_ptr) noexcept
        : m_own_properties {}, m_native_ptr {procedure_ptr} {}

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

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() const noexcept -> const PropPool<PropertyHandle<Value>, Value>& override {
            return m_own_properties;
        }

        [[nodiscard]] auto get_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> Value* override {
            // if (auto property_entry_it = m_own_properties.find(handle); property_entry_it != m_own_properties.end()) {
            //     return &property_entry_it->second;
            // }
            // return m_proto->get_property_value(handle);
            return nullptr;
        }

        [[nodiscard]] auto set_property_value([[maybe_unused]] const PropertyHandle<Value>& handle, [[maybe_unused]] const Value& value) -> Value* override {
            // m_own_properties[handle] = value;
            // return &m_own_properties[handle];
            return nullptr;
        }

        [[nodiscard]] auto del_property_value([[maybe_unused]] const PropertyHandle<Value>& handle) -> bool override {
            // return m_own_properties.erase(m_own_properties.find(handle)) != m_own_properties.end();
            return false;
        }

        [[nodiscard]] auto call(void* opaque_ctx_p, int argc) -> bool override {
            /// NOTE: On usage, the `opaque_ctx_p` argument MUST point to an `ExternVMCtx` and MUST NOT own the context.
            auto vm_context_p = reinterpret_cast<ExternVMCtx*>(opaque_ctx_p);

            // 1. Prepare native callee stack pointer
            const int16_t callee_rsbp = vm_context_p->rsp - argc;
            const int16_t caller_rsbp = vm_context_p->rsbp;
            vm_context_p->rsbp = callee_rsbp;

            // 2. Make native call
            auto native_call_ok = m_native_ptr(vm_context_p, &m_own_properties, argc);

            // 3. Restore caller stack state after native call 
            vm_context_p->rsp = callee_rsbp;
            vm_context_p->rsbp = caller_rsbp;

            /// NOTE: Advance past the NATIVE_CALL to avoid endlessly dispatching!
            ++vm_context_p->rip_p;

            return native_call_ok;
        }

        /// NOTE: this only makes another wrapper around the C++ function ptr, but the raw pointer must be quickly owned by the VM heap (`PolyPool<ObjectBase<Value>>`)!
        [[nodiscard]] auto clone() const -> ObjectBase<Value>* override {
            auto copied_native_fn_p = new NativeFunction {m_native_ptr};

            return copied_native_fn_p;
        }

        /// NOTE: unlike standard ECMAScript, this just prints `[NativeFunction <function-address>]`
        [[nodiscard]] auto as_string() const -> std::string override {
            return std::format("[{} native-fn-ptr({})]", get_class_name(), reinterpret_cast<void*>(m_native_ptr)); // cast to void* needed here to use a valid std::formatter
        }

        /// NOTE: does an equality check by unique hardware address -> unique instance mapping to a unique C++ function ptr
        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            return this == &other;
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

    /**
     * @brief Wraps a lambda's bytecode buffer which is separate from the main bytecode buffer for top-level functions or code. However, this must be a object- All JS functions are basically objects.
     */
    class Lambda : public ObjectBase<Value> {
    private:
        PropPool<PropertyHandle<Value>, Value> m_own_properties;
        std::vector<Instruction> m_code;
        // std::unique_ptr<ObjectBase<Value>> m_prototype;

    public:
        Lambda(std::vector<Instruction> code) noexcept
        : m_own_properties {}, m_code (std::move(code)) {}

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "Lambda"; // non-standard behavior for ease of debugging the VM heap
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto is_frozen() const noexcept -> bool override {
            return true; // TODO: change this to depend on a member variable later since objects can be frozen (made immutable) at runtime.
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() const noexcept -> const PropPool<PropertyHandle<Value>, Value>& override {
            return m_own_properties;
        }

        [[nodiscard]] auto get_property_value(const PropertyHandle<Value>& handle) -> Value* override {
            return nullptr;
        }

        [[nodiscard]] auto set_property_value(const PropertyHandle<Value>& handle, const Value& value) -> Value* override {
            return nullptr;
        }

        [[nodiscard]] auto del_property_value(const PropertyHandle<Value>& handle) -> bool override {
            return false;
        }

        [[maybe_unused]] auto call(void* opaque_ctx_p, int argc) -> bool override {
            auto vm_ctx_p = reinterpret_cast<ExternVMCtx*>(opaque_ctx_p);

            const auto caller_ret_ip = vm_ctx_p->rip_p + 1;
            const int16_t new_callee_sbp = vm_ctx_p->rsp - argc;
            const int16_t old_caller_sbp = vm_ctx_p->rsbp;

            vm_ctx_p->rip_p = m_code.data();
            vm_ctx_p->rsbp = new_callee_sbp;

            vm_ctx_p->frames.emplace_back(tco_call_frame_type {
                caller_ret_ip,
                new_callee_sbp,
                old_caller_sbp
            });

            return true;
        }

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        [[nodiscard]] auto clone() const -> ObjectBase<Value>* override {
            return new Lambda {m_code};
        }

        /// NOTE: For default printing of objects, etc. to the console (stdout). 
        [[nodiscard]] auto as_string() const -> std::string override {
            return std::format("[Lambda bytecode-ptr({})]", reinterpret_cast<const void*>(m_code.data()));
        }

        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            return this == &other;
        }

        [[nodiscard]] auto operator<(const ObjectBase& other) const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto operator>(const ObjectBase& other) const noexcept -> bool override {
            return false;
        }

    };
}

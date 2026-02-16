module;

#include <cstdint>
#include <utility>
#include <format>
#include <string>
#include <string_view>
#include <sstream>
// #include <print>

export module runtime.callables;

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
        using native_func_p = bool(*)(ExternVMCtx*, PropPool<Value, Value>*, int);

    private:
        PropPool<Value, Value> m_own_properties;
        Value m_prototype;
        native_func_p m_native_ptr;
        uint8_t m_flags;

    public:
        NativeFunction(native_func_p procedure_ptr, ObjectBase<Value>* prototype_p) noexcept
        : m_own_properties {}, m_prototype {(prototype_p) ? Value {prototype_p} : Value {}}, m_native_ptr {procedure_ptr}, m_flags {std::to_underlying(AttrMask::unused)} {
            m_prototype.update_parent_flags(m_flags);
        }

        [[nodiscard]] auto get_native_fn_addr() const noexcept -> const void* {
            return reinterpret_cast<const void*>(m_native_ptr);
        }

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "function";
        }

        [[nodiscard]] auto get_typename() const noexcept -> std::string_view override {
            return "function";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return true;
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_prototype.to_object();
        }

        [[nodiscard]] auto get_seq_items() noexcept -> std::vector<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_properties;
        }

        [[nodiscard]] auto get_property_value([[maybe_unused]] const Value& key, [[maybe_unused]] bool allow_filler) -> PropertyDescriptor<Value> override {
            /// NOTE: for now, I'll cheese this to get Object methods working: Just get / search the prototype.
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {&key, &m_prototype, this, m_flags};
            } else if (auto prototype_p = m_prototype.to_object(); prototype_p) {
                return prototype_p->get_property_value(key, allow_filler);
            }

            return PropertyDescriptor<Value> {};
        }

        void freeze() noexcept override {
            m_flags = std::to_underlying(AttrMask::is_parent_frozen);

            // for (auto& entry : m_own_properties) {
            //     entry.item.update_parent_flags(m_flags);
            // }

            m_prototype.update_parent_flags(m_flags);

            if (auto prototype_object_p = m_prototype.to_object(); prototype_object_p) {
                prototype_object_p->freeze();
            }
        }

        [[nodiscard]] auto set_property_value([[maybe_unused]] const Value& key, [[maybe_unused]] const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);

            return &(property_desc = value);
        }

        [[nodiscard]] auto del_property_value([[maybe_unused]] const Value& key) -> bool override {
            // return m_own_properties.erase(m_own_properties.find(handle)) != m_own_properties.end();
            return false;
        }

        void update_on_accessor_mut([[maybe_unused]] Value* accessor_p, [[maybe_unused]] const Value& value) override {}

        [[nodiscard]] auto call(void* opaque_ctx_p, int argc, [[maybe_unused]] bool has_this_arg) -> bool override {
            /// 1.1: Prepare the `opaque_ctx_p` argument, which MUST point to an `ExternVMCtx` and MUST NOT own the context.
            auto vm_context_p = reinterpret_cast<ExternVMCtx*>(opaque_ctx_p);
            ObjectBase<Value>* this_arg_p = (has_this_arg) ? vm_context_p->stack[vm_context_p->rsp - 1].to_object() : nullptr;

            // 1.2: Just get the captures from the previous caller. This is a native function that's NOT a regular JS Function.
            ObjectBase<Value>* caller_capture_p = vm_context_p->frames.back().capture_p;

            if (has_this_arg) {
                --vm_context_p->rsp;
            }

            // 1. Prepare native callee stack pointer
            const int16_t callee_rsbp = vm_context_p->rsp - static_cast<int16_t>(argc);
            const int16_t caller_rsbp = vm_context_p->rsbp;
            vm_context_p->rsbp = callee_rsbp;

            vm_context_p->frames.emplace_back(ExternVMCtx::call_frame_type {
                /// NOTE: Just put nullptr- the native callee will handle its own return of control back to the interpreter.
                .m_caller_ret_ip = vm_context_p->rip_p + 1,
                /// NOTE: Take the `this` argument of the object passed in case the native callee needs it via the C++ API.
                .this_p = this_arg_p,
                .caller_addr = this,
                .capture_p = caller_capture_p,
                /// NOTE: Put unused dud values for bytecode RBP, etc. because the native callee handles its own return.
                .m_callee_sbp = -1,
                .m_caller_sbp = -1,
                .m_flags = 0
            });

            // 2. Make native call
            auto native_call_ok = m_native_ptr(vm_context_p, &m_own_properties, argc);

            // 3. Restore caller stack state after native call
            vm_context_p->frames.pop_back();
            vm_context_p->rsp = callee_rsbp;
            vm_context_p->rsbp = caller_rsbp;
            vm_context_p->rip_p++;

            return native_call_ok;
        }

        [[nodiscard]] auto call_as_ctor([[maybe_unused]] void* opaque_ctx_p, [[maybe_unused]] int argc) -> bool override {
            // 1.1: After access of the VM context, all constructor result objects have the function prototype.
            auto vm_context_p = reinterpret_cast<ExternVMCtx*>(opaque_ctx_p);

            const int16_t callee_rsbp = vm_context_p->rsp - static_cast<int16_t>(argc);
            const int16_t caller_rsbp = vm_context_p->rsbp;
            vm_context_p->rsbp = callee_rsbp;

            // 2. Push dummy frame mostly to track ctor-call status.
            vm_context_p->frames.emplace_back(ExternVMCtx::call_frame_type {
                /// NOTE: Just put nullptr- the native callee will handle its own return of control back to the interpreter.
                .m_caller_ret_ip = nullptr,
                .this_p = nullptr,
                .caller_addr = this,
                .capture_p = nullptr,
                /// NOTE: Put unused dud values for bytecode RBP, etc. because the native callee handles its own return.
                .m_callee_sbp = -1,
                .m_caller_sbp = -1,
                /// NOTE: ctor call applies here.
                .m_flags = std::to_underlying(CallFlags::is_ctor)
            });

            // 3. Make native call
            auto native_call_ok = m_native_ptr(vm_context_p, &m_own_properties, argc);

            vm_context_p->frames.pop_back();

            // 4. Restore caller stack state after native call
            vm_context_p->rsp = callee_rsbp;
            vm_context_p->rsbp = caller_rsbp;
            vm_context_p->rip_p++;

            return native_call_ok;
        }

        /// NOTE: this only makes another wrapper around the C++ function ptr, but the raw pointer must be quickly owned by the VM heap (`PolyPool<ObjectBase<Value>>`)!
        [[nodiscard]] auto clone() -> ObjectBase<Value>* override {
            auto copied_native_fn_p = new NativeFunction {m_native_ptr, m_prototype.to_object()};

            return copied_native_fn_p;
        }

        [[nodiscard]] auto opaque_iter() const noexcept -> OpaqueIterator override {
            return OpaqueIterator {};
        }

        /// NOTE: unlike standard ECMAScript, this just prints `[NativeFunction <function-address>]`
        [[nodiscard]] auto as_string() const -> std::string override {
            return std::format("[{} native-fn-ptr({})]", get_class_name(), reinterpret_cast<void*>(m_native_ptr)); // cast to void* needed here to use a valid std::formatter
        }

        /// NOTE: does an equality check by unique hardware address -> unique instance mapping to a unique C++ function ptr
        [[nodiscard]] auto operator==(const ObjectBase& other) const noexcept -> bool override {
            if (auto other_as_native_callable_p = dynamic_cast<const NativeFunction*>(&other); other_as_native_callable_p) {
                return other_as_native_callable_p->get_native_fn_addr() == reinterpret_cast<const void*>(m_native_ptr);
            }

            return false;
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
        static constexpr std::array<std::string_view, static_cast<std::size_t>(Opcode::last)> opcode_names = {
            "djs_nop",
            "djs_dup",
            "djs_dup_local",
            "djs_ref_local",
            "djs_store_upval",
            "djs_ref_upval",
            "djs_put_const",
            "djs_deref",
            "djs_pop",
            "djs_emplace",
            "djs_put_this",
            "djs_discard",
            "djs_typename",
            "djs_put_obj_dud",
            "djs_make_arr",
            "djs_put_proto_key",
            "djs_get_prop", // Args: <should-default>: gets a property value based on RSP: <OBJ-REF>, RSP - 1: <POOLED-STR-REF>; IF should-default == 1, default any invalid key to `undefined`.
            "djs_put_prop", // SEE: djs_get_prop for stack args passing...
            "djs_del_prop", // SEE: djs_get_prop for stack args passing...
            "djs_numify",
            "djs_strcat",
            "djs_mod",
            "djs_mul",
            "djs_div",
            "djs_add",
            "djs_sub",
            "djs_test_falsy",
            "djs_test_strict_eq",
            "djs_test_strict_ne",
            "djs_test_lt",
            "djs_test_lte",
            "djs_test_gt",
            "djs_test_gte",
            "djs_jump_else",
            "djs_jump_if",
            "djs_jump",
            "djs_object_call",
            "djs_ctor_call",
            "djs_ret",
            "djs_halt",
        };

        PropPool<Value, Value> m_own_properties;
        std::vector<Instruction> m_code;
        Value m_prototype;
        uint8_t m_flags;

    public:
        Lambda(std::vector<Instruction> code, ObjectBase<Value>* prototype_p) noexcept
        : m_own_properties {}, m_code (std::move(code)), m_prototype {prototype_p}, m_flags {std::to_underlying(AttrMask::unused)} {
            m_prototype.update_parent_flags(m_flags);
        }

        [[nodiscard]] auto get_unique_addr() noexcept -> void* override {
            return this;
        }

        [[nodiscard]] auto get_class_name() const noexcept -> std::string override {
            return "function";
        }

        [[nodiscard]] auto get_typename() const noexcept -> std::string_view override {
            return "function";
        }

        [[nodiscard]] auto is_extensible() const noexcept -> bool override {
            return true;
        }

        [[nodiscard]] auto is_prototype() const noexcept -> bool override {
            return false;
        }

        [[nodiscard]] auto get_prototype() noexcept -> ObjectBase<Value>* override {
            return m_prototype.to_object();
        }

        [[nodiscard]] auto get_seq_items() noexcept -> std::vector<Value>* override {
            return nullptr;
        }

        [[nodiscard]] auto get_own_prop_pool() noexcept -> PropPool<Value, Value>& override {
            return m_own_properties;
        }

        [[nodiscard]] auto get_property_value(const Value& key, bool allow_filler) -> PropertyDescriptor<Value> override {
            if (key.is_prototype_key()) {
                return PropertyDescriptor<Value> {&key, &m_prototype, this, m_flags};
            } else if (auto property_entry_it = std::find_if(m_own_properties.begin(), m_own_properties.end(), [&key](const auto& prop) -> bool {
                return prop.key == key;
            }); property_entry_it != m_own_properties.end()) {
                return PropertyDescriptor<Value> {&property_entry_it->key, &property_entry_it->item, this, static_cast<uint8_t>(m_flags & property_entry_it->flags)};
            } else if ((m_flags & std::to_underlying(AttrMask::writable)) && !m_prototype && allow_filler) {
                return PropertyDescriptor<Value> {
                    &key,
                    &m_own_properties.emplace_back(
                        key, Value {},
                        static_cast<uint8_t>(m_flags & std::to_underlying(AttrMask::unused))
                    ).item,
                    this,
                    m_flags
                };
            } else if (auto prototype_p = m_prototype.to_object(); prototype_p) {
                return prototype_p->get_property_value(key, allow_filler);
            }

            return PropertyDescriptor<Value> {};
        }

        void freeze() noexcept override {
            m_flags = std::to_underlying(AttrMask::is_parent_frozen);

            // for (auto& entry : m_own_properties) {
            //     entry.item.update_parent_flags(m_flags);
            // }

            m_prototype.update_parent_flags(m_flags);

            if (auto prototype_object_p = m_prototype.to_object(); prototype_object_p) {
                prototype_object_p->freeze();
            }
        }

        [[nodiscard]] auto set_property_value(const Value& key, const Value& value) -> Value* override {
            auto property_desc = get_property_value(key, true);

            return &(property_desc = value);
        }

        [[nodiscard]] auto del_property_value([[maybe_unused]] const Value& key) -> bool override {
            return false;
        }

        void update_on_accessor_mut([[maybe_unused]] Value* accessor_p, [[maybe_unused]] const Value& value) override {}

        [[maybe_unused]] auto call(void* opaque_ctx_p, int argc, bool has_this_arg) -> bool override {
            auto vm_context_p = reinterpret_cast<ExternVMCtx*>(opaque_ctx_p);

            // 1.1: After accessing the VM context, consume `this` argument (if needed) before the call to avoid garbage results.
            ObjectBase<Value>* this_arg_p = (has_this_arg) ? vm_context_p->stack[vm_context_p->rsp - 1].to_object() : nullptr;

            // 1.2: Only allocate a capture Object for different callee vs. caller Functions.
            ObjectBase<Value>* caller_capture_p = vm_context_p->frames.back().capture_p;
            auto capture_obj_p = (vm_context_p->frames.back().caller_addr == this) ? caller_capture_p : vm_context_p->heap.add_item(vm_context_p->heap.get_next_id(), std::make_unique<Object>(caller_capture_p));

            // if (vm_context_p->frames.back().caller_addr == this) {
            //     std::println("Captures: Object(this = {})", reinterpret_cast<void*>(capture_obj_p));
            // } else {
            //     std::println("Captures: Object(this = {}, prototype = {})", reinterpret_cast<void*>(capture_obj_p), reinterpret_cast<void*>(caller_capture_p));
            // }

            const auto caller_ret_ip = vm_context_p->rip_p + 1;
            const int16_t old_caller_sbp = vm_context_p->rsbp;
            int16_t new_callee_sbp = vm_context_p->rsp - (argc + static_cast<int16_t>(has_this_arg));

            vm_context_p->rip_p = m_code.data();
            vm_context_p->rsbp = new_callee_sbp;
            vm_context_p->rsp = new_callee_sbp + argc - 1;

            vm_context_p->frames.emplace_back(ExternVMCtx::call_frame_type {
                .m_caller_ret_ip = caller_ret_ip,
                .this_p = this_arg_p,
                .caller_addr = this,
                .capture_p = capture_obj_p,
                .m_callee_sbp = new_callee_sbp,
                .m_caller_sbp = old_caller_sbp,
                .m_flags = 0
            });

            return true;
        }

        /// TODO: support passing of `this` arguments to ctors which may be methods of an object!
        [[nodiscard]] auto call_as_ctor(void* opaque_ctx_p, int argc) -> bool override {
            // 1.1: After access of the VM context, all constructor result objects have the function prototype.
            auto vm_context_p = reinterpret_cast<ExternVMCtx*>(opaque_ctx_p);
            auto this_arg_p = vm_context_p->heap.add_item(
                vm_context_p->heap.get_next_id(),
                std::make_unique<Object>(m_prototype.to_object())
            );

            if (!this_arg_p) {
                return false;
            }

            // 1.2: Only allocate a capture Object for different callee vs. caller Functions.
            ObjectBase<Value>* caller_capture_p = vm_context_p->frames.back().capture_p;
            auto capture_obj_p = (vm_context_p->frames.back().caller_addr == this) ? caller_capture_p : vm_context_p->heap.add_item(vm_context_p->heap.get_next_id(), std::make_unique<Object>(caller_capture_p));

            const auto caller_ret_ip = vm_context_p->rip_p + 1;
            const int16_t new_callee_sbp = vm_context_p->rsp - argc;
            const int16_t old_caller_sbp = vm_context_p->rsbp;

            vm_context_p->rip_p = m_code.data();
            vm_context_p->rsbp = new_callee_sbp; // 0, Sequence
            vm_context_p->rsp = new_callee_sbp;

            vm_context_p->frames.emplace_back(ExternVMCtx::call_frame_type {
                .m_caller_ret_ip = caller_ret_ip,
                .this_p = this_arg_p,
                .caller_addr = this,
                .capture_p = capture_obj_p,
                .m_callee_sbp = new_callee_sbp,
                .m_caller_sbp = old_caller_sbp,
                /// NOTE: ctor call applies here.
                .m_flags = std::to_underlying(CallFlags::is_ctor)
            });

            return true;
        }

        /// For prototypes, this creates a self-clone which is practically an object instance. This raw pointer must be quickly owned by a `PolyPool<ObjectBase<V>>`!
        [[nodiscard]] auto clone() -> ObjectBase<Value>* override {
            return new Lambda {m_code, m_prototype.to_object()};
        }

        [[nodiscard]] auto opaque_iter() const noexcept -> OpaqueIterator override {
            return OpaqueIterator {};
        }

        /// NOTE: For disassembling lambda bytecode for debugging.
        [[nodiscard]] auto as_string() const -> std::string override {
            std::ostringstream sout;

            sout << std::format("[Lambda bytecode-ptr({})] [\n", reinterpret_cast<const void*>(m_code.data()));

            for (auto lambda_bc_pos = 0; const auto& [instr_argv, instr_op] : m_code) {
                sout << std::format(
                    "\t{}: {} {} {}\n",
                    lambda_bc_pos,
                    opcode_names.at(static_cast<std::size_t>(instr_op)),
                    instr_argv[0],
                    instr_argv[1]
                );

                ++lambda_bc_pos;
            }

            sout << "]\n";

            return sout.str();
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

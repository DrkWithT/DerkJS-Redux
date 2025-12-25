module;

#include <optional>
// #include <vector>
#include <flat_map>


export module backend.ir_generate;

import backend.ir;
import frontend.ast;

export namespace DerkJS {
    class IrGenPass {
    private:
        // todo

    public:
        IrGenPass() = default;

        [[nodiscard]] auto operator()([[maybe_unused]] const ASTUnit& tu, [[maybe_unused]] const std::flat_map<int, std::string>& source_map) -> std::optional<IrBundle> {
            return {};
        }
    };
}

module;

#include <memory>
#include <vector>
#include <queue>
#include <flat_map>
#include <print>

export module runtime.gc;

import runtime.objects;
import runtime.value;

namespace DerkJS {
    /// Indicates reachability status for objects.
    enum class GCMark : uint8_t {
        dead,
        live
    };

    struct GCEntry {
        int object_id;
        GCMark mark;
    };

    export class GC {
    private:
        /// Census of all raw object pointers to their IDs & has liveness info.
        std::flat_map<void*, GCEntry> m_tracked;
        std::size_t m_threshold;

    public:
        GC(std::size_t max_overhead)
        : m_tracked {}, m_threshold {max_overhead} {}
        
        void operator()(PolyPool<ObjectBase<Value>>& heap, std::vector<Value>& stack, int rsp) {
            using derkjs_object_ptr = ObjectBase<Value>*;

            auto& heap_items = heap.items();
            const auto heap_overhead_sz = heap.get_overhead();
            auto reap_count = 0;

            if (heap_overhead_sz < m_threshold) {
                return;
            }

            int heap_count = heap_items.size();

            // 1. Census all heap object pointers to IDs.
            for (int heap_id = 0; heap_id < heap_count; heap_id++) {
                derkjs_object_ptr temp_p = heap_items.at(heap_id).get();

                m_tracked[temp_p] = GCEntry {
                    .object_id = heap_id,
                    .mark = GCMark::dead
                };
            }

            // 2. DFS (sweep) over each object reference in the stack, updating reachability data if needed.
            std::queue<derkjs_object_ptr> frontier;

            for (int gc_sp = 0; gc_sp < rsp; gc_sp++) {
                if (derkjs_object_ptr temp_root = stack.at(gc_sp).to_object(); temp_root) {
                    frontier.push(temp_root);
                }
            }

            while (!frontier.empty()) {
                auto next_ptr = frontier.front();
                frontier.pop();

                if (m_tracked.at(next_ptr).mark == GCMark::live) {
                    continue;
                }

                m_tracked.at(next_ptr).mark = GCMark::live;

                if (auto maybe_array_items_p = next_ptr->get_seq_items(); maybe_array_items_p) {
                    for (auto& array_items = *maybe_array_items_p; auto& item_value : array_items) {
                        if (auto next_item_p = item_value.to_object(); next_item_p) {
                            frontier.push(next_item_p);
                        }
                    }
                }

                for (auto& [prop_key, prop_v] : next_ptr->get_own_prop_pool()) {
                    /// TODO: add prop_key reachability checks!
                    if (auto neighbor_ptr = prop_v.to_object(); neighbor_ptr) {
                        frontier.push(neighbor_ptr);
                    }
                }
            }

            // 3. Delete all object IDs that are "dead".
            for (auto [next_object_p, life_info] : m_tracked) {
                if (auto [target_id, target_mark] = life_info; target_mark == GCMark::dead) {
                    if (heap.remove_item(target_id)) {
                        reap_count++;
                    }
                }
            }

            // 4. Reset state except `m_overhead`.
            m_tracked.clear();

            std::println("GC: collected {} objects.", reap_count);
        }
    };
}

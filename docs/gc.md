## GC Notes

### General Ideas:
 - Uses mark & sweep algorithm.
 - Runs just before object creation.

### Reachability Marks:
 - Unknown
 - Live (reachable)
 - Dead (unreachable)

### Steps:
 - Sweep over the stack for any object references as "roots". These are the first reachable objects.
    - Census the mapping of live heap objects to IDs.
        - `std::flat_map<void*, int>`
    - Do a DFS per root with this state:
        - `std::flat_map<void*, GCMark>`
        - Mark all reachable objects with an iterative BFS. If an object pointer is found, mark it as live. Otherwise, leave it as dead.
    - Then pass over the liveness map from all BFS's... For any dead pointer, clear the ID slot.

#ifndef SNAPSHOT_H
#define SNAPSHOT_H

#include <string>
#include <vector>
#include <memory>
#include "filesystem/block_manager.h"

struct SnapshotInfo {
    uint32_t id;            // Snapshot ID (Inode ID of the snapshot root)
    std::string name;       // Snapshot name
    uint64_t timestamp;     // Creation time
    uint32_t sourceInode;   // Original Inode ID
};

class SnapshotManager {
public:
    explicit SnapshotManager(std::shared_ptr<BlockManager> blockManager);
    ~SnapshotManager() = default;

    // Create a snapshot of the directory at sourcePath
    // Returns the Inode ID of the snapshot root, or INVALID_INODE on failure
    uint32_t createSnapshot(uint32_t sourceInodeId, const std::string& name);

    // List all snapshots
    std::vector<SnapshotInfo> listSnapshots() const;

    // Delete a snapshot
    bool deleteSnapshot(uint32_t snapshotInodeId);

private:
    std::shared_ptr<BlockManager> blockManager_;
    std::vector<SnapshotInfo> snapshots_;
    
    void loadSnapshots();
    void saveSnapshots();
};

#endif // SNAPSHOT_H

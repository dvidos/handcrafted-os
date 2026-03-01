#pragma once

// POSIX-related flags for mount()

#define VFS_MOUNT_READONLY   (1 << 0)
#define VFS_MOUNT_NOEXEC     (1 << 1)
#define VFS_MOUNT_NODEV      (1 << 2)
#define VFS_MOUNT_NOSUID     (1 << 3)


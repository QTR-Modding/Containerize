#pragma once

// Debug guards to detect improper re-entrant or mixed locking (debug builds only)
// Debug guards to detect improper re-entrant or mixed locking (debug builds only)
#ifndef NDEBUG
#include <shared_mutex>

namespace DebugLock {
    struct DebugLockState {
        static thread_local int sharedDepth;
        static thread_local int uniqueDepth;
    };

    thread_local int DebugLockState::sharedDepth = 0;
    thread_local int DebugLockState::uniqueDepth = 0;

    struct DebugSharedLock {
        std::shared_mutex* m;

        explicit DebugSharedLock(std::shared_mutex* mutex) : m(mutex) {
            assert(m && "DebugSharedLock: mutex pointer is null");
            // Cannot take shared if this thread already holds unique
            assert(DebugLockState::uniqueDepth == 0 &&
                   "Attempt to acquire shared lock while holding unique lock on Manager::mutex_ (undefined behavior). "
                   "Release unique lock first.");
            // Prevent re-entrant shared acquisition
            if (DebugLockState::sharedDepth++ == 0) {
                m->lock_shared();
            } else {
                assert(false &&
                       "Re-entrant shared lock acquisition detected on Manager::mutex_ (undefined behavior). Refactor "
                       "using NoLock helpers.");
            }
        }

        DebugSharedLock(const DebugSharedLock&) = delete;
        DebugSharedLock& operator=(const DebugSharedLock&) = delete;
        DebugSharedLock(DebugSharedLock&&) = delete;
        DebugSharedLock& operator=(DebugSharedLock&&) = delete;

        ~DebugSharedLock() {
            assert(m && "DebugSharedLock: mutex pointer is null on destruction");
            assert(DebugLockState::sharedDepth > 0 && "Shared depth underflow");
            if (--DebugLockState::sharedDepth == 0) {
                m->unlock_shared();
            }
        }
    };

    struct DebugUniqueLock {
        std::shared_mutex* m;
        bool owns = false;

        explicit DebugUniqueLock(std::shared_mutex* mutex) : m(mutex) {
            assert(m && "DebugUniqueLock: mutex pointer is null");
            // Cannot take unique if shared is currently held
            assert(DebugLockState::sharedDepth == 0 &&
                   "Attempt to acquire unique lock while holding shared lock on Manager::mutex_ (illegal upgrade). "
                   "Release shared first.");
            // Prevent unique re-entrancy
            assert(
                DebugLockState::uniqueDepth == 0 &&
                "Re-entrant unique lock acquisition detected on Manager::mutex_. Refactor to avoid nested mutations.");
            m->lock();
            owns = true;
            DebugLockState::uniqueDepth = 1;
        }

        DebugUniqueLock(const DebugUniqueLock&) = delete;
        DebugUniqueLock& operator=(const DebugUniqueLock&) = delete;
        DebugUniqueLock(DebugUniqueLock&&) = delete;
        DebugUniqueLock& operator=(DebugUniqueLock&&) = delete;

        ~DebugUniqueLock() {
            if (owns) {
                assert(DebugLockState::uniqueDepth == 1 && "Unique depth corruption");
                m->unlock();
                DebugLockState::uniqueDepth = 0;
            }
        }
    };
}

    #define SHARED_GUARD DebugSharedLock slock(&mutex_)
    #define UNIQUE_GUARD DebugUniqueLock ulock(&mutex_)
#else
    #define SHARED_GUARD std::shared_lock slock(mutex_)
    #define UNIQUE_GUARD std::unique_lock ulock(mutex_)
#endif
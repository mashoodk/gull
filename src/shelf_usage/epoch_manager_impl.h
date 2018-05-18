/*
 *  (c) Copyright 2016-2017 Hewlett Packard Enterprise Development Company LP.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  As an exception, the copyright holders of this Library grant you permission
 *  to (i) compile an Application with the Library, and (ii) distribute the
 *  Application containing code generated by the Library and added to the
 *  Application during this compilation process under terms of your choice,
 *  provided you also meet the terms and conditions of the Application license.
 *
 */

#ifndef _NVMM_EPOCH_MANAGER_IMPL_H_
#define _NVMM_EPOCH_MANAGER_IMPL_H_

#include <atomic>
#include <pthread.h>
#include <stddef.h>
#include <string>
#include <time.h>
#include <vector>

#include "nvmm/epoch_manager.h"

#include "shelf_usage/epoch_vector.h"
#include "shelf_usage/dclcrwlock.h"
#include "shelf_usage/participant_manager.h"
#include "shelf_usage/smart_shelf.h"


namespace nvmm {

class EpochManagerImpl
{
public:
    /**
     * \brief Construct and initialize epoch manager 
     *
     * \details 
     * Grabs an epoch-counter slot in the global epoch vector
     */
    EpochManagerImpl(void *addr, bool may_create);

    /**
     * \brief Teardown epoch manager
     * 
     * \details
     * Unregisters itself from the global epoch vector.
     */
    virtual ~EpochManagerImpl();

    /** Disable monitor thread */
    void disable_monitor();

    /** Enter an epoch-protected critical region */
    void enter_critical();

    /** Exit an epoch-protected critical region */
    void exit_critical();

    /** 
     * \brief Return whether there is at least one active epoch-protected 
     * critical region 
     *
     * \details
     * This check is inherently racy as the active region may end by the time
     * the function returns.
     * On the other hand, we have no way to tell if a thread is running inside
     * a critical region as we don't maintain per-thread state.
     */
    bool exists_active_critical();

    /** Return the last reported epoch by this epoch manager */
    EpochCounter reported_epoch();

    /** Return the frontier epoch */
    EpochCounter frontier_epoch();

    /** Set debug logging level */
    void set_debug_level(int level);

    void register_failure_callback(EpochManagerCallback cb);

    EpochManagerImpl(const EpochManagerImpl&)            = delete;
    EpochManagerImpl& operator=(const EpochManagerImpl&) = delete;

    pid_t self_id() { return pid_; }

    void reset_vector();

private:
    /**
     * \brief Reports this epoch-manager's current view of the frontier
     *
     */
    void report_frontier();

    /** 
     * \brief Attempt to advance frontier epoch
     *
     * \details
     * Succeeds if all participants are at the frontier epoch or the epoch 
     * previous to the frontier.
     * 
     * Not thread-safe
     */
    bool advance_frontier();

private:
    static const size_t POOL_SIZE             = 1024*1024; // bytes
    static const size_t MAX_POOL_SIZE         = 1024*1024; // bytes
    static const size_t MONITOR_INTERVAL_US   = 1000; //10;
    static const size_t HEARTBEAT_INTERVAL_US = 1000; //10;
    static const size_t TIMEOUT_US            = 1000000;
    static const size_t DEBUG_INTERVAL_US     = 1000000;

    void monitor_thread_entry();
    void heartbeat_thread_entry();

private:
    SmartShelf<internal::_EpochVector>      metadata_pool_;      // internal pool storing epoch-manager metadata
    ParticipantID            pid_;
    internal::EpochVector*             epoch_vec_; 
    internal::EpochVector::Participant epoch_participant_;
    internal::DCLCRWLock               epoch_lock_;         // lock protecting local epoch advancement
    pthread_mutex_t                    active_epoch_mutex_; // mutex protecting active epoch count
    int                                active_epoch_count_;
    std::thread                        monitor_thread_;
    std::thread                        heartbeat_thread_;
    std::atomic<bool>                  terminate_monitor_;
    std::atomic<bool>                  terminate_heartbeat_;
    int                                debug_level_;
    struct timespec                    last_scan_time_;
    EpochManagerCallback               cb_;
    EpochCounter                       last_frontier_;

};




} // end namespace nvmm

#endif // _NVMM_EPOCH_MANAGER_H_

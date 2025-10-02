/*
 * Copyright 2021 Database Group, Nagoya University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INDEX_FIXTURES_INDEX_FIXTURE_MULTI_THREAD_HPP
#define INDEX_FIXTURES_INDEX_FIXTURE_MULTI_THREAD_HPP

// C++ standard libraries
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

// external sources
#include "gtest/gtest.h"

// local sources
#include "common.hpp"

namespace dbgroup::index::test
{
/*######################################################################################
 * Fixture class definition
 *####################################################################################*/

template <class IndexInfo>
class IndexMultiThreadFixture : public testing::Test
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  // extract key-payload types
  using Key = typename IndexInfo::Key::Data;
  using Payload = typename IndexInfo::Payload::Data;
  using KeyComp = typename IndexInfo::Key::Comp;
  using PayComp = typename IndexInfo::Payload::Comp;
  using Index_t = typename IndexInfo::Index_t;
  using ImplStat = typename IndexInfo::ImplStatus;
  using ScanKey = std::optional<std::tuple<const Key &, size_t, bool>>;

  using EpochManager = ::dbgroup::memory::EpochManager;

 protected:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  static constexpr size_t kThreadNum = DBGROUP_TEST_THREAD_NUM;
  static constexpr size_t kKeyNum = (kExecNum + 2) * kThreadNum;
  static constexpr size_t kWaitForThreadCreation = 100;
  static constexpr size_t kEpochIntervalMicro = 1000;

  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    keys_ = PrepareTestData<Key>(kKeyNum);
    payloads_ = PrepareTestData<Payload>(kKeyNum);

    auto epoch_manager = std::make_shared<EpochManager>();
    epoch_manager_ = epoch_manager;
    index_ = std::make_unique<Index_t>(epoch_manager, kEpochIntervalMicro);
    is_ready_ = false;
  }

  void
  TearDown() override
  {
    index_ = nullptr;
  }

  /*####################################################################################
   * Utility functions
   *##################################################################################*/

  void
  PrepareData()
  {
    keys_ = PrepareTestData<Key>(kKeyNum);
    payloads_ = PrepareTestData<Payload>(kThreadNum * 2);
  }

  void
  DestroyData()
  {
    ReleaseTestData(keys_);
    ReleaseTestData(payloads_);
  }

  auto
  Write(  //
      [[maybe_unused]] const size_t key_id,
      [[maybe_unused]] const size_t pay_id)
  {
    if constexpr (HasWriteOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      const auto &payload = payloads_.at(pay_id);
      return index_->Write(key, payload, GetLength(key), GetLength(payload));
    } else {
      return 0;
    }
  }

  auto
  Insert(  //
      [[maybe_unused]] const size_t key_id,
      [[maybe_unused]] const size_t pay_id)
  {
    if constexpr (HasInsertOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      const auto &payload = payloads_.at(pay_id);
      return index_->Insert(key, payload, GetLength(key), GetLength(payload));
    } else {
      return 0;
    }
  }

  auto
  Update(  //
      [[maybe_unused]] const size_t key_id,
      [[maybe_unused]] const size_t pay_id)
  {
    if constexpr (HasUpdateOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      const auto &payload = payloads_.at(pay_id);
      return index_->Update(key, payload, GetLength(key), GetLength(payload));
    } else {
      return 0;
    }
  }

  auto
  Delete([[maybe_unused]] const size_t key_id)
  {
    if constexpr (HasDeleteOperation<ImplStat>()) {
      const auto &key = keys_.at(key_id);
      return index_->Delete(key, GetLength(key));
    } else {
      return 0;
    }
  }

  [[nodiscard]] auto
  CreateTargetIDs(                 //
      const size_t rec_num) const  //
      -> std::vector<size_t>
  {
    std::vector<size_t> target_ids{};
    target_ids.reserve(rec_num);
    for (size_t i = 0; i < rec_num; ++i) {
      target_ids.emplace_back(i);
    }

    return target_ids;
  }

  [[nodiscard]] auto
  CreateTargetIDs(  //
      const size_t w_id,
      const AccessPattern pattern)  //
      -> std::vector<size_t>
  {
    std::vector<size_t> target_ids{};
    {
      std::shared_lock guard{s_mtx_};

      target_ids.reserve(kExecNum);
      if (pattern == kReverse) {
        for (size_t i = kExecNum; i > 0; --i) {
          target_ids.emplace_back(kThreadNum * i + w_id);
        }
      } else {
        for (size_t i = 1; i <= kExecNum; ++i) {
          target_ids.emplace_back(kThreadNum * i + w_id);
        }
      }

      if (pattern == kRandom) {
        std::mt19937_64 rand_engine{kRandomSeed};
        std::shuffle(target_ids.begin(), target_ids.end(), rand_engine);
      }
    }

    std::unique_lock lock{x_mtx_};
    cond_.wait(lock, [this] { return is_ready_; });

    return target_ids;
  }

  [[nodiscard]] auto
  CreateTargetIDsForConcurrentSMOs()  //
      -> std::vector<size_t>
  {
    std::mt19937_64 rng{kRandomSeed};
    std::uniform_int_distribution<size_t> exec_dist{1, kExecNum};
    std::uniform_int_distribution<size_t> thread_dist{0, kThreadNum / 2 - 1};
    std::vector<size_t> target_ids{};
    {
      std::shared_lock guard{s_mtx_};

      target_ids.reserve(kExecNum);
      for (size_t i = 0; i < kExecNum; ++i) {
        target_ids.emplace_back(kThreadNum * exec_dist(rng) + thread_dist(rng));
      }
    }

    std::unique_lock lock{x_mtx_};
    cond_.wait(lock, [this] { return is_ready_; });

    return target_ids;
  }

  void
  RunMT(const std::function<void(size_t)> &func)
  {
    std::vector<std::thread> threads{};
    for (size_t i = 0; i < kThreadNum; ++i) {
      threads.emplace_back(func, i);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds{kWaitForThreadCreation});
    std::lock_guard guard{s_mtx_};

    is_ready_ = true;
    cond_.notify_all();

    for (auto &&t : threads) {
      t.join();
    }
  }

  void
  RunMTMultiOperation(
      const std::function<void(size_t)> &func_single,  // one thread runs func_single
      const std::function<void(size_t)> &func_multi)   // and the others run func_multi
  {
    std::vector<std::thread> threads{};
    for (size_t i = 0; i < kThreadNum - 1; ++i) {
      threads.emplace_back(func_multi, i);
    }
    threads.emplace_back(func_single, kThreadNum - 1);
    std::this_thread::sleep_for(std::chrono::milliseconds{kWaitForThreadCreation});
    std::lock_guard guard{s_mtx_};

    is_ready_ = true;
    cond_.notify_all();

    for (auto &&t : threads) {
      t.join();
    }
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  // NOTE: testing SnapshotRead requires concurrent Write Operation
  void VerifySnapshotRead(  //
  )
  {
    VerifyWrite(!kWriteTwice, kSequential);
    epoch_manager_->ForwardGlobalEpoch();
    const auto &[epoch_guard, protected_epochs] = epoch_manager_->GetProtectedEpochs();

    auto func_snapshot_read = [&]([[maybe_unused]] size_t _) -> void {
      const auto &target_ids = CreateTargetIDs(kExecNum);
      for (size_t i = kThreadNum /*Somehow, CreateTargetIDs(w_id,pattern) starts from 8*/;
           i < target_ids.size(); ++i) {
        const auto &key = keys_.at(i);
        const auto read_val =
            index_->SnapshotRead(key, epoch_guard, protected_epochs, GetLength(key));

        const auto expected_val = payloads_.at(i % kThreadNum);
        const auto actual_val = read_val.value();
        EXPECT_TRUE(IsEqual<PayComp>(expected_val, actual_val));
      }
    };
    std::function<void(size_t)> func_write = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, kSequential)) {
        const auto rc = Write(id, w_id + kThreadNum);
        EXPECT_EQ(rc, 0);
      }
    };
    RunMTMultiOperation(func_snapshot_read, func_write);
  }

  void
  VerifyRead(  //
      const bool expect_success,
      const bool is_update,
      const AccessPattern pattern)
  {
    auto mt_worker = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, pattern)) {
        const auto &key = keys_.at(id);
        const auto &read_val = index_->Read(key, GetLength(key));
        if (expect_success) {
          ASSERT_TRUE(read_val);
          const auto expected_val = payloads_.at((is_update) ? w_id + kThreadNum : w_id);
          const auto actual_val = read_val.value();
          EXPECT_TRUE(IsEqual<PayComp>(expected_val, actual_val));
        } else {
          EXPECT_FALSE(read_val);
        }
      }
    };

    RunMT(mt_worker);
  }

  void
  VerifyScan(  //
      [[maybe_unused]] const bool expect_success,
      [[maybe_unused]] const bool is_update)
  {
    epoch_manager_->ForwardGlobalEpoch();
    const auto &[epoch_guard, protected_epochs] = epoch_manager_->GetProtectedEpochs();

    if constexpr (HasScanOperation<ImplStat>()) {
      auto mt_worker = [&](const size_t w_id) -> void {
        size_t begin_id = kThreadNum + kExecNum * w_id;
        const auto &begin_k = keys_.at(begin_id);
        const auto &begin_key = std::make_tuple(begin_k, GetLength(begin_k), kRangeClosed);

        size_t end_id = kExecNum * (w_id + 1);
        const auto &end_k = keys_.at(end_id);
        const auto &end_key = std::make_tuple(end_k, GetLength(end_k), kRangeOpened);

        auto &&iter = index_->Scan(epoch_guard, protected_epochs, begin_key, end_key);
        if (expect_success) {
          for (; iter; ++iter, ++begin_id) {
            const auto key_id = begin_id;
            const auto val_id =
                (is_update) ? key_id % kThreadNum + kThreadNum : key_id % kThreadNum;

            const auto &[key, payload] = *iter;
            EXPECT_TRUE(IsEqual<KeyComp>(keys_.at(key_id), key));
            EXPECT_TRUE(IsEqual<PayComp>(payloads_.at(val_id), payload));
          }
          EXPECT_EQ(begin_id, end_id);
        }
        EXPECT_FALSE(iter);
      };

      RunMT(mt_worker);
    }
  }

  void
  VerifySnapshotScanWith(  //
      const WriteOperation write_ops,
      const AccessPattern pattern)
  {
    VerifyWrite(!kWriteTwice, kSequential);
    epoch_manager_->ForwardGlobalEpoch();

    const auto &[epoch_guard, protected_epochs] = epoch_manager_->GetProtectedEpochs();
    epoch_manager_->ForwardGlobalEpoch();
    epoch_manager_->ForwardGlobalEpoch();
    // Note: GetProtectedEpochs() returns the current epoch E, E-1, and protected epochs in
    // descending order. Forwarding epoch 2 times makes sure that tail of the list is the oldest
    // protected epoch.
    auto func_full_scan_op = [&]([[maybe_unused]] const size_t _) -> void {
      size_t begin_id = kThreadNum + kExecNum * 0;
      const auto &begin_k = keys_.at(begin_id);
      const auto &begin_key = std::make_tuple(begin_k, GetLength(begin_k), kRangeClosed);

      size_t end_id = kExecNum * kThreadNum;
      const auto &end_k = keys_.at(end_id);
      const auto &end_key = std::make_tuple(end_k, GetLength(end_k), kRangeOpened);

      auto &&iter = index_->Scan(epoch_guard, protected_epochs, begin_key, end_key);

      for (; iter; ++iter, ++begin_id) {
        const auto key_id = begin_id;
        const auto val_id = key_id % kThreadNum;

        const auto &[key, payload] = *iter;
        EXPECT_TRUE(IsEqual<KeyComp>(keys_.at(key_id), key));
        EXPECT_TRUE(IsEqual<PayComp>(payloads_.at(val_id), payload));
      }
      EXPECT_EQ(begin_id, end_id);
    };

    std::function<void(size_t)> func_write_op;
    switch (write_ops) {
      case kWrite:
        func_write_op = [&](const size_t w_id) -> void {
          for (const auto id : CreateTargetIDs(w_id, pattern)) {
            const auto rc = Write(id, w_id + kThreadNum);
            EXPECT_EQ(rc, 0);
          }
        };
        break;
      case kInsert:
        assert(false);  // Insert is an invalid operation for testing versioned scan
        break;
      case kUpdate:
        func_write_op = [&](const size_t w_id) -> void {
          for (const auto id : CreateTargetIDs(w_id, pattern)) {
            const auto rc = Update(id, w_id + kThreadNum);
            EXPECT_EQ(rc, 0);
          }
        };
        break;
      case kDelete:
        func_write_op = [&](const size_t w_id) -> void {
          for (const auto id : CreateTargetIDs(w_id, pattern)) {
            const auto rc = Delete(id);
            EXPECT_EQ(rc, 0);
          }
        };
        break;
      case kWithoutWrite:
      default:
        break;
    }

    RunMTMultiOperation(func_full_scan_op, func_write_op);
  }

  void
  VerifyWrite(  //
      const bool is_update,
      const AccessPattern pattern)
  {
    auto mt_worker = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, pattern)) {
        const auto rc = Write(id, (is_update) ? w_id + kThreadNum : w_id);
        EXPECT_EQ(rc, 0);
      }
    };

    RunMT(mt_worker);
  }

  void
  VerifyInsert(  //
      const bool expect_success,
      const bool is_update,
      const AccessPattern pattern)
  {
    auto mt_worker = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, pattern)) {
        const auto rc = Insert(id, (is_update) ? w_id + kThreadNum : w_id);
        if (expect_success) {
          EXPECT_EQ(rc, 0);
        } else {
          EXPECT_NE(rc, 0);
        }
      }
    };

    RunMT(mt_worker);
  }

  void
  VerifyUpdate(  //
      const bool expect_success,
      const AccessPattern pattern)
  {
    auto mt_worker = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, pattern)) {
        const auto rc = Update(id, w_id + kThreadNum);
        if (expect_success) {
          EXPECT_EQ(rc, 0);
        } else {
          EXPECT_NE(rc, 0);
        }
      }
    };

    RunMT(mt_worker);
  }

  void
  VerifyDelete(  //
      const bool expect_success,
      const AccessPattern pattern)
  {
    auto mt_worker = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, pattern)) {
        const auto rc = Delete(id);
        if (expect_success) {
          EXPECT_EQ(rc, 0);
        } else {
          EXPECT_NE(rc, 0);
        }
      }
    };

    RunMT(mt_worker);
  }

  void
  VerifyBulkload()
  {
    if constexpr (HasBulkloadOperation<ImplStat>()) {
      constexpr size_t kOpsNum = (kExecNum + 1) * kThreadNum;
      if constexpr (IsVarLen<Key>() || IsVarLen<Payload>()) {
        std::vector<std::tuple<Key, Payload, size_t, size_t>> entries{};
        entries.reserve(kOpsNum);
        for (size_t i = kThreadNum; i < kOpsNum; ++i) {
          const auto &key = keys_.at(i);
          const auto &payload = payloads_.at(i % kThreadNum);
          entries.emplace_back(key, payload, GetLength(key), GetLength(payload));
        }

        const auto rc = index_->Bulkload(entries, kThreadNum);
        EXPECT_EQ(rc, 0);
      } else {
        std::vector<std::pair<Key, Payload>> entries{};
        entries.reserve(kOpsNum);
        for (size_t i = kThreadNum; i < kOpsNum; ++i) {
          entries.emplace_back(keys_.at(i), payloads_.at(i % kThreadNum));
        }

        const auto rc = index_->Bulkload(entries, kThreadNum);
        EXPECT_EQ(rc, 0);
      }
    }
  }

  /*####################################################################################
   * Functions for test definitions
   *##################################################################################*/

  void
  VerifyWritesWith(  //
      const bool write_twice,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasWriteOperation<ImplStat>()                        //
        || (with_delete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    VerifyWrite(!kWriteTwice, pattern);
    if (with_delete) VerifyDelete(kExpectSuccess, pattern);
    if (write_twice) VerifyWrite(kWriteTwice, pattern);
    VerifyRead(kExpectSuccess, write_twice, pattern);
    VerifyScan(kExpectSuccess, write_twice);

    ReleaseTestData(keys_);
    ReleaseTestData(payloads_);
  }

  void
  VerifyInsertsWith(  //
      const bool write_twice,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasInsertOperation<ImplStat>()                       //
        || (with_delete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto expect_success = !with_delete || write_twice;
    const auto is_updated = with_delete && write_twice;

    VerifyInsert(kExpectSuccess, !kWriteTwice, pattern);
    if (with_delete) VerifyDelete(kExpectSuccess, pattern);
    if (write_twice) VerifyInsert(with_delete, write_twice, pattern);
    VerifyRead(expect_success, is_updated, pattern);
    VerifyScan(expect_success, is_updated);

    DestroyData();
  }

  void
  VerifyUpdatesWith(  //
      const bool with_write,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasUpdateOperation<ImplStat>()  //
        || (with_write && !HasWriteOperation<ImplStat>())
        || (with_delete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto expect_success = with_write && !with_delete;

    if (with_write) VerifyWrite(!kWriteTwice, pattern);
    if (with_delete) VerifyDelete(with_write, pattern);
    VerifyUpdate(expect_success, pattern);
    VerifyRead(expect_success, kWriteTwice, pattern);
    VerifyScan(expect_success, kWriteTwice);

    DestroyData();
  }

  void
  VerifyDeletesWith(  //
      const bool with_write,
      const bool with_delete,
      const AccessPattern pattern)
  {
    if (!HasDeleteOperation<ImplStat>()                     //
        || (with_write && !HasWriteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    const auto expect_success = with_write && !with_delete;

    if (with_write) VerifyWrite(!kWriteTwice, pattern);
    if (with_delete) VerifyDelete(with_write, pattern);
    VerifyDelete(expect_success, pattern);
    VerifyRead(kExpectFailed, !kWriteTwice, pattern);
    VerifyScan(kExpectFailed, !kWriteTwice);

    DestroyData();
  }

  void
  VerifyConcurrentSMOs()
  {
    constexpr size_t kRepeatNum = 5;
    constexpr size_t kReadThread = kThreadNum / 2;
    constexpr size_t kScanThread = kThreadNum * 3 / 4;
    std::atomic_size_t counter{};

    if (!HasWriteOperation<ImplStat>()      //
        || !HasDeleteOperation<ImplStat>()  //
        || !HasScanOperation<ImplStat>()    //
        || (kThreadNum % 4) != 0)           //
    {
      GTEST_SKIP();
    }

    auto read_proc = [&]() -> void {
      for (const auto id : CreateTargetIDsForConcurrentSMOs()) {
        const auto &key = keys_.at(id);
        const auto &read_val = index_->Read(key, GetLength(key));
        if (read_val) {
          EXPECT_TRUE(IsEqual<PayComp>(payloads_.at(id % kReadThread), read_val.value()));
        }
      }
    };

    auto scan_proc = [&]() -> void {
      epoch_manager_->ForwardGlobalEpoch();
      auto &&guard = epoch_manager_->CreateEpochGuard();

      Key prev_key{};
      if constexpr (IsVarLen<Key>()) {
        prev_key = reinterpret_cast<Key>(::operator new(kVarDataLength));
      }
      while (counter < kReadThread) {
        if constexpr (IsVarLen<Key>()) {
          memcpy(prev_key, keys_.at(0), GetLength<Key>(keys_.at(0)));
        } else {
          prev_key = keys_.at(0);
        }
        for (auto &&iter = index_->Scan(guard); iter; ++iter) {
          const auto &[key, payload] = *iter;
          EXPECT_TRUE(KeyComp{}(prev_key, key));
          if constexpr (IsVarLen<Key>()) {
            memcpy(prev_key, key, GetLength<Key>(key));
          } else {
            prev_key = key;
          }
        }
      }
      if constexpr (IsVarLen<Key>()) {
        ::operator delete(prev_key);
      }
    };

    auto write_proc = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, kRandom)) {
        EXPECT_EQ(Write(id, w_id), 0);
      }
      counter += 1;
    };

    auto delete_proc = [&](const size_t w_id) -> void {
      for (const auto id : CreateTargetIDs(w_id, kRandom)) {
        EXPECT_EQ(Delete(id), 0);
      }
      counter += 1;
    };

    auto init_worker = [&](const size_t w_id) -> void {
      if (w_id < kReadThread && (w_id % 2) == 0) {
        write_proc(w_id);
      }
    };

    auto even_delete_worker = [&](const size_t w_id) -> void {
      if (w_id >= kScanThread) {
        scan_proc();
      } else if (w_id >= kReadThread) {
        read_proc();
      } else if (w_id % 2 == 0) {
        delete_proc(w_id);
      } else {
        write_proc(w_id);
      }
    };

    auto odd_delete_worker = [&](const size_t w_id) -> void {
      if (w_id >= kScanThread) {
        scan_proc();
      } else if (w_id >= kReadThread) {
        read_proc();
      } else if (w_id % 2 == 0) {
        write_proc(w_id);
      } else {
        delete_proc(w_id);
      }
    };

    PrepareData();
    RunMT(init_worker);
    for (size_t i = 0; i < kRepeatNum; ++i) {
      counter = 0;
      RunMT(even_delete_worker);
      counter = 0;
      RunMT(odd_delete_worker);
    }
    DestroyData();
  }

  void
  VerifyBulkloadWith(  //
      const WriteOperation write_ops,
      const AccessPattern pattern)
  {
    if (!HasBulkloadOperation<ImplStat>()                              //
        || (write_ops == kWrite && !HasWriteOperation<ImplStat>())     //
        || (write_ops == kInsert && !HasInsertOperation<ImplStat>())   //
        || (write_ops == kUpdate && !HasUpdateOperation<ImplStat>())   //
        || (write_ops == kDelete && !HasDeleteOperation<ImplStat>()))  //
    {
      GTEST_SKIP();
    }

    PrepareData();

    auto expect_success = true;
    auto is_updated = false;

    VerifyBulkload();
    switch (write_ops) {
      case kWrite:
        VerifyWrite(kWriteTwice, pattern);
        is_updated = true;
        break;
      case kInsert:
        VerifyInsert(kExpectFailed, kWriteTwice, pattern);
        break;
      case kUpdate:
        VerifyUpdate(kExpectSuccess, pattern);
        is_updated = true;
        break;
      case kDelete:
        VerifyDelete(kExpectSuccess, pattern);
        expect_success = false;
        break;
      case kWithoutWrite:
      default:
        break;
    }
    VerifyRead(expect_success, is_updated, pattern);
    VerifyScan(expect_success, is_updated);

    DestroyData();
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// actual keys
  std::vector<Key> keys_{};

  /// actual payloads
  std::vector<Payload> payloads_{};

  /// an index for testing
  std::unique_ptr<Index_t> index_{nullptr};

  /// a mutex for notifying worker threads.
  std::mutex x_mtx_{};

  /// a shared mutex for blocking main process.
  std::shared_mutex s_mtx_{};

  /// a flag for indicating ready.
  bool is_ready_{false};

  /// a condition variable for notifying worker threads.
  std::condition_variable cond_{};

  // an epoch manager for multi-version
  std::shared_ptr<EpochManager> epoch_manager_{nullptr};
};

}  // namespace dbgroup::index::test

#endif  // INDEX_FIXTURES_INDEX_FIXTURE_MULTI_THREAD_HPP
